#include "Reactor.hpp"

Reactor::Reactor(int threadNum) :
    threadPool(std::make_shared<ThreadPool>(threadNum)), poller(std::make_shared<Epoll>()),
    pendingList(std::make_shared<PendingList>()), looping_(false), quit_(false) {
    int ret = eventfd(1, EFD_CLOEXEC | EFD_NONBLOCK);
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
    while (!quit_) {
        ret.clear();
        ret = poller->poll();
        for (auto &it : ret)
            threadPool->append([it] { it->handleEvents(); });
        doPendingTasks();
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
        task();
    }
}

std::shared_ptr<Channel> Reactor::getChannel(int fd) {
    return poller->getChannel(fd);
}

void Reactor::appendToThreadPool(std::function<void()> &&task) {
    threadPool->template append(std::move(task));
}

void Reactor::removeFromPollerWithGuard(const std::shared_ptr<Channel> &channel) {
    {
        std::lock_guard<std::mutex> lk(pendingList->mut);
        pendingList->tasks.emplace_back([this, channel] { removeFromPoller(channel); });
    }
    wakeup();
}

void Reactor::updatePollerWithGuard(const std::shared_ptr<Channel> &channel, int timeout) {
    {
        std::lock_guard<std::mutex> lk(pendingList->mut);
        pendingList->tasks.emplace_back([this, channel] { updatePoller(channel); });
    }
    wakeup();
}

void Reactor::addToPollerWithGuard(const std::shared_ptr<Channel> &channel, int timeout) {
    {
        std::lock_guard<std::mutex> lk(pendingList->mut);
        pendingList->tasks.emplace_back([this, channel] { addToPoller(channel); });
    }
    wakeup();
}

void Reactor::addPendingTask(std::function<void()> &&task) {
    {
        std::lock_guard<std::mutex> lk(pendingList->mut);
        pendingList->tasks.emplace_back(std::move(task));
    }
    wakeup();
}
