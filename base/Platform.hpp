#pragma once

#ifdef _WIN32
#include <WinError.h>
#include <Ws2tcpip.h>
#include <errno.h>
#include <winsock.h>
#include <winsock2.h>
#include <io.h>
#include <BaseTsd.h>
#include <afunix.h>
typedef SSIZE_T ssize_t;
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0) 
#define STDIN_FILENO _fileno(stdin) 
#define EPOLLET 0
#ifndef _S_ISTYPE
#define _S_ISTYPE(mode, mask)  (((mode) & _S_IFMT) == (mask))
#define S_ISREG(mode) _S_ISTYPE((mode), _S_IFREG)
#define S_ISDIR(mode) _S_ISTYPE((mode), _S_IFDIR)
#endif
#define S_IROTH 0
#else
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#endif // _WIN32

#ifdef _WIN32
//typedef SOCKET YETISocketFD;
#define YETI_ERRNO WSAGetLastError()
#define YETI_ENOTSOCK WSAENOTSOCK
#define YETI_EWOULDBLOCK WSAEWOULDBLOCK
#define YETI_EINTR WSAEINTR
#define YETI_ECONNABORTED WSAECONNABORTED
#define YETI_SOCKET_ERROR SOCKET_ERROR
#define YETI_INVALID_SOCKET INVALID_SOCKET

using epoll_handle_t = HANDLE;
using sock_handle_t = SOCKET;

#else
//#define YETI_ERRNO errno
#define YETI_ENOTSOCK EBADF
#define YETI_EWOULDBLOCK EAGAIN
#define YETI_EINTR EINTR
#define YETI_ECONNABORTED ECONNABORTED
typedef int YETISocketFD;
#define YETI_SOCKET_ERROR (-1)
#define YETI_INVALID_SOCKET (-1)

using epoll_handle_t = int;
using sock_handle_t = int;

#endif
