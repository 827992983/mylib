#include "network.h"

#define DEFAULT_SOCKET_WINDOW_SIZE 128*1024;

static void write_err_fd(int sig);
static int sock_server_event_handler(int fd, void *data, int poll_in, int poll_out, int poll_err);
static int sock_event_handler(int fd, void *data, int poll_in, int poll_out, int poll_err);
static event_pool_t *event_pool_create(int count);
static int event_pool_register(event_pool_t *event_pool, int fd, event_handler_t handler, void *data, int poll_in, int poll_out);
static int event_dispatch_epoll(event_pool_t *event_pool);
static void *event_dispatch_start(void *);
static int event_dispatch_epoll_handler(struct event_pool *event_pool, int i);

static int sock_validate_header(network_t *self, struct socket_header *header, size_t *size1_p);
static void sock_msg_entry_free(sock_msg_t *entry);
static void sock_msg_flush(network_t *self);
static int sock_init(network_t *self);
static void sock_reset(network_t *self);
static int sock_listen(network_t *self);
static int sock_server_bind(network_t *self);
static int sock_nodelay(int fd);
static int sock_nonblock(int fd);
static int sock_keepalive (int fd, int keepalive_intvl);
static int sock_connect(network_t *self);
static int sock_connect_state(int fd);
static int sock_connect_finish(network_t *self);
static int sock_disconnect(network_t *self);
static int sock_event_handler(int fd, void *data, int poll_in, int poll_out, int poll_err);
static int sock_server_event_handler(int fd, void *data, int poll_in, int poll_out, int poll_err);
static int sock_event_poll_err(network_t *self);
static int sock_event_poll_out(network_t *self);
static int sock_event_poll_in(network_t *self);
static sock_msg_t *sock_msg_new(network_t *self, char *buf, int len);
static void sock_msg_flush(network_t *self);
static int sock_msg_churn(network_t *self);
static int sock_submit(network_t *self, char *buf, int len);
static int sock_msg_send_entry(network_t *self, sock_msg_t *entry);
static int sock_write_data(network_t *self, void *buf, int size);
static int sock_read_data(network_t *self, void *buf, int size);
static ssize_t sock_writen(int fd, const void *ptr, size_t n);
static ssize_t sock_readn(int fd, void *ptr, size_t n);

static void clearup_network(network_t* self);
static network_t *network_load(void);
static int32_t network_listen(network_t *self);
static int32_t network_disconnect(network_t *self);
static void write_err_fd(int sig);

static void write_err_fd(int sig)
{
	logger(LOG_INFO,"SIGPIPE signal!\n");
}

static event_pool_t *event_pool_create(int count)
{
	event_pool_t *event_pool = NULL;
	int fd = 0;
	int ret = 0;
	pthread_t pid;

	event_pool = (event_pool_t *) malloc(sizeof(event_pool_t));
	if (!event_pool)
	{
		logger(LOG_ERROR,"event_pool malloc error!\n");
		return NULL;
	}
	event_pool->head = (struct list_node *) malloc(sizeof(struct list_node));
	if (!event_pool->head)
	{
		free(event_pool);
		logger(LOG_ERROR,"event_pool->head malloc error!\n");
		return NULL;
	}
	list_init(event_pool->head);
	fd = epoll_create(count);
	if (fd == -1)
	{
		logger(LOG_ERROR,"epoll_create error!\n");
		return NULL;
	}
	event_pool->fd = fd;
	event_pool->count = count;
	event_pool->events_size = 0;
	event_pool->events = (struct epoll_event *) malloc(
			event_pool->count * sizeof(struct epoll_event));
	pthread_mutex_init(&event_pool->mutex, NULL);
	pthread_cond_init(&event_pool->cond, NULL);
	ret = pthread_create(&pid, NULL, event_dispatch_start, event_pool);
	if (ret < 0)
	{
		logger(LOG_ERROR,"create pthread error!\n");
		free(event_pool);
		return NULL;
	}

	return event_pool;
}

