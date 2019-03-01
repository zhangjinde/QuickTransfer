
#ifndef _SESSION_H_
#define _SESSION_H_
#include "../udp_piece/udp_socket.h"
#include "file.h"

typedef struct _jvt_session
{
    jvt_file_t files_[JVT_MAX_FILES];
    udp_socket_t udp_socket_;
} jvt_session_t;


void jvt_session_on_recv_data(void* user_data, udp_socket_result_t* result);

int jvt_session_init(jvt_session_t *S);
void jvt_session_uninit(jvt_session_t *S);

void jvt_session_recv_downloadfile_req(jvt_session_t *S, pt_downloadfile_req *req);
void jvt_session_recv_transferfile_ack(jvt_session_t *S, pt_transferfile_ack *ack);

#endif //_SERVER_H_