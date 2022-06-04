#pragma once

#include <condition_variable>
#include <cstdio>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>

// 线程池类
class ThreadPool {
public:
    // 参数thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量
    explicit ThreadPool(int thread_number) : pool(std::make_shared<Pool>()) {
        for (int i = 0; i < thread_number; i++) {
            std::thread([pool_ = pool, i] {
                std::unique_lock<std::mutex> lk(pool_->mut);
                while (true) {
                    pool_->cond.wait(lk, [&pool_] { return !pool_->tasks.empty() || pool_->isClosed; });
                    if (pool_->isClosed) {
                        break;
                    }
                    auto task = pool_->tasks.front();
                    pool_->tasks.pop_front();
                    lk.unlock();
                    task();
                    lk.lock();
                }
                printf("thread %d ended\n", i);
            }).detach();
        }
    }
    ~ThreadPool();

    // 往请求队列中添加任务

    template <typename Func>
    bool append(Func &&task);

private:
    class Pool {
    public:
        // 队列
        std::list<std::function<void()>> tasks;
        // 任务队列的读写锁
        std::mutex mut;
        // 任务队列的条件变量
        std::condition_variable cond;
        // 线程池是否析构
        std::atomic<bool> isClosed{false};
    };

    std::shared_ptr<Pool> pool;
};

ThreadPool::~ThreadPool() {
    //{
    //    std::lock_guard<std::mutex> lk(pool->mut);
    //    pool->isClosed = true;
    //}
    pool->isClosed = true;
    pool->cond.notify_all();
}

template <typename Func>
bool ThreadPool::append(Func &&task) {
    if (!pool) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(pool->mut);
        pool->tasks.emplace_back(std::forward<Func>(task));
    }
    pool->cond.notify_one();
    return true;
}