static int event_pool_register(event_pool_t *event_pool, int fd,
		event_handler_t handler, void *data, int poll_in, int poll_out)
{
	int idx = 0;
	int ret = 0;
	struct epoll_event epoll_event =
	{ 0, };
	struct event_data *ev_data = (void *) &epoll_event.data;
	register_info_t *reg = NULL;
	if (event_pool == NULL)
	{
		logger(LOG_ERROR,"invalid argument\n");
		return -1;
	}

	pthread_mutex_lock(&event_pool->mutex);
	{
		if (list_size(event_pool->head) < event_pool->count)
		{
			reg = (register_info_t *) malloc(sizeof(register_info_t));
			if (!reg)
			{
				logger(LOG_ERROR,"event registry re-allocation failed\n");
				goto unlock;
			}
		}
		else
		{
			logger(LOG_ERROR,"epoll listen fd numbers is larger than init value(%d)\n",
					event_pool->count);
			goto unlock;
		}

		reg->fd = fd;
		reg->events = EPOLLPRI;
		reg->handler = handler;
		reg->data = data;
		list_insert(&reg->node, event_pool->head);
		switch (poll_in)
		{
		case 1:
			reg->events |= EPOLLIN;
			break;
		case 0:
			reg->events &= ~EPOLLIN;
			break;
		case -1:
			break;
		default:
			logger(LOG_ERROR,"invalid poll_in value %d", poll_in);
			break;
		}

		switch (poll_out)
		{
		case 1:
			reg->events |= EPOLLOUT;
			break;
		case 0:
			reg->events &= ~EPOLLOUT;
			break;
		case -1:
			break;
		default:
			logger(LOG_ERROR,"invalid poll_out value %d", poll_out);
			break;
		}

		epoll_event.events = reg->events;
		ret = epoll_ctl(event_pool->fd, EPOLL_CTL_ADD, fd, &epoll_event);
		if (ret == -1)
		{
			logger(LOG_ERROR,"failed to add fd(=%d) to epoll fd(=%d) (%s)", fd,
					event_pool->fd, strerror(errno));
			goto unlock;
		}

		pthread_cond_broadcast(&event_pool->cond);
	}
unlock: pthread_mutex_unlock(&event_pool->mutex);
		return ret;
}

static int event_pool_unregister(struct event_pool *event_pool, int fd)
{
	int ret = -1;
	struct epoll_event epoll_event =
	{ 0, };
	struct list_node *tmp;
	register_info_t *reg_info;

	if (event_pool == NULL)
	{
		logger(LOG_ERROR,"invalid argument");
		return -1;
	}

	pthread_mutex_lock(&event_pool->mutex);
	{
		list_for_each(tmp, event_pool->head)
		{
			reg_info = list_entry(tmp, register_info_t, node);
			if (reg_info->fd == fd)
			{
				list_remove(tmp);
				free(reg_info);
				//break;
			}
		}

		ret = epoll_ctl(event_pool->fd, EPOLL_CTL_DEL, fd, NULL);
		if (ret == -1)
		{
			logger(LOG_ERROR,"fail to del fd(=%d) from epoll fd(=%d) (%s)", fd,
					event_pool->fd, strerror(errno));
			goto unlock;
		}
	}
unlock: pthread_mutex_unlock(&event_pool->mutex);

		return ret;
}

static int event_pool_select(struct event_pool *event_pool, int fd,
		int poll_in, int poll_out)
{
	int ret = -1;
	struct epoll_event epoll_event =
	{ 0, };
	struct event_data *ev_data = (void *) &epoll_event.data;
	struct list_node *tmp;
	register_info_t *reg_info;

	if (event_pool == NULL)
	{
		logger(LOG_ERROR,"invalid argument");
		return -1;
	}

	pthread_mutex_lock(&event_pool->mutex);
	{
		list_for_each(tmp, event_pool->head)
		{
			reg_info = list_entry(tmp, register_info_t, node);
			if (reg_info->fd == fd)
			{
				break;
			}
		}
		switch (poll_in)
		{
		case 1:
			reg_info->events |= EPOLLIN;
			break;
		case 0:
			reg_info->events &= ~EPOLLIN;
			break;
		case -1:
			/* do nothing */
			break;
		default:
			logger(LOG_ERROR,"invalid poll_in value %d", poll_in);
			break;
		}

		switch (poll_out)
		{
		case 1:
			reg_info->events |= EPOLLOUT;
			break;
		case 0:
			reg_info->events &= ~EPOLLOUT;
			break;
		case -1:
			/* do nothing */
			break;
		default:
			logger(LOG_ERROR,"invalid poll_out value %d", poll_out);
			break;
		}
		epoll_event.events = reg_info->events;

		ret = epoll_ctl(event_pool->fd, EPOLL_CTL_MOD, fd, &epoll_event);
		if (ret == -1)
		{
			logger(LOG_ERROR,"failed to modify fd(=%d) events to %d", fd,
					epoll_event.events);
		}
	}
unlock: pthread_mutex_unlock(&event_pool->mutex);

		return ret;
}

