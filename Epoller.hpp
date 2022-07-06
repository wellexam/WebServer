#pragma once

#include <sys/epoll.h> //epoll_ctl()
#include <vector>

#include "Debug.hpp"

class Epoller {
    int epollfd;
    std::vector<epoll_event> events_;

public:
    explicit Epoller(int maxEventNum = 1024);

    ~Epoller();

    bool addFd(int fd, uint32_t events);

    bool modFd(int fd, uint32_t events);

    bool delFd(int fd);

    int wait(int timeoutMs = -1);

    int getEventFd(size_t i) const;

    uint32_t getEvents(size_t i) const;
};
