#include "Reactor.hpp"

#include "../log/log.h"


Reactor::Reactor(int threadNum) :
    threadPool(std::make_shared<ThreadPool>(threadNum)), poller(std::make_shared<Epoll>()),
    pendingList(std::make_shared<PendingList>()), looping_(false), quit_(false) {
    int ret = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    wakeupChannel = std::make_shared<Channel>(ret);
    wakeupChannel->setEvents(EPOLLIN);
    wakeupChannel->setReadHandler([ret] {
        char buf;
        read(ret, &buf, sizeof(buf));
    });
    addToPoller(wakeupChannel);
}

void Reactor::loop() {
    assert(!looping_);
    looping_ = true;
    quit_ = false;
    std::vector<SP_Channel> ret;
    count = 0;
    int wakeFd = wakeupChannel->getFd();
    LOG_DEBUG("loop started!")
    while (!quit_) {
        ret.clear();
        ret = poller->poll();
        for (auto &it : ret) {
            LOG_DEBUG("handling fd[%d] at loop %d", it->getFd(), count)
            // threadPool->append([it, count = this->count] {
            //     LOG_DEBUG("fd[%d], added to thread pool at loop %d", it->getFd(), count)
            //     it->handleEvents();
            //     LOG_DEBUG("fd[%d], added to thread pool at loop %d, finished", it->getFd(),
            //     count)
            // });
            it->handleEvents();
        }
        // doPendingTasks();
        ++count;
    }
    looping_ = false;
}

void Reactor::quit() {
    quit_ = true;
    close(wakeupChannel->getFd());
}

void Reactor::doPendingTasks() {
    std::vector<std::function<void()>> tempList;
    {
        std::lock_guard<std::mutex> lk(pendingList->mut);
        tempList.swap(pendingList->tasks);
    }
    for (auto &task : tempList) {
        LOG_DEBUG("doing pending tasks in loop %d", count)
        task();
    }
}

std::shared_ptr<Channel> Reactor::getChannel(int fd) {
    return poller->getChannel(fd);
}

void Reactor::appendToThreadPool(std::function<void()> &&task) {
    threadPool->append(std::move(task));
}

void Reactor::addPendingTask(std::function<void()> &&task) {
    {
        std::lock_guard<std::mutex> lk(pendingList->mut);
        pendingList->tasks.emplace_back(std::move(task));
    }
    wakeup();
}