static void *event_dispatch_start(void *data)
{
	event_pool_t *eventp = (event_pool_t *)data;
	event_dispatch_epoll(eventp);
	return NULL;
}
static int event_dispatch_epoll(event_pool_t *event_pool)
{
	event_pool_t *event_poll = NULL;
	int size = 0;
	int i = 0;
	int ret = -1;

	//event_poll = (event_pool_t *) data;
	if (event_pool == NULL)
	{
		logger(LOG_ERROR,"invalid argument");
		return -1;
	}

	while (1)
	{
		pthread_mutex_lock(&event_pool->mutex);
		{
			while (list_empty(event_pool->head))
				pthread_cond_wait(&event_pool->cond, &event_pool->mutex);

			if (list_size(event_pool->head) > event_pool->count)
			{
				/*
				   event_pool->count += 100;
				   event_pool->events = (struct epoll_event *) relloc(
				   event_pool->count * sizeof(struct epoll_event));
				   event_pool->event_size = list_size(event_pool->head);
				   */
				pthread_mutex_unlock(&event_pool->mutex);
				break;
			}
		}
		pthread_mutex_unlock(&event_pool->mutex);
		ret = epoll_wait(event_pool->fd, event_pool->events,
				event_pool->events_size, -1);
		if (ret == 0)
			/* timeout */
			continue;

		if (ret == -1 && errno == EINTR)
			/* sys call */
			continue;

		size = ret;
		for (i = 0; i < size; i++)
		{
			if (!event_pool->events[i].events)
				continue;
			ret = event_dispatch_epoll_handler(event_pool, i);
		}
	}
	return -1;
}

static int event_dispatch_epoll_handler(struct event_pool *event_pool, int i)
{
	event_handler_t handler = NULL;
	void *data = NULL;
	int fd = -1;
	int ret = -1;
	struct list_node *tmp;
	register_info_t *reg_info;

	handler = NULL;
	data = NULL;

	pthread_mutex_lock(&event_pool->mutex);
	{
		list_for_each(tmp, event_pool->head)
		{
			reg_info = list_entry(tmp, register_info_t, node);
			if (reg_info->fd == event_pool->events[i].data.fd)
			{
				handler = reg_info->handler;
				fd = reg_info->fd;
			}
		}
	}
unlock: pthread_mutex_unlock(&event_pool->mutex);

		if (handler)
			ret = handler(fd, data,
					(event_pool->events[i].events & (EPOLLIN | EPOLLPRI)),
					(event_pool->events[i].events & (EPOLLOUT)),
					(event_pool->events[i].events & (EPOLLERR | EPOLLHUP)));
		return ret;
}

static int sock_event_handler(int fd, void *data, int poll_in, int poll_out, int poll_err)
{
	network_t *self = NULL;
	socket_info_t *priv = NULL;
	int ret = 0;

	self = (network_t *) data;
	priv = (socket_info_t *) self->sockinfo;
	if (!priv->connected)
	{
		ret = sock_connect_finish(self);
	}
	if (!ret && poll_out)
	{
		ret = sock_event_poll_out(self);
	}
	if (!ret && poll_in)
	{
		ret = sock_event_poll_in(self);
	}
	if (ret < 0 || poll_err)
	{
		sock_event_poll_err(self);
	}
	return 0;
}

static int sock_server_event_handler(int fd, void *data, int poll_in, int poll_out, int poll_err)
{
	network_t *self = NULL;
	socket_info_t *priv = NULL;
	int ret = 0;
	int new_sock = -1;
	network_t *new_client = NULL;
	struct sockaddr_in  new_sockaddr = { 0, };
	socklen_t addrlen = sizeof(new_sockaddr);
	socket_info_t *new_priv = NULL;

	self = (network_t *) data;
	priv = (socket_info_t *) self->sockinfo;

	LOCK (&priv->lock);
	{
		if (poll_in)
		{
			new_sock = accept(priv->sock, (struct sockaddr *)&new_sockaddr, &addrlen);
			if (new_sock == -1)
				goto unlock;
			ret = sock_nonblock(new_sock);
			if (ret == -1)
			{
				close(new_sock);
				goto unlock;
			}
			ret = sock_nodelay(new_sock);
			if (ret == -1)
			{
				logger(LOG_ERROR,"setsockopt() failed for NODELAY (%s)", strerror(errno));
			}

			ret = sock_keepalive(new_sock, priv->keepaliveintvl);
			if (ret == -1)
				logger(LOG_ERROR,"Failed to set keep-alive: %s", strerror(errno));
			new_client = (network_t *) malloc(sizeof(network_t));
			//		memcpy(new_client->ip, inet_ntoa(new_sockaddr.sin_addr.s_addr),sizeof(int));
			new_client->ip_type = IS_IPV4;
			new_client->port = ntohs(new_sockaddr.sin_port);
			ret = getsockname(new_sock, (struct sockaddr *)&new_sockaddr, &addrlen);
			if (ret == -1)
			{
				close(new_sock);
				goto unlock;
			}
			sock_init(new_client);
			new_client->ops = self->ops;
			new_client->callback = self->callback;
			new_client->event_pool = self->event_pool;

			new_priv = (socket_info_t *) new_client->sockinfo;

			self->callback(self, NETWORK_CBK_NEW, new_client, 0);

			LOCK (&new_priv->lock);
			{
				new_priv->sock = new_sock;
				new_priv->connected = 1;
				ret = event_pool_register(self->event_pool, new_sock,
						sock_event_handler, new_client, 1, 0);
				if (ret < 0)
				{
					logger(LOG_ERROR,"new client event register error");
				}
			}
			UNLOCK (&new_priv->lock);
		}
	}
unlock: UNLOCK (&priv->lock);
		return ret;
}

