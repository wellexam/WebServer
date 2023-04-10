#pragma once

#ifndef _WIN32
#include <sys/eventfd.h>
#include <unistd.h>
#endif // !_WIN32


#include <memory>
#include <cassert>

#include "ThreadPool.hpp"
#include "Epoll.hpp"
#include "Channel.hpp"

class Reactor {
    class PendingList {
    public:
        // 队列
        std::vector<std::function<void()>> tasks;
        // 任务队列的读写锁
        std::mutex mut;
    };

    std::shared_ptr<PendingList> pendingList;
    std::shared_ptr<ThreadPool> threadPool;
    std::shared_ptr<Epoll> poller;

    std::shared_ptr<Channel> wakeupChannel;

    bool looping_;
    bool quit_;
    unsigned long long count = 0;

public:
    explicit Reactor(int threadNum = 20);

    void loop();

    void quit();

    std::shared_ptr<Channel> getChannel(int fd);

    void appendToThreadPool(std::function<void()> &&task);

    void addPendingTask(std::function<void()> &&task);

    void removeFromPoller(const std::shared_ptr<Channel> &channel) { poller->epoll_del(channel); }
    void updatePoller(const std::shared_ptr<Channel> &channel, int timeout = 0) {
        poller->epoll_mod(channel);
    }
    void addToPoller(const std::shared_ptr<Channel> &channel, int timeout = 0) {
        poller->epoll_add(channel);
    }

private:
    void wakeup() {
        char buf;
#ifdef _WIN32
        send(wakeupChannel->getFd(), &buf, sizeof buf, 0);
#else
        write(wakeupChannel->getFd(), &buf, sizeof buf);
#endif // _WIN32
    }

    void doPendingTasks();
};