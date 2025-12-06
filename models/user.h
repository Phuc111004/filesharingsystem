#ifndef USER_H
#define USER_H
#include <time.h>


typedef struct {
int user_id;
char username[51];
char password[256]; // hashed
char session_token[65]; // varchar(64)
int token_expiry; // expiry timestamp
time_t created_at;
long long new_column; // BIGINT
} User;

#endif // USER_H