static int sock_event_poll_err(network_t *self)
{
	socket_info_t *priv = NULL;
	int ret = -1;

	priv = (socket_info_t *) self->sockinfo;
	LOCK (&priv->lock);
	{
		sock_msg_flush(self);
		sock_reset(self);
	}
	UNLOCK (&priv->lock);

	self->callback(self, NETWORK_CBK_CLOSE, NULL, 0);
	return ret;
}

static int sock_event_poll_out(network_t *self)
{
	socket_info_t *priv = NULL;
	int ret = -1;

	priv = (socket_info_t *) self->sockinfo;
	LOCK (&priv->lock);
	{
		if (priv->connected == 1)
		{
			ret = sock_msg_churn(self);
			if (ret == -1)
			{
				sock_disconnect(self);
				self->callback(self, NETWORK_CBK_CLOSE, NULL, 0);
			}
		}
	}
	UNLOCK (&priv->lock);
	return ret;
}

static int sock_validate_header(network_t *self, struct socket_header *header, size_t *size1_p)
{
	size_t size1 = 0;

	socket_info_t *priv = self->sockinfo;

	if (memcmp(header->name, "yue-soft", 8))
	{
		logger(LOG_ERROR,"socket proto validate,name error");
		return -1;
	}

	//size1 = ntoh32(header->size);
	size1 = header->size;
	if (size1 <= 8 || size1 > 10485760)
	{
		logger(LOG_ERROR,"socket proto validate,length error");
		return -1;
	}

	if (size1_p)
		*size1_p = size1;

	return 0;
}

static int sock_proto_state_machine(network_t *self)
{
	int ret = -1;
	socket_info_t *priv = NULL;
	size_t size1 = 0;
	int previous_state = -1;
	struct socket_header *hdr = NULL;
	char buf[16] = "";
	char read_buf[8];

	priv = (socket_info_t *) self->sockinfo;
	LOCK (&priv->lock);
	while (priv->recv.state != SOCKET_COMPLETE)
	{
		/* avoid infinite loops */
		if (previous_state == priv->recv.state)
		{
			ret = 1;
			goto unlock;
		}

		previous_state = priv->recv.state;

		switch (priv->recv.state)
		{
		case SOCKET_NADA:
			priv->recv.size = 12;
			priv->recv.pos = 0;
			priv->recv.state = SOCKET_HEADER_COMING;
			break;

		case SOCKET_HEADER_COMING:
			ret = sock_read_data(self, priv->recv.head_buf + priv->recv.pos,
					priv->recv.size - priv->recv.pos);

			if (ret < 0)
			{
				goto unlock;
			}
			else if (ret == 0)
			{
				memcpy(priv->recv.header.name, priv->recv.head_buf, 8);
				memcpy(&priv->recv.header.size, priv->recv.head_buf + 8,
						sizeof(int));
				priv->recv.state = SOCKET_HEADER_CAME;
				memset(priv->recv.head_buf, '\0', 12);
			}
			else
			{
				ret = -1;
				goto unlock;
			}
			break;

		case SOCKET_HEADER_CAME:
			hdr = &priv->recv.header;
			ret = sock_validate_header(self, hdr, &size1);
			if (ret == -1)
			{
				logger(LOG_ERROR,"socket header validate failed");
				goto unlock;
			}

			priv->recv.size = size1;
			priv->recv.pos = 0;
			priv->recv.buf = (char *) malloc(sizeof(char) * size1);
			priv->recv.state = SOCKET_DATA_COMING;
			break;

		case SOCKET_DATA_COMING:
			ret = sock_read_data(self, priv->recv.buf + priv->recv.pos,
					priv->recv.size - priv->recv.pos);
			if (ret == 0)
			{
				priv->recv.state = SOCKET_DATA_CAME;
				break;
			}

			else if (ret == -1)
			{
				goto unlock;
			}
			else
			{
				ret = -1;
				goto unlock;
			}
			break;

		case SOCKET_DATA_CAME:
			priv->recv.state = SOCKET_COMPLETE;
			break;
		default:
			goto unlock;
			break;
		}
	}
unlock: UNLOCK (&priv->lock);
		return ret;
}

static int sock_event_poll_in(network_t *self)
{
	int ret = -1;
	socket_info_t *priv = NULL;
	priv = (socket_info_t *) self->sockinfo;
	int size = 0;
	char* buf = NULL;

	ret = sock_proto_state_machine(self);

	if (ret == 0)
	{

		priv = (socket_info_t *) self->sockinfo;

		size = priv->recv.size;
		buf = priv->recv.buf;
		memset(&priv->recv, 0, sizeof(priv->recv));
		priv->recv.state = SOCKET_NADA;
		self->callback(self, NETWORK_CBK_DATA, buf, size);
		free(buf);
	}

	return ret;
}

