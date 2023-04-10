#pragma once

#include <queue>
#include <unordered_map>
#include <ctime>
#include <algorithm>
#include <functional>
#include <cassert>
#include <chrono>
#include <mutex>

#define four_ary_heap true

using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using TimeStamp = Clock::time_point;

struct TimerNode {
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator<(const TimerNode &t) const { return expires < t.expires; }
};

class ConcurrentUnorderedMap {
    std::mutex mut{};
    std::unordered_map<int, bool> map{};

public:
    bool operator[](int id) {
        std::lock_guard<std::mutex> lk(mut);
        return map[id];
    }

    void set(int id) {
        std::lock_guard<std::mutex> lk(mut);
        map[id] = true;
    }

    void reset(int id) {
        std::lock_guard<std::mutex> lk(mut);
        map[id] = false;
    }

    bool reset_if_true(int id) {
        std::lock_guard<std::mutex> lk(mut);
        if (map[id]) {
            map[id] = false;
            return true;
        } else {
            return false;
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mut);
        map.clear();
    }
};

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(1 << 10); }

    ~HeapTimer() { clear(); }

    // 重新调整连接调整超时时间
    void adjust(int id, int newExpires);

    // 注册新的连接，同时传入超时回调函数入口
    void add(int id, int timeOut, const TimeoutCallBack &cb);

    // 清空时间堆
    void clear();

    void pop();

    // 获得下一个最近的超时时间距当前时间的间隔毫秒数
    int getNextTick();

    void heap_size();

    void disable(int id);

private:
    void del_(size_t index);

    void siftup_(int i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    // 拨动一个刻度，从堆顶循环清除当前堆中所有超时节点
    void tick();

    std::vector<TimerNode> heap_;

    std::unordered_map<int, size_t> ref_;

    ConcurrentUnorderedMap isEnabled;
};