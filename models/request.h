#ifndef REQUEST_H
#define REQUEST_H
#include <time.h>


typedef enum {
REQ_JOIN,
REQ_INVITATION
} RequestType;


typedef enum {
REQ_PENDING,
REQ_ACCEPTED,
REQ_REJECTED
} RequestStatus;


typedef struct {
int request_id;
int user_id;
int group_id;
RequestType request_type;
RequestStatus status;
time_t created_at;
time_t updated_at;
} GroupRequest;

#endif // REQUEST_H
