#include "snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

/* ---- hint table (mtime/size fast-path) ------------------------- */

#define HINT_SIZE 65536u

typedef struct HintNode { const FileEntry *entry; struct HintNode *next; } HintNode;
typedef struct { HintNode *buckets[HINT_SIZE]; } HintTable;

static unsigned hint_hash(const char *s)
{
    unsigned h = 5381;
    while (*s) h = h * 33 ^ (unsigned char)*s++;
    return h & (HINT_SIZE - 1);
}

static HintTable *hint_build(const Snapshot *snap)
{
    HintTable *ht = calloc(1, sizeof(*ht));
    if (!ht) return NULL;
    for (const FileEntry *e = snap->head; e; e = e->next) {
        unsigned slot = hint_hash(e->path);
        HintNode *n = malloc(sizeof(*n));
        if (!n) continue;
        n->entry = e;
        n->next  = ht->buckets[slot];
        ht->buckets[slot] = n;
    }
    return ht;
}

static const FileEntry *hint_find(const HintTable *ht, const char *path)
{
    for (HintNode *n = ht->buckets[hint_hash(path)]; n; n = n->next)
        if (!strcmp(n->entry->path, path)) return n->entry;
    return NULL;
}

static void hint_free(HintTable *ht)
{
    for (unsigned i = 0; i < HINT_SIZE; i++) {
        HintNode *n = ht->buckets[i];
        while (n) { HintNode *nx = n->next; free(n); n = nx; }
    }
    free(ht);
}

/* ---- parallel hashing ------------------------------------------ */

#define MAX_HASH_THREADS 16

typedef struct {
    FileEntry  **entries;
    int          count;
    atomic_int   next;
    const char  *root;
} HashWork;

static void *hash_worker(void *arg)
{
    HashWork *w = arg;
    int i;
    while ((i = atomic_fetch_add_explicit(&w->next, 1, memory_order_relaxed)) < w->count) {
        FileEntry *e = w->entries[i];
        char full[MAX_PATH * 2];
        snprintf(full, sizeof(full), "%s/%s", w->root, e->path);
        hash_file(full, e->hash);
    }
    return NULL;
}

/* ---- internal helpers ------------------------------------------ */

static FileEntry *entry_new(void)
{
    FileEntry *e = calloc(1, sizeof(*e));
    if (!e) { perror("calloc"); exit(1); }
    return e;
}

static void snapshot_append(Snapshot *snap, FileEntry *e)
{
    e->next = snap->head;
    snap->head = e;
    snap->count++;
}

/* ---- directory walk -------------------------------------------- */

static void walk(Snapshot *snap, const char *base, const char *rel,
                 const HintTable *hint)
{
    char full[MAX_PATH];
    if (*rel)
        snprintf(full, sizeof(full), "%s/%s", base, rel);
    else
        snprintf(full, sizeof(full), "%s", base);

    DIR *d = opendir(full);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;

        char child_rel[MAX_PATH];
        if (*rel)
            snprintf(child_rel, sizeof(child_rel), "%s/%s", rel, de->d_name);
        else
            snprintf(child_rel, sizeof(child_rel), "%s", de->d_name);

        char child_full[MAX_PATH * 2];
        snprintf(child_full, sizeof(child_full), "%s/%s", full, de->d_name);

        struct stat st;
        if (lstat(child_full, &st) < 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walk(snap, base, child_rel, hint);
        } else if (S_ISREG(st.st_mode)) {
            FileEntry *e = entry_new();
            snprintf(e->path, MAX_PATH, "%s", child_rel);
            e->size  = st.st_size;
            e->mode  = st.st_mode & 07777;
            e->mtime = st.st_mtime;

            if (hint) {
                const FileEntry *h = hint_find(hint, child_rel);
                if (h && h->mtime == e->mtime && h->size == e->size)
                    memcpy(e->hash, h->hash, HASH_HEX_LEN);
                /* else hash[0] stays '\0': picked up by parallel phase */
            }
            snapshot_append(snap, e);
        }
    }
    closedir(d);
}

