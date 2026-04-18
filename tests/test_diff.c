#include "testutil.h"
#include "../src/diff.h"
#include "../src/snapshot.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static FileEntry *make_entry(const char *path, const char *hash,
                             off_t size, mode_t mode)
{
    FileEntry *e = calloc(1, sizeof(*e));
    e->path = strdup(path);
    snprintf(e->hash, HASH_HEX_LEN, "%s", hash);
    e->size = size;
    e->mode = mode;
    return e;
}

static Snapshot *make_snap(FileEntry *entries[], int n)
{
    Snapshot *s = calloc(1, sizeof(*s));
    snprintf(s->root, MAX_PATH, "/tmp/test");
    s->created = 1000000;
    for (int i = n - 1; i >= 0; i--) {
        entries[i]->next = s->head;
        s->head = entries[i];
        s->count++;
    }
    return s;
}

static void free_snap(Snapshot *s)
{
    FileEntry *e = s->head;
    while (e) { FileEntry *nx = e->next; free(e->path); free(e); e = nx; }
    free(s);
}

#define HASH_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define HASH_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define HASH_C "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
#define HASH_D "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"

static void test_no_changes(void)
{
    FileEntry *ea[] = { make_entry("foo.txt", HASH_A, 10, 0644) };
    FileEntry *eb[] = { make_entry("foo.txt", HASH_A, 10, 0644) };
    Snapshot *old = make_snap(ea, 1);
    Snapshot *neo = make_snap(eb, 1);
    ASSERT_EQ(diff_snapshots(old, neo), 0);
    free_snap(old); free_snap(neo);
}

static void test_both_empty(void)
{
    Snapshot *old = calloc(1, sizeof(*old));
    Snapshot *neo = calloc(1, sizeof(*neo));
    ASSERT_EQ(diff_snapshots(old, neo), 0);
    free(old); free(neo);
}

static void test_old_empty_new_has_files(void)
{
    FileEntry *eb[] = {
        make_entry("a.txt", HASH_A, 1, 0644),
        make_entry("b.txt", HASH_B, 2, 0644),
    };
    Snapshot *old = calloc(1, sizeof(*old));
    Snapshot *neo = make_snap(eb, 2);
    ASSERT_EQ(diff_snapshots(old, neo), 2);
    free(old); free_snap(neo);
}

static void test_new_empty_old_has_files(void)
{
    FileEntry *ea[] = {
        make_entry("a.txt", HASH_A, 1, 0644),
        make_entry("b.txt", HASH_B, 2, 0644),
    };
    Snapshot *old = make_snap(ea, 2);
    Snapshot *neo = calloc(1, sizeof(*neo));
    ASSERT_EQ(diff_snapshots(old, neo), 2);
    free_snap(old); free(neo);
}

static void test_added_file(void)
{
    FileEntry *ea[] = { make_entry("a.txt", HASH_A, 5, 0644) };
    FileEntry *eb[] = { make_entry("a.txt", HASH_A, 5, 0644),
                        make_entry("b.txt", HASH_B, 7, 0644) };
    Snapshot *old = make_snap(ea, 1);
    Snapshot *neo = make_snap(eb, 2);
    ASSERT_EQ(diff_snapshots(old, neo), 1);
    free_snap(old); free_snap(neo);
}

static void test_removed_file(void)
{
    FileEntry *ea[] = { make_entry("a.txt", HASH_A, 5, 0644),
                        make_entry("b.txt", HASH_B, 5, 0644) };
    FileEntry *eb[] = { make_entry("a.txt", HASH_A, 5, 0644) };
    Snapshot *old = make_snap(ea, 2);
    Snapshot *neo = make_snap(eb, 1);
    ASSERT_EQ(diff_snapshots(old, neo), 1);
    free_snap(old); free_snap(neo);
}

static void test_modified_content(void)
{
    FileEntry *ea[] = { make_entry("f.txt", HASH_A, 5, 0644) };
    FileEntry *eb[] = { make_entry("f.txt", HASH_B, 9, 0644) };
    Snapshot *old = make_snap(ea, 1);
    Snapshot *neo = make_snap(eb, 1);
    ASSERT_EQ(diff_snapshots(old, neo), 1);
    free_snap(old); free_snap(neo);
}

static void test_chmod_only(void)
{
    FileEntry *ea[] = { make_entry("f.txt", HASH_A, 5, 0644) };
    FileEntry *eb[] = { make_entry("f.txt", HASH_A, 5, 0755) };
    Snapshot *old = make_snap(ea, 1);
    Snapshot *neo = make_snap(eb, 1);
    ASSERT_EQ(diff_snapshots(old, neo), 1);
    free_snap(old); free_snap(neo);
}