static int sock_init(network_t *self)
{
	socket_info_t *priv = NULL;

	priv = (socket_info_t *) malloc(sizeof(socket_info_t));
	LOCK_INIT (&priv->lock);
	priv->sock = -1;
	priv->connected = -1;

	priv->send_list = (struct list_node *) malloc(sizeof(struct list_node));
	list_init(priv->send_list);

	priv->nodelay = 1;
	priv->windowsize = DEFAULT_SOCKET_WINDOW_SIZE;
	priv->keepalive = 1;
	priv->keepaliveintvl = 5;

	self->sockinfo = priv;
	return 0;
}

static void sock_reset(network_t *self)
{
	socket_info_t *priv = NULL;
	priv = (socket_info_t *) self->sockinfo;

	if (priv->recv.buf)
	{
		free(priv->recv.buf);
		priv->recv.buf = NULL;
	}
	memset(&priv->recv, 0, sizeof(priv->recv));

	event_pool_unregister(self->event_pool, priv->sock);
	close(priv->sock);
	priv->sock = -1;
	priv->connected = -1;
}

static int sock_nonblock(int fd)
{
	int flags = 0;
	int ret = -1;

	flags = fcntl(fd, F_GETFL);
	if (flags != -1)
		ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	return ret;
}
static int sock_keepalive (int fd, int keepalive_intvl)
{
	int     on = 1;
	int     ret = -1;

	ret = setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof (on));
	if (ret == -1)
		goto ret_err;

	ret = setsockopt (fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_intvl, sizeof (keepalive_intvl));
	if (ret == -1)
		goto ret_err;

	ret = setsockopt (fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof (keepalive_intvl));
	if (ret == -1)
		goto ret_err;
ret_err:
	return ret;
}

static int sock_nodelay(int fd)
{
	int on = 1;
	int ret = -1;

	ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
	return ret;
}

static int sock_server_bind(network_t *self)
{
	struct sockaddr_in s_add;
	socklen_t sockaddr_len;
	int ret = -1;
	int opt = 1;
	socket_info_t *priv;

	bzero(&s_add, sizeof(struct sockaddr_in));
	s_add.sin_family = AF_INET;
	//s_add.sin_addr.s_addr=htonl(INADDR_ANY);
	s_add.sin_addr.s_addr = inet_addr(self->ip);
	s_add.sin_port = htons(self->port);

	priv = (socket_info_t *) self->sockinfo;
	ret = setsockopt(priv->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1)
	{
		logger(LOG_ERROR,"setsockopt() for SO_REUSEADDR failed (%s)", strerror(errno));
	}

	sockaddr_len = sizeof(struct sockaddr);
	ret = bind(priv->sock, (struct sockaddr *) &s_add, sockaddr_len);
	if (ret == -1)
	{
		logger(LOG_ERROR,"binding to failed: %s", strerror(errno));
	}
	else if (errno == EADDRINUSE)
	{
		logger(LOG_ERROR,"Port is already in use");
	}

	return ret;
}

static int sock_listen(network_t *self)
{
	socket_info_t *priv = NULL;
	int ret = -1;

	sock_init(self);
	priv = (socket_info_t *) self->sockinfo;
	LOCK (&priv->lock);
	{
		if (priv->sock != -1)
		{
			goto unlock;
		}
		priv->sock = socket(AF_INET, SOCK_STREAM, 0);
		if (priv->sock == -1)
		{
			logger(LOG_ERROR,"socket creation failed (%s)", strerror(errno));
			goto unlock;
		}
		if (setsockopt(priv->sock, SOL_SOCKET, SO_RCVBUF, &priv->windowsize,
					sizeof(priv->windowsize)) < 0)
		{
			logger(LOG_ERROR,"setting receive window size failed: %d: %d: %s",
					priv->sock, priv->windowsize, strerror(errno));
		}

		if (setsockopt(priv->sock, SOL_SOCKET, SO_SNDBUF, &priv->windowsize,
					sizeof(priv->windowsize)) < 0)
		{
			logger(LOG_ERROR,"setting send window size failed: %d: %d: %s", priv->sock,
					priv->windowsize, strerror(errno));
		}

		ret = sock_nodelay(priv->sock);
		if (ret == -1)
		{
			logger(LOG_ERROR,"setsockopt() failed for NODELAY (%s)", strerror(errno));
		}

		ret = sock_server_bind(self);
		if (ret == -1)
		{
			close(priv->sock);
			priv->sock = -1;
			goto unlock;
		}

		ret = listen(priv->sock, 100);

		if (ret == -1)
		{
			logger(LOG_ERROR,"could not set socket %d to listen mode (%s)", priv->sock,
					strerror(errno));
			close(priv->sock);
			priv->sock = -1;
			goto unlock;
		}

		ret = event_pool_register(self->event_pool, priv->sock,
				sock_server_event_handler, self, 1, 0);
		if (ret < -1)
		{
			ret = -1;
			close(priv->sock);
			priv->sock = -1;
			goto unlock;
		}
	}
unlock: UNLOCK (&priv->lock);
		return ret;
}