/* ---- public API ------------------------------------------------- */

Snapshot *snapshot_create(const char *dir, const Snapshot *hint)
{
    Snapshot *snap = calloc(1, sizeof(*snap));
    if (!snap) { perror("calloc"); exit(1); }
    strncpy(snap->root, dir, MAX_PATH - 1);
    snap->created = time(NULL);

    HintTable *ht = hint ? hint_build(hint) : NULL;
    walk(snap, dir, "", ht);
    if (ht) hint_free(ht);

    /* collect entries still needing a hash */
    FileEntry **work = NULL;
    int work_n = 0;
    if (snap->count > 0) {
        work = malloc((size_t)snap->count * sizeof(*work));
        if (!work) { perror("malloc"); exit(1); }
        for (FileEntry *e = snap->head; e; e = e->next)
            if (e->hash[0] == '\0')
                work[work_n++] = e;
    }

    if (work_n > 0) {
        int ncpu = (int)sysconf(_SC_NPROCESSORS_ONLN);
        int nt   = ncpu < 1 ? 1 : ncpu > MAX_HASH_THREADS ? MAX_HASH_THREADS : ncpu;
        if (nt > work_n) nt = work_n;

        HashWork hw;
        hw.entries = work;
        hw.count   = work_n;
        atomic_init(&hw.next, 0);
        hw.root    = dir;

        pthread_t tids[MAX_HASH_THREADS];
        for (int i = 0; i < nt; i++)
            pthread_create(&tids[i], NULL, hash_worker, &hw);
        for (int i = 0; i < nt; i++)
            pthread_join(tids[i], NULL);
    }

    free(work);
    return snap;
}

int snapshot_save(const Snapshot *snap, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) { perror(path); return -1; }

    fprintf(fp, "# snapdiff v1\n");
    fprintf(fp, "# root: %s\n", snap->root);
    fprintf(fp, "# created: %ld\n", (long)snap->created);
    fprintf(fp, "# format: path\\tsize\\tmode\\tmtime\\thash\n");

    for (FileEntry *e = snap->head; e; e = e->next)
        fprintf(fp, "%s\t%lld\t%o\t%ld\t%s\n",
                e->path, (long long)e->size, (unsigned)e->mode,
                (long)e->mtime, e->hash);

    fclose(fp);
    return 0;
}

Snapshot *snapshot_load(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return NULL; }

    Snapshot *snap = calloc(1, sizeof(*snap));
    if (!snap) { perror("calloc"); exit(1); }

    char line[MAX_PATH + 256];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';

        if (!strncmp(line, "# root: ", 8)) {
            sscanf(line + 8, "%4095s", snap->root);
            continue;
        }
        if (!strncmp(line, "# created: ", 11)) {
            snap->created = (time_t)atol(line + 11);
            continue;
        }
        if (line[0] == '#' || line[0] == '\0') continue;

        FileEntry *e = entry_new();
        char *tab = strchr(line, '\t');
        if (!tab) { free(e); continue; }
        size_t pathlen = (size_t)(tab - line);
        if (pathlen >= MAX_PATH) { free(e); continue; }
        memcpy(e->path, line, pathlen);
        e->path[pathlen] = '\0';

        long long size; unsigned mode; long mtime;
        int n = sscanf(tab + 1, "%lld\t%o\t%ld\t%64s",
                       &size, &mode, &mtime, e->hash);
        if (n != 4) { free(e); continue; }
        e->size  = (off_t)size;
        e->mode  = (mode_t)mode;
        e->mtime = (time_t)mtime;
        snapshot_append(snap, e);
    }
    fclose(fp);
    return snap;
}

void snapshot_free(Snapshot *snap)
{
    if (!snap) return;
    FileEntry *e = snap->head;
    while (e) {
        FileEntry *next = e->next;
        free(e);
        e = next;
    }
    free(snap);
}
