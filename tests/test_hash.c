#include "testutil.h"
#include "../src/hash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void write_tmp(const char *path, const void *data, size_t len)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) { perror(path); exit(1); }
    fwrite(data, 1, len, fp);
    fclose(fp);
}

#define TMP(tag) "/tmp/snapdiff_hash_" tag "_" XSTR(__LINE__)
#define XSTR(x) STR(x)
#define STR(x) #x

static char g_tmp[256];
static void mktmp(const char *tag)
{
    snprintf(g_tmp, sizeof(g_tmp), "/tmp/snapdiff_hash_%s_%d", tag, (int)getpid());
}

static void test_known_hash_hello_newline(void)
{
    mktmp("hello");
    write_tmp(g_tmp, "hello\n", 6);
    char hex[HASH_HEX_LEN];
    ASSERT_EQ(hash_file(g_tmp, hex), 0);
    ASSERT_EQ((int)strlen(hex), 64);
    ASSERT_STR_EQ(hex, "5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03");
    remove(g_tmp);
}

static void test_agrees_with_system_sha256sum(void)
{
    mktmp("syscheck");
    write_tmp(g_tmp, "snapdiff test vector", 20);

    char hex[HASH_HEX_LEN];
    ASSERT_EQ(hash_file(g_tmp, hex), 0);

    char cmd[512], sys_hex[128];
    snprintf(cmd, sizeof(cmd), "shasum -a 256 %s", g_tmp);
    FILE *fp = popen(cmd, "r");
    ASSERT(fp != NULL);
    fscanf(fp, "%127s", sys_hex);
    pclose(fp);

    ASSERT_STR_EQ(hex, sys_hex);
    remove(g_tmp);
}

static void test_empty_file(void)
{
    mktmp("empty");
    write_tmp(g_tmp, "", 0);
    char hex[HASH_HEX_LEN];
    ASSERT_EQ(hash_file(g_tmp, hex), 0);
    ASSERT_STR_EQ(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    remove(g_tmp);
}

static void test_output_is_lowercase_hex(void)
{
    mktmp("case");
    write_tmp(g_tmp, "x", 1);
    char hex[HASH_HEX_LEN];
    hash_file(g_tmp, hex);
    for (int i = 0; hex[i]; i++)
        ASSERT((hex[i] >= '0' && hex[i] <= '9') || (hex[i] >= 'a' && hex[i] <= 'f'));
    remove(g_tmp);
}

static void test_output_length_is_64(void)
{
    mktmp("len");
    write_tmp(g_tmp, "test", 4);
    char hex[HASH_HEX_LEN];
    hash_file(g_tmp, hex);
    ASSERT_EQ((int)strlen(hex), 64);
    remove(g_tmp);
}

static void test_deterministic(void)
{
    mktmp("det");
    write_tmp(g_tmp, "same content", 12);
    char hex1[HASH_HEX_LEN], hex2[HASH_HEX_LEN];
    ASSERT_EQ(hash_file(g_tmp, hex1), 0);
    ASSERT_EQ(hash_file(g_tmp, hex2), 0);
    ASSERT_STR_EQ(hex1, hex2);
    remove(g_tmp);
}

static void test_different_content_yields_different_hash(void)
{
    char pa[256], pb[256];
    snprintf(pa, sizeof(pa), "/tmp/snapdiff_hash_diffa_%d", (int)getpid());
    snprintf(pb, sizeof(pb), "/tmp/snapdiff_hash_diffb_%d", (int)getpid());
    write_tmp(pa, "aaa", 3);
    write_tmp(pb, "bbb", 3);
    char ha[HASH_HEX_LEN], hb[HASH_HEX_LEN];
    hash_file(pa, ha);
    hash_file(pb, hb);
    ASSERT(strcmp(ha, hb) != 0);
    remove(pa);
    remove(pb);
}

static void test_single_byte_differs(void)
{
    char pa[256], pb[256];
    snprintf(pa, sizeof(pa), "/tmp/snapdiff_hash_bytea_%d", (int)getpid());
    snprintf(pb, sizeof(pb), "/tmp/snapdiff_hash_byteb_%d", (int)getpid());
    write_tmp(pa, "hello", 5);
    write_tmp(pb, "hellp", 5);
    char ha[HASH_HEX_LEN], hb[HASH_HEX_LEN];
    hash_file(pa, ha);
    hash_file(pb, hb);
    ASSERT(strcmp(ha, hb) != 0);
    remove(pa);
    remove(pb);
}

static void test_binary_data(void)
{
    mktmp("bin");
    unsigned char data[256];
    for (int i = 0; i < 256; i++) data[i] = (unsigned char)i;
    write_tmp(g_tmp, data, 256);
    char hex[HASH_HEX_LEN];
    ASSERT_EQ(hash_file(g_tmp, hex), 0);
    ASSERT_EQ((int)strlen(hex), 64);
    remove(g_tmp);
}

static void test_null_bytes_in_content(void)
{
    mktmp("null");
    char data[8] = {0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04};
    write_tmp(g_tmp, data, 8);
    char hex[HASH_HEX_LEN];
    ASSERT_EQ(hash_file(g_tmp, hex), 0);
    ASSERT_EQ((int)strlen(hex), 64);
    remove(g_tmp);
}

static void test_missing_file_returns_error(void)
{
    char hex[HASH_HEX_LEN];
    int rc = hash_file("/tmp/snapdiff_no_such_file_xyz_99", hex);
    ASSERT_EQ(rc, -1);
}

static void test_missing_file_output_is_safe(void)
{
    char hex[HASH_HEX_LEN];
    hash_file("/tmp/snapdiff_no_such_file_xyz_99", hex);
    ASSERT_EQ((int)strlen(hex), 64);
}

static void test_large_file(void)
{
    mktmp("large");
    FILE *fp = fopen(g_tmp, "wb");
    char block[4096];
    memset(block, 0xAB, sizeof(block));
    for (int i = 0; i < 256; i++) fwrite(block, 1, sizeof(block), fp);
    fclose(fp);
    char hex[HASH_HEX_LEN];
    ASSERT_EQ(hash_file(g_tmp, hex), 0);
    ASSERT_EQ((int)strlen(hex), 64);
    remove(g_tmp);
}

int main(void)
{
    test_known_hash_hello_newline();
    test_agrees_with_system_sha256sum();
    test_empty_file();
    test_output_is_lowercase_hex();
    test_output_length_is_64();
    test_deterministic();
    test_different_content_yields_different_hash();
    test_single_byte_differs();
    test_binary_data();
    test_null_bytes_in_content();
    test_missing_file_returns_error();
    test_missing_file_output_is_safe();
    test_large_file();
    TEST_SUMMARY();
}