static int sock_connect(network_t *self)
{
	int ret = -1;
	socket_info_t *priv = NULL;
	struct sockaddr_in addr;
	socklen_t len = 0;
	sa_family_t sa_family = { 0, };

	sock_init(self);
	priv = (socket_info_t *) self->sockinfo;

	LOCK (&priv->lock);
	{
		priv->sock = socket(AF_INET, SOCK_STREAM, 0);

		if (priv->sock == -1)
		{
			logger(LOG_ERROR,"socket creation failed (%s)", strerror(errno));
			goto unlock;
		}

		if (setsockopt(priv->sock, SOL_SOCKET, SO_RCVBUF, &priv->windowsize,
					sizeof(priv->windowsize)) < 0)
		{
			logger(LOG_ERROR,"setting receive window size failed: %d: %d: "
					"%s", priv->sock, priv->windowsize, strerror(errno));
		}

		if (setsockopt(priv->sock, SOL_SOCKET, SO_SNDBUF, &priv->windowsize,
					sizeof(priv->windowsize)) < 0)
		{
			logger(LOG_ERROR,"setting send window size failed: %d: %d: %s", priv->sock,
					priv->windowsize, strerror(errno));
		}

		ret = sock_nodelay(priv->sock);
		if (ret == -1)
		{
			logger(LOG_ERROR,"setsockopt() failed for NODELAY (%s)", strerror(errno));
		}

		ret = sock_keepalive(priv->sock, priv->keepaliveintvl);
		if (ret == -1)
		{
			logger(LOG_ERROR,"Failed to set keep-alive: %s", strerror(errno));
		}

		bzero(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(self->port);
		addr.sin_addr.s_addr = inet_addr(self->ip);

		ret = connect(priv->sock, (struct sockaddr *)&addr, sizeof(struct sockaddr));
		if (ret == -1 && errno != EINPROGRESS)
		{
			logger(LOG_ERROR,"connection attempt failed (%s)", strerror(errno));
			close(priv->sock);
			priv->sock = -1;
			goto unlock;
		}
		else
		{
			priv->connected = 1;
			ret = 0;
		}

		ret = sock_nonblock(priv->sock);
		if (ret == -1)
		{
			logger(LOG_ERROR,"NBIO on %d failed (%s)", priv->sock, strerror(errno));
			close(priv->sock);
			priv->sock = -1;
			goto unlock;
		}

		ret = event_pool_register(self->event_pool, priv->sock,
				sock_event_handler, self, 1, 1);
		if (ret < 0)
		{
			logger(LOG_ERROR,"event_register failed");
			UNLOCK (&priv->lock);
			ret = -1;
		}
	}
unlock: UNLOCK (&priv->lock);
err: return ret;
}

static int sock_connect_state(int fd)
{
	int ret = -1;
	int optval = 0;
	socklen_t optlen = sizeof(int);

	ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *) &optval, &optlen);
	if (ret == 0 && optval)
	{
		errno = optval;
		ret = -1;
	}

	return ret;
}

static int sock_connect_finish(network_t *self)
{
	int ret = -1;
	socket_info_t *priv = NULL;
	struct sockaddr addr;
	socklen_t len;

	priv = (socket_info_t *) self->sockinfo;
	LOCK (&priv->lock);

	if (priv->connected == 1)
		goto unlock;

	ret = sock_connect_state(priv->sock);
	if (ret == -1 && errno == EINPROGRESS)
		ret = 1;
	if (ret == -1 && errno != EINPROGRESS)
	{
		sock_disconnect(self);
		self->callback(self, NETWORK_CBK_CLOSE, NULL, 0);
		goto unlock;
	}

	if (ret == 0)
	{
		bzero(&addr, sizeof(addr));
		ret = getsockname(priv->sock, &addr, &len);
		if (ret == -1)
		{
			sock_disconnect(self);
			self->callback(self, NETWORK_CBK_CLOSE, NULL, 0);
			goto unlock;
		}

		priv->connected = 1;
	}

unlock: UNLOCK (&priv->lock);
		return 0;
}

