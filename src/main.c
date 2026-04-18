#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "snapshot.h"
#include "diff.h"

static void usage(const char *prog)
{
    fprintf(stderr,
        "snapdiff — directory snapshot and diff tool\n"
        "\n"
        "Usage:\n"
        "  %s snap <dir> [-o <output.snap>]\n"
        "      Capture a snapshot of <dir> and save it.\n"
        "      Default output: <dir-basename>.snap\n"
        "\n"
        "  %s diff <snapshot.snap> <dir>\n"
        "      Compare <dir> against a previously taken snapshot.\n"
        "\n"
        "  %s info <snapshot.snap>\n"
        "      Display metadata about a snapshot file.\n"
        "\n"
        "Examples:\n"
        "  snapdiff snap /etc -o before.snap\n"
        "  snapdiff diff before.snap /etc\n"
        "\n",
        prog, prog, prog);
}

static int cmd_snap(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "snap: missing <dir>\n");
        return 1;
    }

    const char *dir    = argv[0];
    const char *outfile = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i+1 < argc)
            outfile = argv[++i];
        else {
            fprintf(stderr, "snap: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    char default_out[4096];
    if (!outfile) {
        char tmp[4096];
        strncpy(tmp, dir, sizeof(tmp) - 1);
        size_t len = strlen(tmp);
        while (len > 1 && tmp[len - 1] == '/') tmp[--len] = '\0';
        char *base = strrchr(tmp, '/');
        base = base ? base + 1 : tmp;
        snprintf(default_out, sizeof(default_out), "%s.snap", base);
        outfile = default_out;
    }

    fprintf(stderr, "Scanning %s ...\n", dir);
    Snapshot *snap = snapshot_create(dir, NULL);
    fprintf(stderr, "  %d files indexed\n", snap->count);

    if (snapshot_save(snap, outfile) < 0) {
        snapshot_free(snap);
        return 1;
    }

    fprintf(stderr, "Snapshot saved to: %s\n", outfile);
    snapshot_free(snap);
    return 0;
}

static int cmd_diff(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "diff: usage: snapdiff diff <snapshot> <dir>\n");
        return 1;
    }

    const char *snapfile = argv[0];
    const char *dir      = argv[1];

    fprintf(stderr, "Loading snapshot: %s\n", snapfile);
    Snapshot *old_snap = snapshot_load(snapfile);
    if (!old_snap) return 1;

    fprintf(stderr, "Scanning %s ...\n", dir);
    Snapshot *new_snap = snapshot_create(dir, old_snap);

    char tstr[64];
    struct tm *tm = localtime(&old_snap->created);
    strftime(tstr, sizeof(tstr), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(stderr, "Snapshot taken: %s (%d files)\n", tstr, old_snap->count);
    fprintf(stderr, "Current state:  %d files\n\n", new_snap->count);

    int diffs = diff_snapshots(old_snap, new_snap);

    if (diffs == 0)
        printf("No changes detected.\n");
    else
        printf("\n%d change(s) found.\n", diffs);

    snapshot_free(old_snap);
    snapshot_free(new_snap);
    return diffs > 0 ? 2 : 0;
}

static int cmd_info(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "info: missing <snapshot>\n");
        return 1;
    }

    Snapshot *snap = snapshot_load(argv[0]);
    if (!snap) return 1;

    char tstr[64];
    struct tm *tm = localtime(&snap->created);
    strftime(tstr, sizeof(tstr), "%Y-%m-%d %H:%M:%S", tm);

    long long total_size = 0;
    for (const FileEntry *e = snap->head; e; e = e->next)
        total_size += (long long)e->size;

    printf("Snapshot file : %s\n", argv[0]);
    printf("Root directory: %s\n", snap->root);
    printf("Created       : %s\n", tstr);
    printf("Files tracked : %d\n", snap->count);
    printf("Total size    : %lld bytes\n", total_size);

    snapshot_free(snap);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *cmd = argv[1];

    if (!strcmp(cmd, "snap"))
        return cmd_snap(argc - 2, argv + 2);
    if (!strcmp(cmd, "diff"))
        return cmd_diff(argc - 2, argv + 2);
    if (!strcmp(cmd, "info"))
        return cmd_info(argc - 2, argv + 2);
    if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
        usage(argv[0]); return 0;
    }

    fprintf(stderr, "Unknown command: %s\n\n", cmd);
    usage(argv[0]);
    return 1;
}
