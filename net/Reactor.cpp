#include "Reactor.hpp"

#include "../log/log.h"
#include "../base/Platform.hpp"

Reactor::Reactor(int threadNum) :
    threadPool(std::make_shared<ThreadPool>(threadNum)), poller(std::make_shared<Epoll>()),
    pendingList(std::make_shared<PendingList>()), looping_(false), quit_(false) {
#ifdef _WIN32
    YetiSocketFD ret = socket(NULL, AF_UNIX, SOCK_STREAM);
    if (ret == YETI_INVALID_SOCKET) {
        perror("event socket invalid");
        return;
    }
#else
    YetiSocketFD ret = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#endif // _WIN32
    
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
    YetiSocketFD wakeFd = wakeupChannel->getFd();
    LOG_DEBUG("loop started!")
    while (!quit_) {
        ret.clear();
        ret = poller->poll();
        doPendingTasks();
        for (auto &it : ret) {
            LOG_DEBUG("handling fd[%d] at loop %d", it->getFd(), count)
            it->handleEvents();
        }
        ++count;
    }
    looping_ = false;
}

void Reactor::quit() {
    quit_ = true;
    SocketClose(wakeupChannel->getFd());
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
