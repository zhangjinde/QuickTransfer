#include "prefixhead.h"
#include "session.h"
#include "parser.h"

jvt_file_t* _new_file(jvt_session_t *S, const char *filename)
{
	size_t i = 0;
	for(size_t i = 0; i < sizeof(S->files_)/sizeof(jvt_file_t); i++)
	{
		if (S->files_[i].fileid_ == 0)
		{
			gettimeofday(&S->files_[i].tv_, NULL);
			strcpy(S->files_[i].fileinfo_.filename, filename);
			return &S->files_[i];
		}
	}
	
	return NULL;
}

jvt_file_t* _find_file(jvt_session_t *S, const char *filename)
{
	size_t i = 0;
	for(size_t i = 0; i < sizeof(S->files_)/sizeof(jvt_file_t); i++)
	{
		if (strcmp(filename, S->files_[i].fileinfo_.filename)  == 0)
		{
			return &S->files_[i];
		}
	}
	
	return NULL;
}

jvt_file_t* _find_file_byfileid(jvt_session_t *S, int fileid)
{
	size_t i = 0;
	for(size_t i = 0; i < sizeof(S->files_)/sizeof(jvt_file_t); i++)
	{
		if (fileid == S->files_[i].fileid_)
		{
			return &S->files_[i];
		}
	}
	
	return NULL;
}

int _send_data(jvt_session_t *S, void *buf, int size)
{
	jvt_net_event_t evt;
	evt.eid = 0;

	evt.data = jvt_parser_encode(S, buf, size, &evt.len);
	if (!evt.data)
	{
		LOG_ERR("failed to encode! buf=%p, size=%d", buf, size);
		return -1;
	}

	// 加入反应堆队列
	jvt_net_reactor_push_event(&S->reactor, &S->udp_socket_, &evt);
	jvt_net_reactor_add_event(&S->reactor, &S->udp_socket_, EPOLLOUT);

	return 0;
}

udp_socket_t* _on_find_udp_socket(udp_socket_t *udp_socket, char* ip, int port
								, reset_udp_socket_func_t func, void *user_data)
{
	udp_socket_t* udp_socket_new = udp_socket;

	if ((strcmp(udp_socket_new->ip, ip) != 0) || (udp_socket_new->port != port))
		return NULL;

	return udp_socket_new;
}

void _on_recv_data(void* user_data, udp_socket_result_t* result)
{
	assert(result);
	jvt_session_t* session = (jvt_session_t *)user_data;

	if (result->type == RECV_TYPE_SUCCESS)
	{
		if (0 != jvt_parser_decode(session, (void *)result->data, result->len))
		{
			LOG_ERR("failed to decode the message: data=%p, len=%d", result->data, result->len);
		}
	}
	else if (result->type == RECV_TYPE_INCOMPLETE)
	{
        // LOG_INF("recv: type=%d", result->type, result->data, result->len);
	}
	else
	{
        LOG_ERR("recv: error=%d, data=%p, len=%d", result->type, result->data, result->len);
	}
}

int _download_file_req(jvt_session_t *session, const char *filename)
{
	jvt_file_t* file = _new_file(session, filename);
	if (!file)
	{
		LOG_INF("no enough space to save file: '%s'!", filename);
		return -1;
	}

	pt_downloadfile_req req;
	req.opcode = downloadfile_req;
	strcpy(req.filename, filename);
	if (_send_data(session, (void *)&req, sizeof(pt_downloadfile_req)) != 0)
	{
		LOG_INF("send[downloadfile_req]::failed to send req: filename='%s'", filename);
		return -2;
	}

    LOG_INF("send[%s:%d][downloadfile_req]::filename='%s'", session->udp_socket_.ip, session->udp_socket_.port, req.filename);

	return 0;
}

int _transfer_file_req(jvt_session_t *session, int fileid, int block, int ret)
{
	pt_transferfile_ack ack;
	ack.opcode = transferfile_ack;
	ack.ret = ret;
	ack.fileid = fileid;
	ack.block = block;
	_send_data(session, (void *)&ack, sizeof(ack));

    LOG_INF("send[%s:%d][transferfile_ack]:: ret=%d, fileid=%d, block=%d"
		, session->udp_socket_.ip, session->udp_socket_.port
		, ack.ret, ack.fileid, ack.block);

	return ret;
}

