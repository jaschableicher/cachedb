#include "command_worker.h"

CommandWorker::CommandWorker()
    : stop_(false), worker_(&CommandWorker::run, this) {}

CommandWorker::~CommandWorker() {
    stop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void CommandWorker::push(Task task) {
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
       
        std::optional<Task> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // Sleep until a new task arrives or a stop is requested
            cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });

            if (stop_ && tasks_.empty()) {
                break;
            }

       
            task.emplace(std::move(tasks_.front()));
            tasks_.pop();
        }

        // Execute outside the lock
        if (task.has_value()) {
            // Accessing members directly via the optional pointer operator
            Value reply = execute_command(task->db, task->msg, true);

            ssize_t bytes_sent = send(task->client_fd, &reply, sizeof(reply), 0);
            if (bytes_sent < 0) {
                close(task->client_fd);
            }
        }
    }
}
