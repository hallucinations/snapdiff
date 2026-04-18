#include "testutil.h"
#include "../src/snapshot.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static void make_dir(const char *path)
{
    mkdir(path, 0755);
}

static void write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "w");
    if (!fp) { perror(path); exit(1); }
    fputs(content, fp);
    fclose(fp);
}

static void mkpath(char *buf, size_t n, const char *tag)
{
    snprintf(buf, n, "/tmp/snapdiff_snap_%s_%d", tag, (int)getpid());
}

static FileEntry *find_entry(const Snapshot *snap, const char *path)
{
    for (FileEntry *e = snap->head; e; e = e->next)
        if (strcmp(e->path, path) == 0) return e;
    return NULL;
}

static void test_create_basic_count(void)
{
    char dir[128];
    mkpath(dir, sizeof(dir), "basic");
    make_dir(dir);
    char p[256];
    snprintf(p, sizeof(p), "%s/a.txt", dir); write_file(p, "hello");
    snprintf(p, sizeof(p), "%s/b.txt", dir); write_file(p, "world");

    Snapshot *snap = snapshot_create(dir);
    ASSERT(snap != NULL);
    ASSERT_EQ(snap->count, 2);
    ASSERT_STR_EQ(snap->root, dir);
    ASSERT(snap->created > 0);
    snapshot_free(snap);
}

static void test_create_empty_dir(void)
{
    char dir[128];
    mkpath(dir, sizeof(dir), "empty");
    make_dir(dir);
    Snapshot *snap = snapshot_create(dir);
    ASSERT(snap != NULL);
    ASSERT_EQ(snap->count, 0);
    ASSERT(snap->head == NULL);
    snapshot_free(snap);
}

static void test_create_single_file(void)
{
    char dir[128];
    mkpath(dir, sizeof(dir), "single");
    make_dir(dir);
    char p[256];
    snprintf(p, sizeof(p), "%s/only.txt", dir);
    write_file(p, "content");
    Snapshot *snap = snapshot_create(dir);
    ASSERT_EQ(snap->count, 1);
    ASSERT(snap->head != NULL);
    ASSERT_STR_EQ(snap->head->path, "only.txt");
    ASSERT(snap->head->size == (off_t)7);
    ASSERT(snap->head->mode != 0);
    ASSERT((int)strlen(snap->head->hash) == 64);
    snapshot_free(snap);
}

static void test_create_nested(void)
{
    char dir[128], sub[256], f[384];
    mkpath(dir, sizeof(dir), "nested");
    snprintf(sub, sizeof(sub), "%s/sub", dir);
    make_dir(dir);
    make_dir(sub);
    snprintf(f, sizeof(f), "%s/root.txt", dir);   write_file(f, "root");
    snprintf(f, sizeof(f), "%s/child.txt", sub);  write_file(f, "child");
    Snapshot *snap = snapshot_create(dir);
    ASSERT_EQ(snap->count, 2);
    ASSERT(find_entry(snap, "root.txt") != NULL);
    ASSERT(find_entry(snap, "sub/child.txt") != NULL);
    snapshot_free(snap);
}

static void test_create_deeply_nested(void)
{
    char dir[128], a[256], b[320], c[384];
    mkpath(dir, sizeof(dir), "deep");
    snprintf(a, sizeof(a), "%s/a", dir);
    snprintf(b, sizeof(b), "%s/a/b", dir);
    snprintf(c, sizeof(c), "%s/a/b/c", dir);
    make_dir(dir); make_dir(a); make_dir(b); make_dir(c);
    char f[512];
    snprintf(f, sizeof(f), "%s/deep.txt", c); write_file(f, "deep");
    Snapshot *snap = snapshot_create(dir);
    ASSERT_EQ(snap->count, 1);
    ASSERT(find_entry(snap, "a/b/c/deep.txt") != NULL);
    snapshot_free(snap);
}

static void test_entries_have_correct_size(void)
{
    char dir[128], p[256];
    mkpath(dir, sizeof(dir), "size");
    make_dir(dir);
    snprintf(p, sizeof(p), "%s/exact.txt", dir);
    write_file(p, "12345678");
    Snapshot *snap = snapshot_create(dir);
    FileEntry *e = find_entry(snap, "exact.txt");
    ASSERT(e != NULL);
    ASSERT_EQ((int)e->size, 8);
    snapshot_free(snap);
}

static void test_entries_have_valid_mode(void)
{
    char dir[128], p[256];
    mkpath(dir, sizeof(dir), "mode");
    make_dir(dir);
    snprintf(p, sizeof(p), "%s/m.txt", dir);
    write_file(p, "x");
    chmod(p, 0600);
    Snapshot *snap = snapshot_create(dir);
    FileEntry *e = find_entry(snap, "m.txt");
    ASSERT(e != NULL);
    ASSERT_EQ((int)(e->mode & 0777), 0600);
    snapshot_free(snap);
}

static void test_file_content_changes_hash(void)
{
    char dir[128], p[256];
    mkpath(dir, sizeof(dir), "rehash");
    make_dir(dir);
    snprintf(p, sizeof(p), "%s/f.txt", dir);

    write_file(p, "version one");
    Snapshot *s1 = snapshot_create(dir);
    char hash1[HASH_HEX_LEN];
    snprintf(hash1, sizeof(hash1), "%s", s1->head->hash);
    snapshot_free(s1);

    write_file(p, "version two");
    Snapshot *s2 = snapshot_create(dir);
    ASSERT(strcmp(hash1, s2->head->hash) != 0);
    snapshot_free(s2);
}

