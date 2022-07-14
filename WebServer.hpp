#pragma once

#include <unordered_map>

#include "net/ThreadPool.hpp"
#include "Epoller.hpp"
#include "HttpConn.hpp"
#include "HeapTimer.hpp"

class WebServer {
    char *srcDir;
    int port;
    bool isClosed;
    int listenFd;
    int timeoutMS; /* 毫秒MS */

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unique_ptr<Epoller> epoller;
    std::unordered_map<int, std::shared_ptr<HttpConn>> clients;
    std::unique_ptr<ThreadPool> threadpool;
    std::unique_ptr<HeapTimer> timer;

    static const int MAX_FD = 65536;

    void handleRead(const std::shared_ptr<HttpConn> &);
    void handleWrite(const std::shared_ptr<HttpConn> &);
    void handleAccept();

    void closeConn(const std::shared_ptr<HttpConn> &);
    void onRead(const std::shared_ptr<HttpConn> &);
    void onWrite(const std::shared_ptr<HttpConn> &);
    void onProcess(const std::shared_ptr<HttpConn> &);

    static void sendError(int fd, const char *info);

    int setFdNonblock(int fd);
    void addClient(int fd, sockaddr_in addr);
    void extentTime(const std::shared_ptr<HttpConn> &client);

public:
    WebServer(int _port, int _threadNum, int _timeoutMS);
    ~WebServer();

    bool initSocket();
    void start();
};