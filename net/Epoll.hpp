#pragma once

#include <sys/epoll.h> //epoll_ctl()
#include <vector>
#include <memory>
#include "Channel.hpp"

class Epoll {
    int epollFd;
    std::vector<epoll_event> events_;

    static const int MAXFDS = 100000;
    std::vector<std::shared_ptr<Channel>> fd2chan_;

public:
    explicit Epoll();

    ~Epoll();

    void epoll_add(const std::shared_ptr<Channel> &request);

    void epoll_mod(const std::shared_ptr<Channel> &request);

    void epoll_del(const std::shared_ptr<Channel> &request);

    std::shared_ptr<Channel> getChannel(int fd);

    std::vector<std::shared_ptr<Channel>> poll();
};
