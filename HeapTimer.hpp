#pragma once

#include <queue>
#include <unordered_map>
#include <ctime>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <cassert>
#include <chrono>
#include <mutex>

#include "Debug.hpp"

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
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }

    ~HeapTimer() { clear(); }

    // 重新调整连接调整超时时间
    void adjust(int id, int newExpires);

    // 注册新的连接，同时传入超时回调函数入口
    void add(int id, int timeOut, const TimeoutCallBack &cb);

    // 清空时间堆
    void clear();

    // 拨动一个刻度，从堆顶循环清除当前堆中所有超时节点
    void tick();

    void pop();

    // 获得下一个最近的超时时间距当前时间的间隔毫秒数
    int getNextTick();

    void heap_size();

private:
    void del_(size_t index);

    void siftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;

    std::unordered_map<int, size_t> ref_;
};