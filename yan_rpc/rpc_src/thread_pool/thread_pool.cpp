#include "thread_pool.h"
#include "../../log_manager.h"

#include <chrono>
#include <stdexcept>
namespace yan_rpc {
ThreadPool::ThreadPool(size_t threads_size) :
stop_(false),
task_finished_(0),
active_threads_(0),
state_(ThreadPoolState::RUNNING),
current_threads_(0)
{
    Config.core_threads = threads_size;
    Config.max_core_threads = threads_size;
    Config.max_queue_size = 0; //无限
    Config.keep_alive_time = std::chrono::milliseconds(8000);
    workers_.reserve(threads_size); //先预留好空间避免扩容
    for (int i = 0; i < threads_size; i++) {
        workers_.emplace_back(&ThreadPool::Work, this);
    }
    current_threads_ = threads_size;
}
ThreadPool::ThreadPool(const ThreadPoolConfig& config) :
stop_(false),
task_finished_(0),
active_threads_(0),
state_(ThreadPoolState::RUNNING),
current_threads_(0)
{
    Config = config;
    workers_.reserve(config.core_threads);
    for (int i = 0; i < Config.core_threads; i++) {
        workers_.emplace_back(&ThreadPool::Work, this);
    }
   current_threads_ = config.core_threads;
}
void ThreadPool::Work() {
    while (true) {
        Task task{};

        {
            std::unique_lock<std::mutex> lock(queue_mtx_);

            const auto keep_time = Config.keep_alive_time;
            const bool has_task = task_vari_.wait_for(
                lock,
                keep_time,
                [this] {
                    return stop_ ||
                           (state_ == ThreadPoolState::RUNNING && !tasks_.empty()) ||
                           (state_ == ThreadPoolState::SHUT_DOWN && !tasks_.empty());
                });

            if (!has_task) {
                if (current_threads_ > Config.core_threads) {
                    --current_threads_;
                    LOGGER_INFO("thread {} close", std::this_thread::get_id());
                    return;
                }

                continue;
            }

            if (stop_) {
                return;
            }

            if (tasks_.empty()) {
                continue;
            }

            task = std::move(tasks_.top());
            tasks_.pop();
            ++active_threads_;

            LOGGER_INFO("thread {} get task", std::this_thread::get_id());
            queue_vari_.notify_one();
        }

        if (task.IsVailed()) {
            try {
                task.Execute();
                ++task_finished_;
                LOGGER_INFO("thread {} execute finish", std::this_thread::get_id());
            } catch (const std::exception& e) {
                LOGGER_ERROR("EXECUTE TASK ERROR {}", e.what());
            }
        }

        bool should_notify_shutdown = false;

        {
            std::lock_guard<std::mutex> lock(queue_mtx_);

            --active_threads_;

            should_notify_shutdown =
                state_ == ThreadPoolState::SHUT_DOWN &&
                tasks_.empty() &&
                active_threads_ == 0;
        }

        if (should_notify_shutdown) {
            task_vari_.notify_all();
        }
    }
}

void ThreadPool::Pause() {
    auto expected = ThreadPoolState ::RUNNING;
    if(state_.compare_exchange_strong(expected, ThreadPoolState::PAUSED)) {
         task_vari_.notify_all();
    }
    
}
//从暂停转为running
void ThreadPool::Resume() {
    auto expected = ThreadPoolState::PAUSED;
    if (state_.compare_exchange_strong(expected, ThreadPoolState::RUNNING)) {
        task_vari_.notify_all();
    }
}
//优雅的退出 超时退化为stop
bool ThreadPool::ShutDown(std::chrono::milliseconds wait_timeout_ms) {
    auto expected = ThreadPoolState::RUNNING;

    if (!state_.compare_exchange_strong(
            expected, ThreadPoolState::SHUT_DOWN)) {
        return state_ == ThreadPoolState::STOP;
    }

    task_vari_.notify_all();
    queue_vari_.notify_all();

    bool completed = false;

    {
        std::unique_lock<std::mutex> lock(queue_mtx_);

        completed = task_vari_.wait_for(lock, wait_timeout_ms, [this] {
            return tasks_.empty() && active_threads_ == 0;
        });
        if (!completed) {
         while (!tasks_.empty()) {
        tasks_.pop();
    }
}   
        state_ = ThreadPoolState::STOP;
        stop_ = true;
    }

    task_vari_.notify_all();
    queue_vari_.notify_all();

    {
        std::lock_guard<std::mutex> lock(vector_mtx_);

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    return completed;
}

//直接退出 暴力
void ThreadPool::Stop() {
    {
     std::unique_lock<std::mutex> lm(queue_mtx_);
      //队列里还有任务直接抛弃
      while(!tasks_.empty()) {
        tasks_.pop();
      }
     stop_.store(true);
     state_ = ThreadPoolState::STOP;
     task_vari_.notify_all();
     queue_vari_.notify_all();
    }
    //线程全部退出 join
    {
        std::unique_lock<std::mutex> lm(vector_mtx_);
       for (auto& p : workers_) {
        if (p.joinable()) {
            p.join(); //销毁
        }
    }
}
}
ThreadPool::Stat ThreadPool::GetStat() const {
       std::unique_lock<std::mutex> lm(data_mtx_);
        return {
             task_finished_.load(),
            current_threads_.load()
            };
}
size_t ThreadPool:: WorkerSize() const {
    return current_threads_.load();
}
size_t ThreadPool:: TasksSize()const {
     std::unique_lock<std::mutex> lm(queue_mtx_);
     return tasks_.size();
}
void ThreadPool:: ConjustThreadSize() {
    if (current_threads_ < Config.max_core_threads && tasks_.size() > active_threads_) {
        current_threads_++;
        try{
        std::unique_lock<std::mutex>um(vector_mtx_);
        workers_.emplace_back(&ThreadPool::Work, this);
        } catch(std::exception& e) {
            current_threads_--;
            LOGGER_ERROR("Failed To Conjust Thread Size {}", e.what());
        }
    }
}
ThreadPool::~ThreadPool() {
    Stop();

}
} //namespace yan_rpc