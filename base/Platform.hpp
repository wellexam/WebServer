#pragma once

#include <cstring>
#include <cstdio>
#include <string>

#ifdef _WIN32
#include <WinError.h>
#include <Ws2tcpip.h>
#include <cerrno>
#include <winsock.h>
#include <winsock2.h>
#include <io.h>
#include <basetsd.h>
#include <afunix.h>
typedef SSIZE_T ssize_t;
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
#define EPOLLET 0
#ifndef _S_ISTYPE
#define _S_ISTYPE(mode, mask)  (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode) _S_ISTYPE((mode), _S_IFDIR)
#endif
#define S_IROTH _S_IREAD
#else
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif // _WIN32

#ifdef _WIN32
#define YETI_ERRNO WSAGetLastError()
#define YETI_ENOTSOCK WSAENOTSOCK
#define YETI_EWOULDBLOCK WSAEWOULDBLOCK
#define YETI_EINTR WSAEINTR
#define YETI_ECONNABORTED WSAECONNABORTED
#define YETI_SOCKET_ERROR SOCKET_ERROR
#define YETI_INVALID_SOCKET INVALID_SOCKET

using YetiEpollFD = HANDLE;
using YetiSocketFD = SOCKET;

#else
#define YETI_ERRNO errno
#define YETI_ENOTSOCK EBADF
#define YETI_EWOULDBLOCK EAGAIN
#define YETI_EINTR EINTR
#define YETI_ECONNABORTED ECONNABORTED
#define YETI_SOCKET_ERROR (-1)
#define YETI_INVALID_SOCKET (-1)

using YetiEpollFD = int;
using YetiSocketFD = int;

#endif

static bool InitSocket()
{
    bool ret = true;
#ifdef _WIN32
    static WSADATA g_WSAData;
    static bool WinSockIsInit = false;
    if (WinSockIsInit)
    {
        return true;
    }
    if (WSAStartup(MAKEWORD(2, 2), &g_WSAData) == 0)
    {
        WinSockIsInit = true;
    }
    else
    {
        ret = false;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    return ret;
}

static void DestroySocket()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static int SocketNodelay(YetiSocketFD fd)
{
    const int flag = 1;
    return ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*) &flag, sizeof(flag));
}

static bool SocketBlock(YetiSocketFD fd)
{
    int err;
    unsigned long ul = false;
#ifdef _WIN32
    err = ioctlsocket(fd, FIONBIO, &ul);
#else
    err = ioctl(fd, FIONBIO, &ul);
#endif

    return err != YETI_SOCKET_ERROR;
}

static bool SocketNonblock(YetiSocketFD fd)
{
    int err;
    unsigned long ul = true;
#ifdef _WIN32
    err = ioctlsocket(fd, FIONBIO, &ul);
#else
    err = ioctl(fd, FIONBIO, &ul);
#endif

    return err != YETI_SOCKET_ERROR;
}

static int SocketSetSendSize(YetiSocketFD fd, int sd_size)
{
    return ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*) &sd_size, sizeof(sd_size));
}

static int SocketSetRecvSize(YetiSocketFD fd, int rd_size)
{
    return ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*) &rd_size, sizeof(rd_size));
}

static int SocketSetReuseAddr(YetiSocketFD fd)
{
#ifdef _WIN32
    BOOL enable = true;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(enable));
#else
    int enable = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#endif
}

static int SocketDisableReuseAddr(YetiSocketFD fd)
{
#ifdef _WIN32
    BOOL enable = false;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(enable));
#else
    int enable = 0;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#endif
}

static int SocketSetReusePort(YetiSocketFD fd)
{
#ifdef _WIN32
    BOOL enable = true;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(enable));
#else
    int enable = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));
#endif
}

static int SocketDisableReusePort(YetiSocketFD fd)
{
#ifdef _WIN32
    BOOL enable = false;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(enable));
#else
    int enable = 0;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));
#endif
}

static YetiSocketFD SocketCreate(int af, int type, int protocol)
{
    return ::socket(af, type, protocol);
}

static void SocketClose(YetiSocketFD fd)
{
#ifdef _WIN32
    ::closesocket(fd);
#else
    ::close(fd);
#endif
}

static YetiSocketFD Connect(const std::string& server_ip, int port)
{
    InitSocket();

    struct sockaddr_in ipAddr = sockaddr_in();
    struct sockaddr_in* paddr = &ipAddr;
    int addrLen = sizeof(ipAddr);

    YetiSocketFD clientfd = SocketCreate(AF_INET, SOCK_STREAM, 0);

    if (clientfd == YETI_INVALID_SOCKET)
    {
        return clientfd;
    }

    bool ptonResult = false;

    ipAddr.sin_family = AF_INET;
    ipAddr.sin_port = htons(port);
    ptonResult = inet_pton(AF_INET, server_ip.c_str(), &ipAddr.sin_addr) > 0;

    if (!ptonResult)
    {
        SocketClose(clientfd);
        return YETI_INVALID_SOCKET;
    }

    while (::connect(clientfd, (struct sockaddr*) paddr, addrLen) < 0)
    {
        if (EINTR == YETI_ERRNO)
        {
            continue;
        }

        SocketClose(clientfd);
        return YETI_INVALID_SOCKET;
    }

    return clientfd;
}

static YetiSocketFD Listen(const char* ip, int port, int back_num, bool enabledReusePort)
{
    InitSocket();

    struct sockaddr_in ipAddr = sockaddr_in();
    struct sockaddr_in* paddr = &ipAddr;
    int addrLen = sizeof(ipAddr);

    const auto socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == YETI_INVALID_SOCKET)
    {
        return YETI_INVALID_SOCKET;
    }

    bool ptonResult = false;

    ipAddr.sin_family = AF_INET;
    ipAddr.sin_port = htons(port);
    ipAddr.sin_addr.s_addr = INADDR_ANY;
    ptonResult = inet_pton(AF_INET, ip, &ipAddr.sin_addr) > 0;

    // default enable SO_REUSEADDR
    if (!ptonResult || SocketSetReuseAddr(socketfd) < 0)
    {
        SocketClose(socketfd);
        return YETI_INVALID_SOCKET;
    }

    if ((enabledReusePort && SocketSetReusePort(socketfd) < 0) ||
        (!enabledReusePort && SocketDisableReusePort(socketfd) < 0))
    {
        SocketClose(socketfd);
        return YETI_INVALID_SOCKET;
    }

    const int bindRet = ::bind(socketfd, (struct sockaddr*) paddr, addrLen);
    if (bindRet == YETI_SOCKET_ERROR ||
        listen(socketfd, back_num) == YETI_SOCKET_ERROR)
    {
        SocketClose(socketfd);
        return YETI_INVALID_SOCKET;
    }

    return socketfd;
}

static int SocketSend(YetiSocketFD fd, const char* buffer, int len)
{
    int transnum = ::send(fd, buffer, len, 0);
    if (transnum < 0 && YETI_EWOULDBLOCK == YETI_ERRNO)
    {
        transnum = 0;
    }

    /*  send error if transnum < 0  */
    return transnum;
}

static YetiSocketFD Accept(YetiSocketFD listenSocket, struct sockaddr* addr, socklen_t* addrLen)
{
    return ::accept(listenSocket, addr, addrLen);
}
