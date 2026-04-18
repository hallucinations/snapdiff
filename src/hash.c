#include "hash.h"
#include <stdio.h>
#include <openssl/evp.h>

int hash_file(const char *path, char out_hex[HASH_HEX_LEN])
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        snprintf(out_hex, HASH_HEX_LEN, "%064d", 0);
        return -1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(fp); return -1; }
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);

    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        EVP_DigestUpdate(ctx, buf, n);
    fclose(fp);

    unsigned char digest[32];
    unsigned int  digest_len;
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    for (int i = 0; i < 32; i++)
        snprintf(out_hex + i * 2, 3, "%02x", digest[i]);
    out_hex[64] = '\0';
    return 0;
}
