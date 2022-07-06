#include "HeapTimer.hpp"

void HeapTimer::adjust(int id, int newExpires) {
    /* 调整指定id的结点 */
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(newExpires);
    siftdown_(ref_[id], heap_.size());
}
void HeapTimer::add(int id, int timeOut, const TimeoutCallBack &cb) {
    assert(id >= 0);
    size_t i;
    if (ref_.count(id) == 0) {
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeOut), cb});
        siftup_(i);
    } else {
        /* 已有结点：调整堆 */
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeOut);
        heap_[i].cb = cb;
        if (!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }
    // printf("%d added to timerHeap\n", id);
}
void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}
void HeapTimer::tick() {
    /* 清除超时结点 */
    if (heap_.empty()) {
        return;
    }
    while (!heap_.empty()) {
        auto &node = heap_.front();
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        // printf("tick closed %d \n", node.id);
        pop();
    }
}
void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}
int HeapTimer::getNextTick() {
    // printf("getNextTick closed ");
    tick();
    size_t res = -1;
    if (!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (res < 0) {
            res = 0;
        }
    }
    return res;
}
void HeapTimer::del_(size_t index) {
    /* 删除指定位置的结点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if (i < n) {
        SwapNode_(i, n);
        if (!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    // printf("%d timeout\n", heap_.back().id);
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

void HeapTimer::heap_size() {
    printf("heap size is %ld cap is %ld\n", heap_.size(), heap_.capacity());
}

#if four_ary_heap

void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 4;
    while (j >= 0) {
        if (heap_[j] < heap_[i]) {
            break;
        }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 4;
    }
}
bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 4 + 1;
    while (j < n) {
        size_t rightest_child = i * 4 + 4;
        size_t min_child_index = j;
        for (; j < rightest_child && j + 1 < n; j++) {
            if (heap_[j + 1] < heap_[min_child_index]) {
                min_child_index = j + 1;
            }
        }
        j = min_child_index;
        if (heap_[i] < heap_[j])
            break;
        SwapNode_(i, j);
        i = j;
        j = i * 4 + 1;
    }
    return i > index;
}

#else

void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;
    while (j >= 0) {
        if (heap_[j] < heap_[i]) {
            break;
        }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}
bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while (j < n) {
        if (j + 1 < n && heap_[j + 1] < heap_[j])
            j++;
        if (heap_[i] < heap_[j])
            break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

#endif