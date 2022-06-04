#pragma once

#include <unordered_map>
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <cassert>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <cstring>

#include "ThreadPool.hpp"
#include "Epoller.hpp"
#include "HttpConn.hpp"

class WebServer {
    char *srcDir;
    int port;
    bool isClosed;
    int listenFd;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unique_ptr<Epoller> epoller;
    std::unordered_map<int, std::shared_ptr<HttpConn>> clients;
    std::unique_ptr<ThreadPool> threadpool;

    static const int MAX_FD = 65536;

    void handleRead(const std::shared_ptr<HttpConn> &);
    void handleWrite(const std::shared_ptr<HttpConn> &);
    void handleAccept();

    void closeConn(const std::shared_ptr<HttpConn> &);
    void onRead(const std::shared_ptr<HttpConn> &);
    void onWrite(const std::shared_ptr<HttpConn> &);
    void onProcess(const std::shared_ptr<HttpConn> &);

    void sendError(int fd, const char *info);

    int setFdNonblock(int fd);
    void addClient(int fd, sockaddr_in addr);

public:
    WebServer(int port);
    ~WebServer();

    bool initSocket();
    void start();
};
WebServer::WebServer(int port) : port(port), isClosed(false), threadpool(std::make_unique<ThreadPool>(20)), epoller(std::make_unique<Epoller>()) {
    srcDir = getcwd(nullptr, 256);
    assert(srcDir);
    strncat(srcDir, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir;
    initSocket();
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;

    listenEvent_ |= EPOLLET;
    connEvent_ |= EPOLLET;
}
WebServer::~WebServer() {
    close(listenFd);
    isClosed = true;
    free(srcDir);
}
void WebServer::start() {
    int timeMS = -1;
    while (!isClosed) {
        int eventCount = epoller->wait(timeMS);
        for (int i = 0; i < eventCount; i++) {
            int fd = epoller->getEventFd(i);
            auto events = epoller->getEvents(i);
            if (fd == listenFd) {
                handleAccept();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(clients.count(fd) > 0);
                closeConn(clients[fd]);
            } else if (events & EPOLLIN) {
                assert(clients.count(fd) > 0);
                handleRead(clients[fd]);
            } else if (events & EPOLLOUT) {
                assert(clients.count(fd) > 0);
                handleWrite(clients[fd]);
            } else {
                // LOG ERROR
            }
        }
    }
}
bool WebServer::initSocket() {
    int ret;
    sockaddr_in addr{};
    if (port > 65535 || port < 1024) {
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    struct linger optLinger = {0};

    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        return false;
    }

    ret = setsockopt(listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(listenFd);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
    if (ret == -1) {
        close(listenFd);
        return false;
    }

    ret = bind(listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    if (ret < 0) {
        close(listenFd);
        return false;
    }

    ret = listen(listenFd, 6);
    if (ret < 0) {
        close(listenFd);
        return false;
    }
    ret = epoller->addFd(listenFd, EPOLLET | EPOLLIN);
    if (ret == 0) {
        close(listenFd);
        return false;
    }
    setFdNonblock(listenFd);
    return true;
}
int WebServer::setFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
void WebServer::closeConn(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    epoller->delFd(client->GetFd());
    client->Close();
    // clients.erase(client->GetFd());
}
void WebServer::addClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    clients[fd] = std::make_shared<HttpConn>();
    clients[fd]->init(fd, addr);
    epoller->addFd(fd, EPOLLIN | connEvent_);
    setFdNonblock(fd);
}
void WebServer::handleAccept() {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd, reinterpret_cast<sockaddr *>(&addr), &len);
        if (fd <= 0)
            return;
        else if (HttpConn::userCount >= MAX_FD) {
            sendError(fd, "Server busy!");
            return;
        }
        addClient(fd, addr);
    } while (listenEvent_ & EPOLLET);
}
void WebServer::handleRead(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    threadpool->append([this, client] { onRead(client); });
}
void WebServer::onRead(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    // int ret = -1;
    int readErrno = 0;
    auto ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        closeConn(client);
        return;
    }
    onProcess(client);
}
void WebServer::onProcess(const std::shared_ptr<HttpConn> &client) {
    if (client->process()) {
        epoller->modFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        epoller->modFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}
void WebServer::handleWrite(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    threadpool->append([this, client] { onWrite(client); });
}
void WebServer::onWrite(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0) {
        if (client->IsKeepAlive()) {
            onProcess(client);
            return;
        }
    } else if (ret < 0) {
        if (writeErrno == EAGAIN) {
            epoller->modFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    closeConn(client);
}
void WebServer::sendError(int fd, const char *info) {
    assert(fd > 0);
    auto ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        // Log error
    }
    close(fd);
}
