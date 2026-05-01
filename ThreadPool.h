#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <thread>
#include <vector>
#include <functional>
#include <future>
#include <type_traits>
#include <atomic>
#include "ThreadSafeQueue.h"

class ThreadPool {
public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // 提交任务，返回 future 以便获取结果（可选）
    template<typename F, typename... Args>
    auto Enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    // 简化版：提交一个无返回值的任务
    void EnqueueSimple(std::function<void()> task);

    size_t ThreadCount() const { return workers_.size(); }

private:
    std::vector<std::thread> workers_;
    ThreadSafeQueue<std::function<void()>> taskQueue_;
    std::atomic<bool> stop_{false};

    void WorkerLoop();
};

// 模板实现（需要放在头文件中）
template<typename F, typename... Args>
auto ThreadPool::Enqueue(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    taskQueue_.push([task]() { (*task)(); });
    return res;
}

#endif // THREAD_POOL_H