#include "Server.hpp"

#include "HttpConn.hpp"
#include "log/log.h"

#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <sys/socket.h>
#include <thread>

Server::Server(int _port, int _threadNum, int _timeoutMS, bool openLog, int logLevel) :
    port(_port), reactor(std::make_shared<Reactor>(_threadNum)) {
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
    acceptor = std::make_shared<Channel>(listenFd);
    acceptor->setEvents(listenEvent_ | EPOLLIN);
    acceptor->setConnHandler([this] { handleAccept(); });
    reactor->addToPoller(acceptor);
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

void Server::handleRead(std::shared_ptr<HttpConn> client) {
    assert(client);
    LOG_DEBUG("handleRead() trying to append onRead() to thread pool on fd[%d]", client->GetFd())
    reactor->appendToThreadPool([this, client] { onRead(client); });
    LOG_DEBUG("handleRead() finished to append onRead() to thread pool on fd[%d]", client->GetFd())
}

void Server::onRead(std::shared_ptr<HttpConn> client) {
    assert(client);
    int readErrno = 0;
    auto ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        LOG_DEBUG("onRead() called closeConn on client[%d]", client->GetFd())
        closeConn(client);
        return;
    }
    LOG_DEBUG("onRead() called onProcess on client[%d]", client->GetFd())
    onProcess(client);
}

void Server::closeConn(std::shared_ptr<HttpConn> client) {
    assert(client);
    auto channel = reactor->getChannel(client->GetFd());
    assert(channel);
    // reactor->appendToThreadPool([this, channel, client] {
    //     LOG_DEBUG("closeConn removing fd [%d] from poller", channel->getFd())
    //     reactor->removeFromPoller(channel);
    //     client->Close();
    // });
    reactor->removeFromPoller(channel);
    client->Close();
}

void Server::onProcess(std::shared_ptr<HttpConn> client) {
    assert(client);
    if (client->process()) {
        reactor->appendToThreadPool([this, client] {
            LOG_DEBUG("onWrite append from onProcess() called on client[%d]", client->GetFd())
            onWrite(client);
        });
    } else {
        auto channel = reactor->getChannel(client->GetFd());
        channel->setEvents(connEvent_ | EPOLLIN);
        reactor->updatePoller(channel);
    }
}

void Server::handleWrite(std::shared_ptr<HttpConn> client) {
    assert(client);
    LOG_DEBUG("handleWrite on client[%d] called,trying to append onWrite() to thread pool",
              client->GetFd())
    reactor->appendToThreadPool([this, client] {
        LOG_DEBUG("onWrite append from handleWrite called on client[%d]", client->GetFd())
        onWrite(client);
    });
    LOG_DEBUG("handleWrite on client[%d] finished to append onWrite() to thread pool",
              client->GetFd())
}

void Server::onWrite(std::shared_ptr<HttpConn> client) {
    assert(client);
    int writeErrno = 0;
    auto ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0) {
        if (client->IsKeepAlive()) {
            LOG_DEBUG("onWrite() called onProcess on client[%d]", client->GetFd())
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
    closeConn(client);
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
        LOG_DEBUG("closeHandler calling closeConn on client[%d]", client->GetFd())
        closeConn(client);
    });
    reactor->addToPoller(channel);
    setFdNonblock(fd);
}

int Server::setFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

void Server::sendError(int fd, const char *info) {
    assert(fd > 0);
    auto ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        // Log error
    }
    close(fd);
}
