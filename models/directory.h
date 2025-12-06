#ifndef DIRECTORY_H
#define DIRECTORY_H
#include <time.h>
#include <stdbool.h>


typedef struct {
int id;
int group_id; // FK
char name[256]; // filename or dirname
char *path; // TEXT
long long size; // BIGINT
int uploaded_by; // FK users
bool is_deleted;
time_t uploaded_at;
time_t updated_at;
} DirectoryEntry;

#endif // DIRECTORY_H
