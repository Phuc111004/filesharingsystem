#include "utils.h"
#include <string.h>
#include <stdio.h>


char* str_trim(char* s) {
if (!s) return s;
// trim leading
while(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
// TODO: implement full trim (this is a placeholder)
return s;
}


int hash_password(const char* password, char* out_hash, size_t out_size) {
// TODO: replace with a proper password hashing (bcrypt/argon2). Placeholder below:
if (!password || !out_hash) return -1;
snprintf(out_hash, out_size, "HASHED_%s", password);
return 0;
}