static sock_msg_t *sock_msg_new(network_t *self, char *buf, int len)
{
	socket_info_t *priv = NULL;
	sock_msg_t *entry = NULL;

	priv = (socket_info_t *) self->sockinfo;

	entry = (sock_msg_t *) malloc(sizeof(sock_msg_t));
	if (!entry)
		return NULL;

	memcpy(entry->header.name, "yue-soft", 8);

	//entry->header.size = hton32(len);
	entry->header.size = len;
	entry->buf = malloc(len + 12);
	if (!entry->buf)
	{
		free(entry);
		return NULL;
	}
	memcpy(entry->buf, entry->header.name, 8);
	memcpy(entry->buf + 8, &entry->header.size, 4);
	memcpy(entry->buf + 12, buf, len);
	entry->pos = 0;
	entry->size = len + 12;
	entry->state = SOCKET_DATA_SENDING;
	list_init(&entry->node);
	return entry;
}

static void sock_msg_flush(network_t *self)
{
	socket_info_t *priv = NULL;
	sock_msg_t *entry = NULL;
	struct list_node *temp = NULL;

	priv = (socket_info_t *) self->sockinfo;
	while (!list_empty(priv->send_list))
	{
		temp = priv->send_list->next;
		entry = list_entry(temp, sock_msg_t, node);
		sock_msg_entry_free(entry);
	}
	return;
}

static int sock_msg_churn(network_t *self)
{
	socket_info_t *priv = NULL;
	int ret = 0;
	sock_msg_t *entry = NULL;
	struct list_node *temp;

	priv = (socket_info_t *) self->sockinfo;

	while (!list_empty(priv->send_list))
	{
		temp = priv->send_list->next;
		entry = list_entry(temp, sock_msg_t, node);
		ret = sock_msg_send_entry(self, entry);
		if (ret != 0)
		{
			break;
		}
	}

	if (list_empty(priv->send_list))
	{
		/* all pending writes done, not interested in POLLOUT */
		ret = event_pool_select(self->event_pool, priv->sock, -1, 0);
	}
	return ret;
}

static int sock_disconnect(network_t *self)
{
	socket_info_t *priv = NULL;
	int ret = -1;

	priv = (socket_info_t *) self->sockinfo;
	if (priv->sock != -1)
	{
		ret = shutdown(priv->sock, SHUT_RDWR);
		priv->connected = -1;
	}
	return ret;
}

static ssize_t sock_writen(int fd, const void *ptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;

	nleft = n;
	while (nleft > 0)
	{
		if ((nwritten = write(fd, ptr, nleft)) < 0)
		{

			if (errno == EWOULDBLOCK || errno == EAGAIN)
				continue;
			else
			{
				return -1;
			}
		}
		else if (nwritten == 0)
		{

			break;
		}
		nleft -= nwritten;

		ptr += nwritten;
	}
	return (n - nleft); /* return >= 0 */
}

static ssize_t sock_readn(int fd, void *ptr, size_t n)
{
	size_t nleft;
	ssize_t nread;

	nleft = n;
	while (nleft > 0)
	{
		if ((nread = read(fd, ptr, nleft)) < 0)
		{

			if (errno == EWOULDBLOCK || errno == EAGAIN)
				continue;
			else
				return -1;
		}
		else if (nread == 0)
		{

			break; /* EOF */
		}
		nleft -= nread;

		ptr += nread;
	}
	return (n - nleft); /* return >= 0 */
}

static int sock_write_data(network_t *self, void *buf, int size)
{
	int ret = -1;
	socket_info_t *priv = self->sockinfo;
	int sock = priv->sock;

	ret = sock_writen(sock, buf, size);
	return ret;
}

static int sock_read_data(network_t *self, void *buf, int size)
{
	int ret = -1;
	socket_info_t *priv = self->sockinfo;
	int sock = priv->sock;

	ret = sock_readn(sock, buf, size);
	if (ret < 0)
	{
		return -1;
	}
	else
	{
		priv->recv.pos += ret;
	}
	return priv->recv.size - priv->recv.pos;
}

static void sock_msg_entry_free(sock_msg_t *entry)
{
	list_remove(&entry->node);
	free(entry->buf);
	free(entry);
}

static int sock_msg_send_entry(network_t *self, sock_msg_t *entry)
{
	int ret = -1;
	int state = SOCKET_NONE;
	int n = 0;

	while (entry->state != SOCKET_OK)
	{
		if (state == entry->state)
		{
			ret = 1;
			goto complete;
		}
		state = entry->state;
		switch (entry->state)
		{
		case SOCKET_NADA:
			break;
		case SOCKET_OK:
			goto complete;
		case SOCKET_DATA_SENDING:
			n = sock_write_data(self, entry->buf + entry->pos,
					entry->size - entry->pos);
			if (n < 0)
				return -1;
			else
				entry->pos += n;
			ret = entry->size - entry->pos;
			if (ret == 0)
			{
				entry->state = SOCKET_DATA_SEND;
			}
			break;

		case SOCKET_DATA_SEND:
			entry->state = SOCKET_OK;
			ret = 0;
			break;
		default:
			break;
		}
	}
complete: if (entry->state == SOCKET_OK)
		  {
			  sock_msg_entry_free(entry);
		  }
		  return ret;
}