int jvt_session_init(jvt_session_t *session, char *ip, int port)
{
	assert(session);

	memset(session->files_, 0, sizeof(session->files_));
	session->udp_socket_.callback.session = session;
    session->udp_socket_.callback.find_udp_socket_func = _on_find_udp_socket;
    session->udp_socket_.callback.create_session_func = NULL;
    session->udp_socket_.callback.destroy_session_func = NULL;
    session->udp_socket_.callback.recv_data_func = _on_recv_data;
	int ret = udp_socket_init(&session->udp_socket_, ip, port, JVT_PIECE_CAPACITY);
	if (ret < 0)
	{
		LOG_ERR("failed to init socket! ret=%d, port=%d", ret, port);
		return ret;
	}

	ret = jvt_net_reactor_init(&session->reactor);
	if (ret < 0)
	{
		LOG_ERR("failed to initalize reactor! ret=%d", ret);
		return ret;
	}

	ret = jvt_net_reactor_add_event(&session->reactor, &session->udp_socket_, EPOLLIN);
	if (ret < 0)
	{
		LOG_ERR("failed to add reactor event[%d]! ret=%d", EPOLLIN, ret);
		return ret;
	}

	return 0;
}

void jvt_session_uninit(jvt_session_t *S)
{
	LOG_ERR("TODO: jvt_session_uninit...");
}

void jvt_session_run(jvt_session_t *S)
{
	_download_file_req(S, "git_cmd.jpg");
	// _download_file_req(S, "test100KB.txt");
	// _download_file_req(S, "test3KB.txt");
	
	// 反应堆处理过程：
	// 0.启动反应堆
	// 1.反应堆线程: 循环epoll_wait，并将结果以事件的方式保存在队列Q中
	// 2.主线程: 循环取出队列Q的事件, 并分发处理
	if (0 != jvt_net_reactor_run(&S->reactor))
	{
		LOG_ERR("failed to run reactor!");
	}
}

void jvt_session_recv_downloadfile_ack(jvt_session_t *S, pt_downloadfile_ack *ack)
{
    LOG_INF("recv[%s:%d][downloadfile_ack]:: ret=%d, fileid=%d, filesize=%d, filename='%s'"
		, S->udp_socket_.ip, S->udp_socket_.port
		, ack->ret, ack->fileid, ack->filesize, ack->filename);

	jvt_file_t* file = _find_file(S, ack->filename);
	if (!file)
	{
   		LOG_ERR("file['%s'] is not existed!", ack->filename);
		return;
	}

	if ( 0!= jvt_file_init(file, ack->fileid, ack->filesize, S))
	{
   		LOG_ERR("failed to initailize file: '%s'!", ack->filename);
		return;
	}

	_transfer_file_req(S, file->fileid_, 0, 0);
}

void jvt_session_recv_transferfile_noti(jvt_session_t *S, pt_transferfile_noti *noti)
{
    LOG_INF("send[%s:%d][transferfile_noti]:: fileid=%d, block=%d, size=%d, data='%2.2s'"
		, S->udp_socket_.ip, S->udp_socket_.port
		, noti->fileid, noti->block, noti->size, noti->data);

	int ret = 0;
	bool completed = false;
	// char data[FILE_PACKET] = {0};
	// jvt_base64_decode(data, noti->data, noti->size);

	jvt_file_t* file = _find_file_byfileid(S, noti->fileid);

	do
	{
		if (!file)
		{
			ret = -1;
   			LOG_ERR("failed to find file[%d]!", noti->fileid);
			break;
		}

		if (0 != jvt_file_write(file, noti->block, noti->data, noti->size))
		// if (0 != jvt_file_write(file, noti->block, data, strlen(data)+1))
		{
			ret = -2;
   			LOG_ERR("failed to write file[%d]!", noti->fileid);
			break;
		}
	
		if ((noti->block + 1) * FILE_BLOCK >= file->fileinfo_.filesize)
		{
			completed = true;
		}

	} while(0);

	if (0 == _transfer_file_req(S, noti->fileid, noti->block + 1, ret))
	{
		if (completed)
		{
			jvt_file_uninit(file);
		}
	}
}