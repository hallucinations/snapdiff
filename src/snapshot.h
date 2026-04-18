#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "hash.h"
#include <sys/stat.h>
#include <time.h>

#define MAX_PATH 4096

typedef struct FileEntry {
    char             path[MAX_PATH];
    off_t            size;
    mode_t           mode;
    time_t           mtime;
    char             hash[HASH_HEX_LEN];
    struct FileEntry *next;
} FileEntry;

typedef struct {
    char       root[MAX_PATH];
    time_t     created;
    FileEntry *head;
    int        count;
} Snapshot;

Snapshot *snapshot_create(const char *dir);
Snapshot *snapshot_load(const char *snapfile);
int       snapshot_save(const Snapshot *snap, const char *snapfile);
void      snapshot_free(Snapshot *snap);

#endif