static int sock_submit(network_t *self, char *buf, int len)
{
	socket_info_t *priv = NULL;
	int ret = -1;
	char need_poll_out = 0;
	char need_append = 1;
	char *tmp_buf = NULL;
	sock_msg_t *entry = NULL;

	priv = (socket_info_t *) self->sockinfo;

	LOCK (&priv->lock);
	{
		if (priv->connected != 1)
		{
			logger(LOG_ERROR,"No connection, cann't send data\n");
			goto unlock;
		}

		tmp_buf = (char *) malloc(len * sizeof(char));
		memcpy(tmp_buf, buf, len);
		entry = sock_msg_new(self, tmp_buf, len);
		if (!entry)
		{
			goto unlock;
		}
		if (list_empty(priv->send_list))
		{
			//list is empty to do
			ret = sock_msg_send_entry(self, entry);
			if (ret == 0)
				need_append = 0;
			if (ret > 0)
				need_poll_out = 1;
		}
		if (need_append)
		{
			list_insert_tail(&entry->node, priv->send_list);
			ret = 0;
		}
		if (need_poll_out)
		{
			ret = event_pool_select(self->event_pool, priv->sock, -1, 1);
		}
	}
unlock: if (tmp_buf)
			free(tmp_buf);
		UNLOCK (&priv->lock);
		return ret;
}

static void clearup_network(network_t* self)
{
	LOCK_DESTROY(&self->lock);
	if (self->ops != NULL)
	{
		free(self->ops);
		self->ops = NULL;
	}
	if (self != NULL)
	{
		free(self);
		self = NULL;
	}
}

static network_t *network_load(void)
{
	struct network *self = NULL;
	struct network_ops *ops = NULL;

	self = (struct network *) malloc(sizeof(struct network));
	ops = (struct network_ops *) malloc(sizeof(struct network_ops));
	self->ops = ops;
	self->ops->submit = sock_submit;
	self->ops->connect = sock_connect;
	self->ops->listen = sock_listen;
	self->ops->disconnect = sock_disconnect;
	self->event_pool = event_pool_create(100);
	LOCK_INIT(&self->lock);
	return self;
}

static int32_t network_listen(network_t *self)
{
	int ret = -1;
	ret = self->ops->listen(self);
	return ret;
}

static int32_t network_disconnect(network_t *self)
{
	int32_t ret = -1;
	ret = self->ops->disconnect(self);
	return ret;
}



network_t *network_server_config(int ip_type, char* addr, int16_t port,
		network_cbk_t callback)
{
	network_t *self = NULL;
	int ret = -1;

	self = network_load();

	self->role = NETWORK_SERVER;
	memset(self->ip, 0, 128);
	memcpy(self->ip, addr, strlen(addr));
	self->ip_type = ip_type;
	self->port = port;
	self->role = NETWORK_SERVER;
	self->callback = callback;

	ret = network_listen(self);
	if (ret)
	{
		clearup_network(self);
	}

	if (signal(SIGPIPE, write_err_fd) == SIG_ERR)
	{
		clearup_network(self);
	}
	return self;
}

void network_server_close(network_t *self)
{
	pthread_mutex_lock(&self->event_pool->mutex);
	event_pool_unregister(self->event_pool,
			((socket_info_t *) self->sockinfo)->sock);
	pthread_mutex_lock(&self->event_pool->mutex);
	network_disconnect(self);
	clearup_network(self);
}

int32_t network_send(network_t *self, char *buf, int32_t len)
{
	int32_t ret = -1;
	ret = self->ops->submit(self, buf, len);
	return ret;
}

network_t *network_client_connect(int ip_type, char* addr, int16_t port, network_cbk_t callback)
{
	network_t *self = NULL;
	int ret = -1;

	self = network_load();

	self->port = port;
	self->callback = callback;
	strcpy(self->ip, addr);
	self->ip_type = ip_type;
	self->port = port;
	self->role = NETWORK_CLIENT;

	ret = self->ops->connect(self);

	if (ret)
	{
		clearup_network(self);
		self = NULL;
	}
	if (signal(SIGPIPE, write_err_fd) == SIG_ERR)
	{
		clearup_network(self);
		self = NULL;
	}

	return self;
}

int32_t network_client_close(network_t *self)
{
	pthread_mutex_lock(&self->event_pool->mutex);
	event_pool_unregister(self->event_pool,
			((socket_info_t *) self->sockinfo)->sock);
	pthread_mutex_unlock(&self->event_pool->mutex);
	network_disconnect(self);
	clearup_network(self);
	return -1;
}