static void test_content_and_mode_both_changed(void)
{
    FileEntry *ea[] = { make_entry("f.txt", HASH_A, 5, 0644) };
    FileEntry *eb[] = { make_entry("f.txt", HASH_B, 8, 0755) };
    Snapshot *old = make_snap(ea, 1);
    Snapshot *neo = make_snap(eb, 1);
    ASSERT_EQ(diff_snapshots(old, neo), 1);
    free_snap(old); free_snap(neo);
}

static void test_size_changed_same_hash_counts_as_no_change(void)
{
    FileEntry *ea[] = { make_entry("f.txt", HASH_A, 5, 0644) };
    FileEntry *eb[] = { make_entry("f.txt", HASH_A, 9, 0644) };
    Snapshot *old = make_snap(ea, 1);
    Snapshot *neo = make_snap(eb, 1);
    ASSERT_EQ(diff_snapshots(old, neo), 0);
    free_snap(old); free_snap(neo);
}

static void test_multiple_adds_removes_modifies(void)
{
    FileEntry *ea[] = {
        make_entry("a.txt", HASH_A, 1, 0644),
        make_entry("b.txt", HASH_A, 1, 0644),
        make_entry("c.txt", HASH_A, 1, 0644),
    };
    FileEntry *eb[] = {
        make_entry("a.txt", HASH_B, 2, 0644),
        make_entry("d.txt", HASH_A, 1, 0644),
    };
    Snapshot *old = make_snap(ea, 3);
    Snapshot *neo = make_snap(eb, 2);
    ASSERT_EQ(diff_snapshots(old, neo), 4);
    free_snap(old); free_snap(neo);
}

static void test_identical_many_files(void)
{
    enum { N = 20 };
    FileEntry *ea[N], *eb[N];
    char name[32];
    for (int i = 0; i < N; i++) {
        snprintf(name, sizeof(name), "file%02d.txt", i);
        ea[i] = make_entry(name, HASH_A, 100, 0644);
        eb[i] = make_entry(name, HASH_A, 100, 0644);
    }
    Snapshot *old = make_snap(ea, N);
    Snapshot *neo = make_snap(eb, N);
    ASSERT_EQ(diff_snapshots(old, neo), 0);
    free_snap(old); free_snap(neo);
}

static void test_all_files_replaced(void)
{
    enum { N = 10 };
    FileEntry *ea[N], *eb[N];
    char na[32], nb[32];
    for (int i = 0; i < N; i++) {
        snprintf(na, sizeof(na), "old%02d.txt", i);
        snprintf(nb, sizeof(nb), "new%02d.txt", i);
        ea[i] = make_entry(na, HASH_A, 1, 0644);
        eb[i] = make_entry(nb, HASH_B, 1, 0644);
    }
    Snapshot *old = make_snap(ea, N);
    Snapshot *neo = make_snap(eb, N);
    ASSERT_EQ(diff_snapshots(old, neo), N * 2);
    free_snap(old); free_snap(neo);
}

static void test_nested_paths(void)
{
    FileEntry *ea[] = {
        make_entry("dir/sub/a.txt", HASH_A, 1, 0644),
        make_entry("dir/b.txt",     HASH_A, 1, 0644),
    };
    FileEntry *eb[] = {
        make_entry("dir/sub/a.txt", HASH_B, 1, 0644),
        make_entry("dir/b.txt",     HASH_A, 1, 0644),
        make_entry("dir/sub/c.txt", HASH_C, 1, 0644),
    };
    Snapshot *old = make_snap(ea, 2);
    Snapshot *neo = make_snap(eb, 3);
    ASSERT_EQ(diff_snapshots(old, neo), 2);
    free_snap(old); free_snap(neo);
}

static void test_idempotent_same_snap(void)
{
    FileEntry *ea[] = {
        make_entry("x.txt", HASH_A, 5, 0644),
        make_entry("y.txt", HASH_C, 3, 0755),
    };
    Snapshot *snap = make_snap(ea, 2);
    ASSERT_EQ(diff_snapshots(snap, snap), 0);
    free_snap(snap);
}

int main(void)
{
    test_no_changes();
    test_both_empty();
    test_old_empty_new_has_files();
    test_new_empty_old_has_files();
    test_added_file();
    test_removed_file();
    test_modified_content();
    test_chmod_only();
    test_content_and_mode_both_changed();
    test_size_changed_same_hash_counts_as_no_change();
    test_multiple_adds_removes_modifies();
    test_identical_many_files();
    test_all_files_replaced();
    test_nested_paths();
    test_idempotent_same_snap();
    TEST_SUMMARY();
}
