#ifndef LOG_H
#define LOG_H
#include <time.h>


typedef struct {
int log_id;
int user_id;
char description[200];
time_t timestamp;
} ActivityLog;


#endif // LOG_H
