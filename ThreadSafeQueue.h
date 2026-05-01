#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <deque>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(item));
        cv_.notify_one();
    }

    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    void wait_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || quit_; });
        if (!queue_.empty()) {
            item = std::move(queue_.front());
            queue_.pop_front();
        }
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }

    void quit() {
        std::lock_guard<std::mutex> lock(mutex_);
        quit_ = true;
        cv_.notify_all();
    }

private:
    std::deque<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool quit_ = false;
};

#endif // THREAD_SAFE_QUEUE_H