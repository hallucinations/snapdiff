#include "diff.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[0;33m"
#define COLOR_CYAN    "\033[0;36m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RESET   "\033[0m"

#define HT_SIZE 4096

typedef struct HNode {
    FileEntry   *entry;
    struct HNode *next;
} HNode;

typedef struct { HNode *buckets[HT_SIZE]; } HTable;

static unsigned ht_hash(const char *s)
{
    unsigned h = 5381;
    while (*s) h = h * 33 ^ (unsigned char)*s++;
    return h % HT_SIZE;
}

static HTable *ht_build(const Snapshot *snap)
{
    HTable *ht = calloc(1, sizeof(*ht));
    if (!ht) { perror("calloc"); exit(1); }
    for (FileEntry *e = snap->head; e; e = e->next) {
        unsigned slot = ht_hash(e->path);
        HNode *n = malloc(sizeof(*n));
        if (!n) { perror("malloc"); exit(1); }
        *n = (HNode){ .entry = e, .next = ht->buckets[slot] };
        ht->buckets[slot] = n;
    }
    return ht;
}

static FileEntry *ht_find(const HTable *ht, const char *path)
{
    unsigned slot = ht_hash(path);
    for (HNode *n = ht->buckets[slot]; n; n = n->next)
        if (!strcmp(n->entry->path, path)) return n->entry;
    return NULL;
}

static void ht_free(HTable *ht)
{
    for (int i = 0; i < HT_SIZE; i++) {
        HNode *n = ht->buckets[i];
        while (n) { HNode *next = n->next; free(n); n = next; }
    }
    free(ht);
}

static void fmt_size(long long bytes, char *buf, size_t buflen)
{
    const char *units[] = {"B","KB","MB","GB","TB"};
    int u = 0;
    double v = (double)bytes;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    if (u == 0) snprintf(buf, buflen, "%lld B", bytes);
    else        snprintf(buf, buflen, "%.1f %s", v, units[u]);
}

static void fmt_mode(mode_t m, char out[5])
{
    snprintf(out, 5, "%04o", (unsigned)(m & 07777));
}

int diff_snapshots(const Snapshot *old_snap, const Snapshot *new_snap)
{
    int total = 0;

    HTable *old_ht = ht_build(old_snap);
    HTable *new_ht = ht_build(new_snap);

    for (FileEntry *e = old_snap->head; e; e = e->next) {
        if (!ht_find(new_ht, e->path)) {
            printf(COLOR_RED COLOR_BOLD "[-] REMOVED " COLOR_RESET
                   COLOR_RED "%s" COLOR_RESET "\n", e->path);
            total++;
        }
    }

    for (FileEntry *ne = new_snap->head; ne; ne = ne->next) {
        FileEntry *oe = ht_find(old_ht, ne->path);

        if (!oe) {
            char szstr[32];
            fmt_size(ne->size, szstr, sizeof(szstr));
            printf(COLOR_GREEN COLOR_BOLD "[+] ADDED   " COLOR_RESET
                   COLOR_GREEN "%s" COLOR_RESET " (%s)\n", ne->path, szstr);
            total++;
            continue;
        }

        int content_changed = strcmp(ne->hash, oe->hash) != 0;
        int mode_changed    = ne->mode != oe->mode;

        if (content_changed) {
            char old_sz[32], new_sz[32];
            fmt_size(oe->size, old_sz, sizeof(old_sz));
            fmt_size(ne->size, new_sz, sizeof(new_sz));

            printf(COLOR_YELLOW COLOR_BOLD "[~] MODIFIED" COLOR_RESET
                   COLOR_YELLOW " %s" COLOR_RESET "\n", ne->path);
            printf("    size:  %s → %s\n", old_sz, new_sz);
            printf("    hash:  %.16s… → %.16s…\n", oe->hash, ne->hash);

            if (mode_changed) {
                char om[5], nm[5];
                fmt_mode(oe->mode, om); fmt_mode(ne->mode, nm);
                printf("    mode:  %s → %s\n", om, nm);
            }
            total++;
        } else if (mode_changed) {
            char om[5], nm[5];
            fmt_mode(oe->mode, om); fmt_mode(ne->mode, nm);
            printf(COLOR_CYAN COLOR_BOLD "[M] CHMOD   " COLOR_RESET
                   COLOR_CYAN "%s" COLOR_RESET " (%s → %s)\n",
                   ne->path, om, nm);
            total++;
        }
    }

    ht_free(old_ht);
    ht_free(new_ht);

    return total;
}
