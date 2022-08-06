#include "Server.hpp"

#include "../http/HttpConn.hpp"
#include "../log/log.h"

#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <sys/socket.h>
#include <thread>
#include <cstdio>
#include <iostream>

Server::Server(int _port, int _threadNum, int _timeoutMS, bool openLog, int logLevel) :
    port(_port), timeoutMS(_timeoutMS), reactor(std::make_shared<Reactor>(_threadNum)),
    heapTimer(std::make_unique<HeapTimer>()) {
    srcDir = getcwd(nullptr, 256);
    assert(srcDir);
    strncat(srcDir, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir;
    HttpConn::isET = true;

    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;

    listenEvent_ |= EPOLLET;
    connEvent_ |= EPOLLET;

    if (openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", 1024);
        LOG_DEBUG("Server init finished")
    }
}

Server::~Server() {
    reactor->quit();
    isClosed = true;
    free(srcDir);
    LOG_DEBUG("Server quited.")
}

void Server::start() {
    if (!initSocket())
        perror("Socket init failed.\n");
    LOG_DEBUG("listenFd is [%d].", listenFd)

    // 设置连接接收器
    acceptor = std::make_shared<Channel>(listenFd);
    acceptor->setEvents(listenEvent_ | EPOLLIN);
    acceptor->setConnHandler([this] { handleAccept(); });
    reactor->addToPoller(acceptor);

    // 从STDIN读取quit命令
    auto cmd = std::make_shared<Channel>(STDIN_FILENO);
    cmd->setEvents(listenEvent_ | EPOLLIN);
    cmd->setReadHandler([this] {
        std::string buf;
        std::cin >> buf;
        if (buf == "quit") {
            reactor->quit();
        } else {
            std::cout << "command error" << std::endl;
        }
    });
    reactor->addToPoller(cmd);

    // 设置定时器
    int timerFd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    auto timer = std::make_shared<Channel>(timerFd);
    timer->setEvents(EPOLLIN);
    timer->setReadHandler([this, timer] {
        char buf[8];
        auto ret = read(timer->getFd(), buf, 8);
        auto nextTick = heapTimer->getNextTick();
        auto sec = nextTick / 1000;
        auto nano_sec = (nextTick % 1000) * 1000000;
        sec = std::max(sec, 1);
        itimerspec new_value{timespec{1}, timespec{sec, nano_sec}};
        timerfd_settime(timer->getFd(), 0, &new_value, nullptr);
    });
    itimerspec new_value{timespec{1}, timespec{1}};
    timerfd_settime(timerFd, 0, &new_value, nullptr);
    reactor->addToPoller(timer);

    reactor->loop();
}

bool Server::initSocket() {
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
    setFdNonblock(listenFd);
    return true;
}

void Server::handleAccept() {
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
        LOG_DEBUG("fd [%d] accepted", fd)
        addClient(fd, addr);
    } while (listenEvent_ & EPOLLET);
}

void Server::addClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    clients[fd] = std::make_shared<HttpConn>();
    clients[fd]->init(fd, addr);
    auto channel = std::make_shared<Channel>(fd);
    channel->setEvents(connEvent_ | EPOLLIN);
    auto client = clients[fd];
    assert(client);
    assert(channel);
    channel->setReadHandler([this, client] { handleRead(client); });
    channel->setCloseHandler([this, client] {
        LOG_DEBUG("closeHandler called on client[%d]", client->GetFd())
        heapTimer->disable(client->GetFd());
        closeConn(client);
    });
    reactor->addToPoller(channel);
    heapTimer->add(fd, timeoutMS, [this, client] {
        LOG_DEBUG("timeout callback() called on client[%d]", client->GetFd())
        closeConn(client);
    });
    setFdNonblock(fd);
}

void Server::handleRead(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    heapTimer->adjust(client->GetFd(), timeoutMS);
    LOG_DEBUG("handleRead() trying to append onRead() to thread pool on fd[%d]", client->GetFd())
    reactor->appendToThreadPool([this, client] { onRead(client); });
    LOG_DEBUG("handleRead() finished to append onRead() to thread pool on fd[%d]", client->GetFd())
}

void Server::onRead(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    int readErrno = 0;
    auto ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        LOG_DEBUG("onRead() called closeConn on client[%d]", client->GetFd())
        heapTimer->disable(client->GetFd());
        closeConn(client);
        return;
    }
    LOG_DEBUG("onRead() called onProcess on client[%d]", client->GetFd())
    onProcess(client);
}

void Server::closeConn(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    auto channel = reactor->getChannel(client->GetFd());
    assert(channel);
    reactor->removeFromPoller(channel);
    client->Close();
}

void Server::onProcess(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    if (client->process()) {
        reactor->addPendingTask([this, client] { heapTimer->adjust(client->GetFd(), timeoutMS); });
        reactor->appendToThreadPool([this, client] {
            LOG_DEBUG("onWrite append from onProcess() called on client[%d]", client->GetFd())
            onWrite(client);
        });
    } else {
        auto channel = reactor->getChannel(client->GetFd());
        channel->setReadHandler([this, client] { handleRead(client); });
        channel->setEvents(connEvent_ | EPOLLIN);
        reactor->updatePoller(channel);
        LOG_DEBUG("onProcess() set handleRead() on client[%d]", client->GetFd())
    }
}

void Server::handleWrite(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    heapTimer->adjust(client->GetFd(), timeoutMS);
    LOG_DEBUG("handleWrite on client[%d] called,trying to append onWrite() to thread pool",
              client->GetFd())
    reactor->appendToThreadPool([this, client] {
        LOG_DEBUG("onWrite append from handleWrite called on client[%d]", client->GetFd())
        onWrite(client);
    });
    LOG_DEBUG("handleWrite on client[%d] finished to append onWrite() to thread pool",
              client->GetFd())
}

void Server::onWrite(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    int writeErrno = 0;
    auto ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0) {
        if (client->IsKeepAlive()) {
            onProcess(client);
            return;
        }
    } else if (ret < 0) {
        if (writeErrno == EAGAIN) {
            auto channel = reactor->getChannel(client->GetFd());
            channel->setEvents(connEvent_ | EPOLLOUT);
            channel->setWriteHandler([this, client] {
                LOG_DEBUG("onWrite set WriteHandler on client[%d]", client->GetFd())
                handleWrite(client);
            });
            reactor->updatePoller(channel);
            return;
        }
    }
    LOG_DEBUG("onWrite() called closeConn on client[%d]", client->GetFd())
    heapTimer->disable(client->GetFd());
    closeConn(client);
}

int Server::setFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

void Server::sendError(int fd, const char *info) {
    assert(fd > 0);
    auto ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_ERROR("Error on fd[%d]", fd)
    }
    close(fd);
}
