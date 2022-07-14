#pragma once

#include <netinet/in.h>
#include "net/Reactor.hpp"

class HttpConn;

class Server {
    char *srcDir;
    int port;
    bool isClosed = false;
    int listenFd{};
    int timeoutMS = 600000; /* 毫秒MS */

    static const int MAX_FD = 65536;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unordered_map<int, std::shared_ptr<HttpConn>> clients;

    std::shared_ptr<Reactor> reactor;

    std::shared_ptr<Channel> acceptor;

    static void sendError(int fd, const char *info);

    static int setFdNonblock(int fd);
    void addClient(int fd, sockaddr_in addr);

    void handleAccept();
    void handleRead(const std::shared_ptr<HttpConn> &client);
    void handleWrite(const std::shared_ptr<HttpConn> &client);

    void onRead(const std::shared_ptr<HttpConn> &client);
    void onWrite(const std::shared_ptr<HttpConn> &client);
    void onProcess(const std::shared_ptr<HttpConn> &client);
    void closeConn(const std::shared_ptr<HttpConn> &client);

public:
    Server(int _port, int _threadNum, int _timeoutMS = 60000);
    ~Server();

    bool initSocket();
    void start();
};