static void test_save_load_roundtrip(void)
{
    char dir[128], p[256], snap_path[128];
    mkpath(dir, sizeof(dir), "rt");
    make_dir(dir);
    snprintf(p, sizeof(p), "%s/only.txt", dir);
    write_file(p, "roundtrip content");
    snprintf(snap_path, sizeof(snap_path), "/tmp/snapdiff_rt_%d.snap", (int)getpid());

    Snapshot *orig = snapshot_create(dir);
    ASSERT_EQ(orig->count, 1);
    ASSERT_EQ(snapshot_save(orig, snap_path), 0);

    Snapshot *loaded = snapshot_load(snap_path);
    ASSERT(loaded != NULL);
    ASSERT_EQ(loaded->count, orig->count);
    ASSERT_STR_EQ(loaded->root, orig->root);
    ASSERT_EQ((long)loaded->created, (long)orig->created);

    FileEntry *lo = find_entry(loaded, "only.txt");
    FileEntry *oo = find_entry(orig,   "only.txt");
    ASSERT(lo != NULL);
    ASSERT(oo != NULL);
    ASSERT_STR_EQ(lo->hash, oo->hash);
    ASSERT_EQ((int)lo->size, (int)oo->size);
    ASSERT_EQ((int)lo->mode, (int)oo->mode);

    snapshot_free(orig);
    snapshot_free(loaded);
    remove(snap_path);
}

static void test_save_load_multiple_files(void)
{
    char dir[128], snap_path[128], p[256];
    mkpath(dir, sizeof(dir), "multi");
    make_dir(dir);
    snprintf(p, sizeof(p), "%s/one.txt", dir);   write_file(p, "one");
    snprintf(p, sizeof(p), "%s/two.txt", dir);   write_file(p, "two");
    snprintf(p, sizeof(p), "%s/three.txt", dir); write_file(p, "three");
    snprintf(snap_path, sizeof(snap_path), "/tmp/snapdiff_multi_%d.snap", (int)getpid());

    Snapshot *orig = snapshot_create(dir);
    ASSERT_EQ(orig->count, 3);
    ASSERT_EQ(snapshot_save(orig, snap_path), 0);

    Snapshot *loaded = snapshot_load(snap_path);
    ASSERT(loaded != NULL);
    ASSERT_EQ(loaded->count, 3);
    ASSERT(find_entry(loaded, "one.txt")   != NULL);
    ASSERT(find_entry(loaded, "two.txt")   != NULL);
    ASSERT(find_entry(loaded, "three.txt") != NULL);

    snapshot_free(orig);
    snapshot_free(loaded);
    remove(snap_path);
}

static void test_save_load_preserves_hash(void)
{
    char dir[128], snap_path[128], p[256];
    mkpath(dir, sizeof(dir), "hash_rt");
    make_dir(dir);
    snprintf(p, sizeof(p), "%s/data.bin", dir);
    unsigned char bytes[64];
    for (int i = 0; i < 64; i++) bytes[i] = (unsigned char)i;
    FILE *fp = fopen(p, "wb");
    fwrite(bytes, 1, 64, fp);
    fclose(fp);
    snprintf(snap_path, sizeof(snap_path), "/tmp/snapdiff_hash_rt_%d.snap", (int)getpid());

    Snapshot *orig = snapshot_create(dir);
    snapshot_save(orig, snap_path);
    Snapshot *loaded = snapshot_load(snap_path);

    FileEntry *oe = find_entry(orig,   "data.bin");
    FileEntry *le = find_entry(loaded, "data.bin");
    ASSERT(oe && le);
    ASSERT_STR_EQ(le->hash, oe->hash);

    snapshot_free(orig);
    snapshot_free(loaded);
    remove(snap_path);
}

static void test_load_nonexistent(void)
{
    Snapshot *snap = snapshot_load("/tmp/snapdiff_no_such_file_xyz.snap");
    ASSERT(snap == NULL);
}

static void test_save_invalid_path(void)
{
    char dir[128], p[256];
    mkpath(dir, sizeof(dir), "inv");
    make_dir(dir);
    snprintf(p, sizeof(p), "%s/x.txt", dir);
    write_file(p, "x");
    Snapshot *snap = snapshot_create(dir);
    ASSERT_EQ(snapshot_save(snap, "/tmp/no/such/dir/out.snap"), -1);
    snapshot_free(snap);
}

static void test_snapshot_free_null(void)
{
    snapshot_free(NULL);
    ASSERT(1);
}

static void test_created_timestamp_is_recent(void)
{
    char dir[128];
    mkpath(dir, sizeof(dir), "ts");
    make_dir(dir);
    time_t before = time(NULL);
    Snapshot *snap = snapshot_create(dir);
    time_t after = time(NULL);
    ASSERT(snap->created >= before);
    ASSERT(snap->created <= after);
    snapshot_free(snap);
}

int main(void)
{
    test_create_basic_count();
    test_create_empty_dir();
    test_create_single_file();
    test_create_nested();
    test_create_deeply_nested();
    test_entries_have_correct_size();
    test_entries_have_valid_mode();
    test_file_content_changes_hash();
    test_save_load_roundtrip();
    test_save_load_multiple_files();
    test_save_load_preserves_hash();
    test_load_nonexistent();
    test_save_invalid_path();
    test_snapshot_free_null();
    test_created_timestamp_is_recent();
    TEST_SUMMARY();
}
