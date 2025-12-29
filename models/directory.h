#ifndef DIRECTORY_H
#define DIRECTORY_H
#include <time.h>
#include <stdbool.h>


typedef struct {
int id;
int group_id; // FK to groups
char name[256]; // filename or folder name
char *path; // TEXT: full path to file/folder
long long size; // BIGINT: file size (0 for folders)
int uploaded_by; // FK to users
bool is_folder; // TRUE if folder, FALSE if file
int parent_id; // FK to root_directory (NULL if root level)
bool is_deleted;
time_t uploaded_at;
time_t updated_at;
} DirectoryEntry;

#endif // DIRECTORY_H
