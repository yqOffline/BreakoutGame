#include "ThreadPool.h"
#include <functional>
#include <chrono>

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ThreadPool::WorkerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    stop_ = true;
    // 向队列推入空任务，通知所有线程退出
    for (size_t i = 0; i < workers_.size(); ++i) {
        taskQueue_.push([](){});
    }
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

void ThreadPool::EnqueueSimple(std::function<void()> task) {
    taskQueue_.push(std::move(task));
}

void ThreadPool::WorkerLoop() {
    while (!stop_) {
        std::function<void()> task;
        // 使用带超时的等待，避免永久阻塞
        if (taskQueue_.try_pop(task)) {
            task();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}