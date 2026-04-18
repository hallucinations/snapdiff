#ifndef HASH_H
#define HASH_H

#include <stddef.h>

#define HASH_HEX_LEN 65

int hash_file(const char *path, char out_hex[HASH_HEX_LEN]);

#endif
