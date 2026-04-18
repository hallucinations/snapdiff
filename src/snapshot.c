#include "snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

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

static void walk(Snapshot *snap, const char *base, const char *rel)
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
            walk(snap, base, child_rel);
        } else if (S_ISREG(st.st_mode)) {
            FileEntry *e = entry_new();
            snprintf(e->path, MAX_PATH, "%s", child_rel);
            e->size  = st.st_size;
            e->mode  = st.st_mode & 07777;
            e->mtime = st.st_mtime;
            hash_file(child_full, e->hash);
            snapshot_append(snap, e);
        }
    }
    closedir(d);
}

Snapshot *snapshot_create(const char *dir)
{
    Snapshot *snap = calloc(1, sizeof(*snap));
    if (!snap) { perror("calloc"); exit(1); }

    strncpy(snap->root, dir, MAX_PATH - 1);
    snap->created = time(NULL);
    walk(snap, dir, "");
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
        long long size; unsigned mode; long mtime;
        int n = sscanf(line, "%4095s\t%lld\t%o\t%ld\t%64s",
                       e->path, &size, &mode, &mtime, e->hash);
        if (n != 5) { free(e); continue; }
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
