#ifndef __NETWORK_H__
#define __NETWORK_H__
#include <stdlib.h>
#include <stdio.h>
#include <sys/poll.h>
#include <fnmatch.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>
#include "list.h"
#include "locker.h"
#include "logger.h"
#include "debug.h"

enum network_cbk_event
{
	NETWORK_CBK_NEW,
	NETWORK_CBK_CLOSE,
	NETWORK_CBK_DATA,
	NETWORK_CBK_RESULT,
	NETWORK_CBK_MAX
};

typedef enum 
{
	SOCKET_NADA = 0,
	SOCKET_HEADER_COMING,
	SOCKET_HEADER_CAME,
	SOCKET_DATA_COMING,
	SOCKET_DATA_CAME,
	SOCKET_COMPLETE,
} socket_recv_state_t;

typedef enum
{
	SOCKET_NONE = 0,
	SOCKET_DATA_SENDING,
	SOCKET_DATA_SEND,
	SOCKET_OK,
} socket_send_state_t;

enum network_identify
{
	NETWORK_CLIENT,
	NETWORK_SERVER
};

enum network_IP_TYPE
{
	IS_IPV4,
	IS_IPV6
};
typedef struct network network_t;
struct socket_header
{
	char     name[8];
	uint32_t size;
} __attribute__((packed));

typedef struct sock_msg
{
	struct list_node node;
	int state;
	struct socket_header header;
	char head_buf[12];
	void *buf;
	int pos;
	int size;
} sock_msg_t;

typedef struct socket_info
{
	int32_t sock;
	unsigned char connected;
	locker_t lock;
	int windowsize;
	char nodelay;
	int keepalive;
	int keepaliveintvl;
	struct list_node *send_list;
	sock_msg_t recv;
} socket_info_t;

typedef int (*event_handler_t) (int fd, void *data,
		int poll_in, int poll_out, int poll_err);

typedef struct register_info
{
	struct list_node node;
	int fd;
	int events;
	void *data;
	event_handler_t handler;
}register_info_t;

typedef struct events_info
{
	struct list_node node;
	struct epoll_event events;
}events_info_t;

typedef struct event_pool 
{
	struct list_node *head;//point to struct register_info
	int fd;
	int count;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct epoll_event *events;
	int events_size;
}event_pool_t;

typedef struct event_deal {
	struct event_pool * (*event_create) (int count);

	int (*event_register) (struct event_pool *event_pool, int fd,
			event_handler_t handler,
			void *data, int poll_in, int poll_out);

	int (*event_select_on) (struct event_pool *event_pool, int fd, int idx,
			int poll_in, int poll_out);

	int (*event_unregister) (struct event_pool *event_pool, int fd, int idx);

	int (*event_dispatch) (struct event_pool *event_pool);
}event_deal_t;

typedef int (*network_cbk_t) (network_t* self, int type, void* data, int size);

typedef struct network_ops 
{
	int32_t (*submit)     (network_t *self, char *buf, int len);
	int32_t (*connect)    (network_t *self);
	int32_t (*listen)     (network_t *self);
	int32_t (*disconnect) (network_t *self);
}network_ops_t;

struct network 
{
	int 							count;
	network_ops_t		  			*ops;
	void                  			*sockinfo;
	locker_t        				lock;
	struct event_pool	    	   	*event_pool;
	int 							role;
	int 							ip_type;
	char 							ip[128];
	int16_t 						port;
	network_cbk_t 					callback;
};

network_t		*network_server_config(int ip_type, char* addr, int16_t port, network_cbk_t callback);
void		 	network_server_close(network_t* self); 
int32_t 		network_send (network_t *self, char *buf, int32_t len);
network_t		*network_client_connect(int ip_type, char* addr, int16_t port, network_cbk_t callback);
int32_t			network_client_close(network_t* self);

#endif
