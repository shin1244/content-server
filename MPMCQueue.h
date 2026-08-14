#pragma once
#include <queue>
#include <mutex>

template<typename T>
class MPMCQueue
{
public:
    void Push(T value)
    {
        {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(value));
        }

        cv_.notify_one();
    }

    bool Pop(T& out)
    {
        std::unique_lock lock(mutex_);

        cv_.wait(lock, [this] {
            return !queue_.empty() || stopping_;
            });

        if (queue_.empty())
            return false;

        out = std::move(queue_.front());
        queue_.pop();

        return true;
    }

    void Stop()
    {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }

        cv_.notify_all();
    }

private:
    std::queue<T> queue_;

    std::mutex mutex_;
    std::condition_variable cv_;

    bool stopping_ = false;
};