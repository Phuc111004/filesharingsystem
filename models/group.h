#ifndef GROUP_H
#define GROUP_H
#include <time.h>


typedef struct {
int group_id;
char group_name[101];
char *description; // text
int created_by; // FK users.user_id
int root_dir_id; // FK root_directory.id
time_t created_at;
} Group;

#endif // GROUP_H
