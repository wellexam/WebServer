#pragma once

#include <sys/epoll.h>
#include <sys/epoll.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class Channel {
private:
    typedef std::function<void()> CallBack;
    int fd_;
    __uint32_t events_;
    __uint32_t revents_{};
    __uint32_t lastEvents_;

private:
    CallBack readHandler_;
    CallBack writeHandler_;
    CallBack errorHandler_;
    CallBack connHandler_;
    CallBack closeHandler_;

public:
    Channel() : events_(0), lastEvents_(0), fd_(0) {}
    explicit Channel(int fd) : events_(0), lastEvents_(0), fd_(fd) {}
    ~Channel();
    int getFd() const { return fd_; }
    void setFd(int fd) { fd_ = fd; }

    void setConnHandler(CallBack &&connHandler) { connHandler_ = connHandler; }
    void setReadHandler(CallBack &&readHandler) { readHandler_ = readHandler; }
    void setWriteHandler(CallBack &&writeHandler) { writeHandler_ = writeHandler; }
    void setCloseHandler(CallBack &&closeHandler) { closeHandler_ = closeHandler; }
    void setErrorHandler(CallBack &&errorHandler) { errorHandler_ = errorHandler; }

    void handleEvents() {
        events_ = 0;
        if (revents_ & (EPOLLHUP | EPOLLRDHUP)) {
            if (closeHandler_)
                closeHandler_();
            events_ = 0;
            return;
        }
        if (revents_ & EPOLLERR) {
            if (errorHandler_)
                errorHandler_();
            events_ = 0;
            return;
        }
        if (revents_ & (EPOLLIN | EPOLLPRI)) {
            if (readHandler_) {
                readHandler_();
            } else {
                if (connHandler_) {
                    connHandler_();
                }
            }
        }
        if (revents_ & EPOLLOUT) {
            if (writeHandler_) {
                writeHandler_();
            }
        }
    }

    void setRevents(__uint32_t ev) { revents_ = ev; }

    void setEvents(__uint32_t ev) { events_ = ev; }
    __uint32_t &getEvents() { return events_; }

    bool EqualAndUpdateLastEvents() {
        bool ret = (lastEvents_ == events_);
        lastEvents_ = events_;
        return ret;
    }

    __uint32_t getLastEvents() const { return lastEvents_; }
};

typedef std::shared_ptr<Channel> SP_Channel;