#ifndef ___FZ_TCP_SERVER_H
#define ___FZ_TCP_SERVER_H

#include "fz_common.h"
#include "fz_trace.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

FZ_BEGIN

class tcp_server {
	public:
		int init(const int port) {

			/* sock init */
			if(make_socket())
				return -1;

			int sock_opt = 1;
			if(fcntl(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&sock_opt, sizeof(sock_opt)) < 0) {
				fz::TRACE(FZ_LOG_ERROR, "sock option(REUSEADDR) failure\n");
				return -1;
			}
			if(fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
				fz::TRACE(FZ_LOG_ERROR, "sock option(nonblock) failure\n");
				return -1;
			}

			/* bind init */
			bzero((char*)&addr, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = INADDR_ANY;
			addr.sin_port = htons(port);

			int opt = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
			if(bind_socket())
				return -1;

			/* listen */
			listen(sock, 5);

			is_initialize = true;
			return 0;
		}
		int destroy() {
			int ret;

			if(sock) {
				ret = shutdown(sock, SHUT_RDWR);
				if(ret < 0) {
					fz::TRACE(FZ_LOG_ERROR, "server sock shutdown error!(%d)\n", ret);
				}
				close(sock);
				sock = 0;
			}
			if(client_sock) {
				ret = shutdown(client_sock, SHUT_RDWR);
				if(ret < 0) {
					fz::TRACE(FZ_LOG_ERROR, "client sock shutdown error!(%d)\n", ret);
				}
				close(client_sock);
				client_sock = 0;
			}

			printf("tcp shutdown done...\n");
			is_initialize = false;
			return 0;
		}

		int make_socket() {
			//if(is_initialize)
			//	return 1;
			if(sock)
				return 0;

			sock = socket(PF_INET, SOCK_STREAM, 0);
			if(sock < 0) {
				fz::TRACE(FZ_LOG_ERROR, "Cannot create socket\n");	
				switch(errno)
				{
					case EAFNOSUPPORT: fz::TRACE(FZ_LOG_ERROR, "EAFNOSUPPORT\n"); break;
					case EMFILE: fz::TRACE(FZ_LOG_ERROR, "EMFILE\n");	break;
					case ENFILE: fz::TRACE(FZ_LOG_ERROR, "ENFILE\n");	break;
					case EPROTONOSUPPORT: fz::TRACE(FZ_LOG_ERROR, "EPROTONOSUPPORT\n"); break;
					case EPROTOTYPE: fz::TRACE(FZ_LOG_ERROR, "EPROTOTYPE\n"); break;
					case EACCES: fz::TRACE(FZ_LOG_ERROR, "EACCESS\n"); break;
					case ENOBUFS: fz::TRACE(FZ_LOG_ERROR, "ENOBUFS\n"); break;
					case ENOMEM: fz::TRACE(FZ_LOG_ERROR, "ENOMEM\n"); break;
					default: fz::TRACE(FZ_LOG_ERROR, "UNKNOWN\n"); break;
				}
				return -1;
			}

			return 0;
		}
		int bind_socket() {
			if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
				fz::TRACE(FZ_LOG_ERROR, "Cannot bind socket\n");
				switch(errno)
				{
					case EADDRINUSE: fz::TRACE(FZ_LOG_ERROR, "EADDRINUSE\n"); break;
					case EADDRNOTAVAIL: fz::TRACE(FZ_LOG_ERROR, "EADDRNOTAVAIL\n"); break;
					case EAFNOSUPPORT: fz::TRACE(FZ_LOG_ERROR, "EAFNOSUPPORT\n"); break;
					case EBADF: fz::TRACE(FZ_LOG_ERROR, "EBADF\n"); break;
					case EINVAL: fz::TRACE(FZ_LOG_ERROR, "EINVAL\n"); break;
					case ENOTSOCK: fz::TRACE(FZ_LOG_ERROR, "ENOTSOCK\n"); break;
					case EOPNOTSUPP: fz::TRACE(FZ_LOG_ERROR, "EOPNOTSUPP\n"); break;
					case EACCES: fz::TRACE(FZ_LOG_ERROR, "EACCES\n"); break;
					case EDESTADDRREQ: fz::TRACE(FZ_LOG_ERROR, "EDESTADDRREQ\n"); break;
					case EISDIR: fz::TRACE(FZ_LOG_ERROR, "EISDIR\n"); break;
					case EIO: fz::TRACE(FZ_LOG_ERROR, "EIO\n"); break;
					case ELOOP: fz::TRACE(FZ_LOG_ERROR, "ELOOP\n"); break;
					case ENAMETOOLONG: fz::TRACE(FZ_LOG_ERROR, "ENAMETOOLONG\n"); break;
					case ENOENT: fz::TRACE(FZ_LOG_ERROR, "ENOENT\n"); break;
					case ENOTDIR: fz::TRACE(FZ_LOG_ERROR, "ENOTDIR\n"); break;
					case EROFS: fz::TRACE(FZ_LOG_ERROR, "EROFS\n"); break;
					case EISCONN: fz::TRACE(FZ_LOG_ERROR, "EISCONN\n"); break;
					case ENOBUFS: fz::TRACE(FZ_LOG_ERROR, "ENOBUFS\n"); break;
					default: fz::TRACE(FZ_LOG_ERROR, "UNKNOWN\n"); break;
				}
				return -1;
			}
			return 0;
		}
		int wait_client() {
			int ret;
			FD_ZERO(&sockfds);
			FD_SET(sock, &sockfds);
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			fz::TRACE(FZ_LOG_INFO, "Wait client...\n");
			ret = select(sock+1, &sockfds, (fd_set *)0, (fd_set *)0, &timeout);

			if(!ret) {
				fz::TRACE(FZ_LOG_INFO, "No connection... \n");
				return 1;
			}

			socklen_t client_addr_len = sizeof(client_addr);
			client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len);
			if(client_sock < 0) {
				return -1;
			}

