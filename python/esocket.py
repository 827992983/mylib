'''
Created on 2018-5-15

@author: yunify
'''
import socket
import select
import struct
import time
import errno
from log.logger import logger
SOCKET_HEADER_NAME = "qing-cloud"
SOCKET_HEADER_SIZE = len(struct.pack("=%dsi"%len(SOCKET_HEADER_NAME), SOCKET_HEADER_NAME, 0))
SOCKET_SEND_BUF_SIZE = 1024 * 1024
SOCKET_RECV_BUF_SIZE = 1024 * 1024

class HeaderMsg:
    def __init__(self, name, size):
        self._name = name
        self._size = size

    @property
    def size(self):
        return self._size
    @size.setter
    def size(self, value):
        self._size = value

    @property
    def name(self):
        return self._name

    def unpack(self):
        header = struct.unpack("=%dsi" % len(SOCKET_HEADER_NAME), self.buf)
        self._name = header[0]
        self._size = header[1]
        return header

    def pack(self):
        struct.pack("=%dsi"%len(SOCKET_HEADER_NAME), self._name, self._size)

class DataMsg:
    def __init__(self, data):
        self._header = HeaderMsg(SOCKET_HEADER_NAME, SOCKET_HEADER_SIZE)
        self._data = data

    def pack(self):
        size = len(struct.pack("=%dsi%ds"%(len(SOCKET_HEADER_NAME), len(self._data)), self._header.name, self._header.size, self._data))
        self._header.size = size
        return struct.pack("=%dsi%ds"%(len(SOCKET_HEADER_NAME), len(self._data)), self._header.name, self._header.size, self._data)

class EsocketServer:
    def __init__(self,
                 host="0.0.0.0",
                 port=9500,
                 listen_size=100,
                 timeout=1,
                 handler = None):
        self.port = port
        self.host = host
        self.listen_size = listen_size
        self.timeout = timeout
        self.running = False

        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._set_socket_opt(self.server_socket)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(self.listen_size)
        self.selector = select.epoll()
        self.selector.register(self.server_socket, select.EPOLLIN | select.EPOLLPRI | select.EPOLLET)
        
        self.clients = {}
        self.clients_ext = {}
        
        self.handler = handler

    def _set_socket_opt(self, sock):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, SOCKET_RECV_BUF_SIZE)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, SOCKET_SEND_BUF_SIZE)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 5)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 5)

    def recv(self, sock, timeout=5):
        end_time = time.time() + timeout
        head = sock.recv(SOCKET_HEADER_SIZE)
        if head is None or len(head) == 0:
            logger.info("client [%s] is closed." % str(self.clients_ext[sock.fileno()]))
            self.selector.unregister(sock.fileno())
            self.clients_ext.pop(sock.fileno())
            self.clients.pop(sock.fileno())
            sock.close()
            return None
        header_data = struct.unpack("=%dsi" % len(SOCKET_HEADER_NAME), head[:SOCKET_HEADER_SIZE])
        logger.info("read-HEAD: %s" % str(header_data))
        header = HeaderMsg(header_data[0], header_data[1])
        data_size = header.size - SOCKET_HEADER_SIZE
        data = ""
        while end_time > time.time():
            try:
                buf = sock.recv(SOCKET_RECV_BUF_SIZE)
                if buf is None or len(buf) == 0:
                    logger.info("client [%s] is closed." % str(self.clients_ext[sock.fileno()]))
                    self.selector.unregister(sock.fileno())
                    self.clients_ext.pop(sock.fileno())
                    self.clients.pop(sock.fileno())
                    sock.close()
                    return None
                data = data + buf
                if len(data) < data_size:
                    continue
                recv_data = struct.unpack("=%ds" % data_size, data)[0]
                #logger.info("read-DATA len(%d): %s" % (len(recv_data), recv_data))
                logger.info("read-DATA len [%d]" % len(recv_data))
                return recv_data
            except socket.error as err:
                logger.error("socket exception: %s" % str(err))
                if err.errno == errno.ECONNRESET or err.errno == errno.EBADF:
                    logger.info("client [%s] is closed." % str(self.clients_ext[sock.fileno()]))
                    self.selector.unregister(sock.fileno())
                    self.clients_ext.pop(sock.fileno())
                    self.clients.pop(sock.fileno())
                    sock.close()
                    return None
                if err.errno == errno.EAGAIN:
                    time.sleep(0.2)
                    continue

    def send(self, sock, data, size):
        if size != len(data):
            logger.error("data size [%d] not equal real size [%d]", size, len(data))
            return -1
        logger.info("send-DATA: %s" % data)
        msg = DataMsg(data)
        buf = msg.pack()
        try:
            ret = sock.send(buf)
            return ret - SOCKET_HEADER_SIZE
        except socket.error as err:
            logger.error("socket exception: %s" % err)
            if err.errno == errno.ECONNRESET or err.errno == errno.EBADF:
                logger.info("client [%s:%s] is closed." % self.clients_ext[sock.fileno()])
                self.selector.unregister(sock.fileno())
                self.clients_ext.pop(sock.fileno())
                self.clients.pop(sock.fileno())
                sock.close()

    def start(self):
        self.running = True
        logger.info("TCP socket server [%s:%s] start." % (self.host, self.port))
        while self.running:
            selectors = self.selector.poll()
            for fd, event in selectors:
                if fd == self.server_socket.fileno():
                    sock, addr = self.server_socket.accept()

                    # new client
                    sock.setblocking(False)
                    self._set_socket_opt(sock)
                    self.clients.update({sock.fileno(): sock})
                    self.clients_ext.update({sock.fileno(): addr})
                    self.selector.register(sock.fileno(), select.EPOLLIN | select.EPOLLPRI | select.EPOLLET)
                    logger.info("new socket client: [%s]" % str(addr))

                elif event & select.EPOLLIN:
                    try:
                        recv_data = self.recv(self.clients[fd])
                        if self.handler and recv_data:
                            self.handler.handle(self, self.clients[fd], recv_data)
                    except Exception, e:
                        logger.error("recv data and handle data with exception: %s" % e)
                        logger.info("close socket [%s]" % str(self.clients_ext[fd]))
                        #self.send(sock, "close you!", 10)
                        self.selector.unregister(fd)
                        self.clients_ext.pop(fd)
                        self.clients.pop(fd)
                        sock.close()
                elif event & select.EPOLLPRI:
                    logger.info("EPOLLPRI")
                elif event & select.EPOLLERR or event & select.EPOLLHUP:
                    logger.info("socket [%s] is error, closed it." % str(self.clients_ext[fd]))
                    self.selector.unregister(fd)
                    self.clients_ext.pop(fd)
                    sock = self.clients.pop(fd)
                    if sock:
                        sock.close()

    def stop(self):
        self.running = False
        logger.info("TCP socket server [%s:%s] stop." % (self.host, self.port))

        for fd,client in self.clients.items():
            self.selector.unregister(fd)
            self.clients_ext.pop(fd)
            client.close()
        self.server_socket.close()
        self.selector.close()


