#include "Epoll.hpp"
#include "../log/log.h"

#include <cassert>
#include <unistd.h>

const int EVENTSNUM = 4096;
const int EPOLLWAIT_TIME = 10000;

Epoll::Epoll() : epollFd(epoll_create1(EPOLL_CLOEXEC)), events_(EVENTSNUM) {
    assert(epollFd > 0);
    fd2chan_ = std::vector<SP_Channel>(MAXFDS);
}

Epoll::~Epoll() {
    close(epollFd);
}

// 注册新描述符
void Epoll::epoll_add(const SP_Channel &request) {
    assert(request);
    int fd = request->getFd();
    epoll_event event{};
    event.data.fd = fd;
    event.events = request->getEvents();

    request->EqualAndUpdateLastEvents();

    fd2chan_[fd] = request;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) < 0) {
        perror("epoll_add error");
        fd2chan_[fd].reset();
    } else {
        LOG_DEBUG("fd [%d] added to poller.", fd)
    }
}

// 修改描述符状态
void Epoll::epoll_mod(const SP_Channel &request) {
    int fd = request->getFd();
    if (!request->EqualAndUpdateLastEvents()) {
        epoll_event event{};
        event.data.fd = fd;
        event.events = request->getEvents();
        if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event) < 0) {
            perror("epoll_mod error");
            fd2chan_[fd].reset();
            LOG_ERROR("Error happened when modifying fd [%d].", fd)
        } else {
            LOG_DEBUG("fd [%d] modified", fd)
        }
    }
}

// 从epoll中删除描述符
void Epoll::epoll_del(const SP_Channel &request) {
    int fd = request->getFd();
    epoll_event event{};
    event.data.fd = fd;
    event.events = request->getLastEvents();
    if (epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, &event) < 0) {
        perror("epoll_del error");
        LOG_ERROR("Error happened when deleting fd [%d].", fd)
    } else {
        LOG_DEBUG("fd [%d] deleted", fd)
    }
    fd2chan_[fd].reset();
}

// 返回活跃事件数
std::vector<SP_Channel> Epoll::poll() {
    while (true) {
        int event_count = epoll_wait(epollFd, &*events_.begin(), events_.size(), EPOLLWAIT_TIME);
        if (event_count < 0)
            perror("epoll wait error");
        // LOG_DEBUG("polled and got %d active channels.", event_count)
        std::vector<SP_Channel> req_data;
        for (int i = 0; i < event_count; ++i) {
            // 获取有事件产生的描述符
            int fd = events_[i].data.fd;
            LOG_DEBUG("fd [%d] active", fd)

            SP_Channel cur_req = fd2chan_[fd];

            if (cur_req) {
                cur_req->setRevents(events_[i].events);
                cur_req->setEvents(0);
                req_data.push_back(cur_req);
            } else {
                // LOG << "SP cur_req is invalid";
            }
        }
        if (!req_data.empty())
            return req_data;
    }
}
std::shared_ptr<Channel> Epoll::getChannel(int fd) {
    return fd2chan_[fd];
}
