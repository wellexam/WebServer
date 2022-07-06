#include "WebServer.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <cstring>
#include <sys/timerfd.h>
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <cassert>
#include <cerrno>

WebServer::WebServer(int _port, int _threadNum, int _timeoutMS) :
    port(_port), isClosed(false), threadpool(std::make_unique<ThreadPool>(_threadNum)), epoller(std::make_unique<Epoller>()), timeoutMS(_timeoutMS), timer(std::make_unique<HeapTimer>()) {
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
#ifdef DEBUG
    int timerFd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    itimerspec new_value{timespec{1}, timespec{1}};
    timerfd_settime(timerFd, 0, &new_value, nullptr);
    epoller->addFd(timerFd, EPOLLIN);
#endif
    while (!isClosed) {
        if (timeoutMS > 0) {
            timeMS = timer->getNextTick();
            // printf("timeMS = %d\n", timeMS);
        }
        int eventCount = epoller->wait(timeMS);
        for (int i = 0; i < eventCount; i++) {
            int fd = epoller->getEventFd(i);
            auto events = epoller->getEvents(i);
            if (fd == listenFd) {
                handleAccept();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(clients.count(fd) > 0);
                // printf("main loop closed %d \n", fd);
                closeConn(clients[fd]);
            } else if (events & EPOLLIN) {
#ifdef DEBUG
                if (fd == timerFd) {
                    char buf[10];
                    read(timerFd, buf, 10);
                    timer->heap_size();
                    continue;
                }
#endif
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
    if (timeoutMS > 0) {
        timer->add(fd, timeoutMS, [this, client = clients[fd]] { closeConn(client); });
    }
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
        // printf("%d accepted\n", fd);
        addClient(fd, addr);
    } while (listenEvent_ & EPOLLET);
}
void WebServer::handleRead(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    extentTime(client);
    threadpool->append([this, client] { onRead(client); });
}
void WebServer::onRead(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    // int ret = -1;
    int readErrno = 0;
    auto ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        // printf("onRead closed %d \n", client->GetFd());
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
    extentTime(client);
    threadpool->append([this, client] { onWrite(client); });
}
void WebServer::onWrite(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    // int ret = -1;
    int writeErrno = 0;
    auto ret = client->write(&writeErrno);
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
    // printf("onWrite closed %d \n", client->GetFd());
}
void WebServer::sendError(int fd, const char *info) {
    assert(fd > 0);
    auto ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        // Log error
    }
    close(fd);
}

void WebServer::extentTime(const std::shared_ptr<HttpConn> &client) {
    assert(client);
    if (timeoutMS > 0) {
        timer->adjust(client->GetFd(), timeoutMS);
    }
}
