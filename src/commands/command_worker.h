#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class CommandWorker {
public:
    CommandWorker();
    ~CommandWorker();

    // Prevent copying to avoid thread/mutex copy issues
    CommandWorker(const CommandWorker&) = delete;
    CommandWorker& operator=(const CommandWorker&) = delete;

    // Enqueue a callable task (lambda, function, etc.)
    void push(std::function<void()> task);

    // Signal the worker to stop processing and wake up the thread
    void stop();

private:
    // Main loop executed by worker_ thread
    void run();

    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
    std::thread worker_;
};
