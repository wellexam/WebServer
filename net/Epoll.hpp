#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "Channel.hpp"

class Epoll {
    YetiEpollFD epollFd;
    std::vector<epoll_event> events_;

    static const int MAXFDS = 100000;
    mutable std::shared_mutex mut;
    std::unordered_map<YetiSocketFD, std::shared_ptr<Channel>> fd2chan_;

public:
    explicit Epoll();

    ~Epoll();

    void epoll_add(const std::shared_ptr<Channel> &request);

    void epoll_mod(const std::shared_ptr<Channel> &request);

    void epoll_del(const std::shared_ptr<Channel> &request);

    std::shared_ptr<Channel> getChannel(YetiSocketFD fd);

    std::vector<std::shared_ptr<Channel>> poll();
};