			if(fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
				fz::TRACE(FZ_LOG_ERROR, "sock option(nonblock) failure\n");
				return -1;
			}

			fz::TRACE(FZ_LOG_INFO, "Connected client...\n");
			return 0;
		}

		int send_buffer(const void* p, _u32 len) {
			if(!is_initialize)
				return -1;

			fz::TRACE(FZ_LOG_INFO, "send_buffer... write %d bytes...\n", len);
			_u32 t = 0;
			while(len) {
				t = write(client_sock, (_u8*)p + t, len);
				if(t == -1) {
					/* 클라이언트 비정상 종료? */
					fz::TRACE(FZ_LOG_ERROR, "send_buffer... write failed!!\n");
					usleep(100);
					return -1;
				}
				len-=t;
				fz::TRACE(FZ_LOG_INFO, "send_buffer %d bytes send...(remain %d bytes)\n", t, len);
			}
			return 0;
		}
		int recv_buffer(const void* p, _u32 len) {
			if(!is_initialize)
				return -1;

			if(!client_sock)
				return -1;

			_u32 recv_len = 0;
			_u32 total_recv_len = 0;
			//recv_len = recv(client_sock, (_u8*)p, len, MSG_PEEK);
			/* 문제 발생시 peek으로 대체하도록 한다. */
			do {
				recv_len = recv(client_sock, (_u8*)p+total_recv_len, len-total_recv_len, 0);
				total_recv_len += recv_len;
			}
			while(total_recv_len < len);

			//if(recv_len) {
			//}
			fz::TRACE(FZ_LOG_INFO, "recv buffer...(%d) \n", total_recv_len);

			return total_recv_len == len ? 0 : -1;
		}
	public:
		tcp_server() 
			: sock(0), client_sock(0), is_initialize(false)
		{}
		~tcp_server() {
			destroy();
		}
	private:
		sockaddr_in addr;
		int sock;

		sockaddr_in client_addr;
		int client_sock;

		bool is_initialize;

		fd_set sockfds;
		struct timeval timeout;
};


FZ_END

#endif /*___FZ_TCP_SERVER_H*/
