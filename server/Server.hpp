#pragma once
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/timerfd.h>
#endif // !_WIN32

#include "../base/Platform.hpp"
#include "../net/Reactor.hpp"
#include "../base/HeapTimer.hpp"

class HttpConn;

class Server {
    char *srcDir;
    int port;
    bool isClosed = false;
    sock_handle_t listenFd{};
    int timeoutMS; /* 毫秒MS */

    static const int MAX_FD = 65536;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unordered_map<sock_handle_t, std::shared_ptr<HttpConn>> clients;

    std::shared_ptr<Reactor> reactor;

    std::shared_ptr<Channel> acceptor;

    std::unique_ptr<HeapTimer> heapTimer;

    static void sendError(sock_handle_t fd, const char *info);

    static int setFdNonblock(sock_handle_t fd);
    void addClient(sock_handle_t fd, sockaddr_in addr);

    void handleAccept();
    void handleRead(const std::shared_ptr<HttpConn> &client);
    void handleWrite(const std::shared_ptr<HttpConn> &client);

    void onRead(const std::shared_ptr<HttpConn> &client);
    void onWrite(const std::shared_ptr<HttpConn> &client);
    void onProcess(const std::shared_ptr<HttpConn> &client);
    void closeConn(const std::shared_ptr<HttpConn> &client);

public:
    Server(int _port, int _threadNum, int _timeoutMS = 60000, bool openLog = false,
           int logLevel = 1);
    ~Server();

    bool initSocket();
    void start();
};