#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>


char* str_trim(char* s) {
if (!s) return s;
// trim leading
while(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
// TODO: implement full trim (this is a placeholder)
return s;
}


int utils_hash_password(const char* password, char* out_hash, size_t out_size) {
	// TODO: replace with a proper password hashing (bcrypt/argon2). Placeholder below:
	if (!password || !out_hash) return -1;
	snprintf(out_hash, out_size, "HASHED_%s", password);
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
