#include "command_worker.h"

CommandWorker::CommandWorker()
    : stop_(false), worker_(&CommandWorker::run, this) {}

CommandWorker::~CommandWorker() {
    stop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void CommandWorker::push(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void CommandWorker::stop() {
    stop_ = true;
    cv_.notify_all();
}

void CommandWorker::run() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // Sleep until new task arrives or stop is requested
            cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });

            if (stop_ && tasks_.empty()) {
                break;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        // Execute task outside the lock
        if (task) {
            task();
        }
    }
}
