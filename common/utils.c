#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <openssl/sha.h>


char* str_trim(char* s) {
if (!s) return s;
// trim leading
while(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
// TODO: implement full trim (this is a placeholder)
return s;
}


int utils_hash_password(const char* password, char* out_hash, size_t out_size) {
    if (!password || !out_hash) return -1;

    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (!SHA256((const unsigned char*)password, strlen(password), digest)) {
        return -1;
    }

    // Need 64 hex chars + null
    const size_t needed = SHA256_DIGEST_LENGTH * 2 + 1;
    if (out_size < needed) return -1;

    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(out_hash + (i * 2), "%02x", digest[i]);
    }
    out_hash[SHA256_DIGEST_LENGTH * 2] = '\0';
    return 0;
}

void mkdir_p(const char *path) {
    char temp[1024];
    snprintf(temp, sizeof(temp), "%s", path);
    size_t len = strlen(temp);
    for (int i = 0; i < len; i++) {
        if (temp[i] == '/') {
            temp[i] = '\0';
            mkdir(temp, 0700);
            temp[i] = '/';
        }
    }
    mkdir(temp, 0700);
}

int split_args(char *line, char **args, int max_args) {
    int count = 0;
    char *p = line;

    while (*p && count < max_args) {
        // Skip whitespace
        while (*p && *p == ' ') p++;
        if (!*p) break;

        if (*p == '"') {
            p++; // Skip opening quote
            args[count++] = p;
            while (*p && *p != '"') p++;
        } else {
            args[count++] = p;
            while (*p && *p != ' ') p++;
        }

        if (*p) {
            *p = '\0'; // Terminate token
            p++;
        }
    }
    return count;
}
