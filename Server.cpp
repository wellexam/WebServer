#include "Server.hpp"

#include "HttpConn.hpp"

#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <sys/socket.h>

Server::Server(int _port, int _threadNum, int _timeoutMS) :
    port(_port), reactor(std::make_shared<Reactor>(_threadNum)) {
    srcDir = getcwd(nullptr, 256);
    assert(srcDir);
    strncat(srcDir, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir;

    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;

    listenEvent_ |= EPOLLET;
    connEvent_ |= EPOLLET;
}

Server::~Server() {
    reactor->quit();
    isClosed = true;
    free(srcDir);
}

void Server::start() {
    if (!initSocket())
        perror("Socket init failed.\n");
    acceptor = std::make_shared<Channel>(listenFd);
    acceptor->setEvents(listenEvent_ | EPOLLIN);
    acceptor->setConnHandler([this] { handleAccept(); });
    reactor->addToPollerWithGuard(acceptor);
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
        addClient(fd, addr);
    } while (listenEvent_ & EPOLLET);
}

void Server::handleRead(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    reactor->appendToThreadPool([this, client] { onRead(client); });
}

void Server::onRead(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    int readErrno = 0;
    auto ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        closeConn(client);
        return;
    }
    onProcess(client);
}

void Server::closeConn(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    auto channel = reactor->getChannel(client->GetFd());
    reactor->removeFromPollerWithGuard(channel);
    reactor->addPendingTask([client] { client->Close(); });
}

void Server::onProcess(const std::shared_ptr<HttpConn> &client) {
    if (client->process()) {
        auto channel = reactor->getChannel(client->GetFd());
        channel->setEvents(connEvent_ | EPOLLOUT);
        channel->setWriteHandler([this, client] { handleWrite(client); });
        reactor->updatePollerWithGuard(channel);
    } else {
        auto channel = reactor->getChannel(client->GetFd());
        channel->setEvents(connEvent_ | EPOLLIN);
        reactor->updatePollerWithGuard(channel);
    }
}

void Server::handleWrite(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    reactor->appendToThreadPool([this, client] { onWrite(client); });
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
            channel->setWriteHandler([this, client] { handleWrite(client); });
            reactor->updatePollerWithGuard(channel);
            return;
        }
    }
    closeConn(client);
}

void Server::addClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    clients[fd] = std::make_shared<HttpConn>();
    clients[fd]->init(fd, addr);
    auto channel = std::make_shared<Channel>(fd);
    channel->setEvents(connEvent_ | EPOLLIN);
    auto client = clients[fd];
    channel->setReadHandler([this, client] { handleRead(client); });
    channel->setCloseHandler([this, client] { closeConn(client); });
    reactor->addToPollerWithGuard(channel);
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
