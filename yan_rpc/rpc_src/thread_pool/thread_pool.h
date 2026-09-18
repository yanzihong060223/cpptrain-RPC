#pragma once
#include "../../log_manager.h"

#include <atomic>
#include <thread>
#include <functional>
#include <utility>
#include <chrono>
#include <vector>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <string>
#include <memory>
#include <future>
#include <stdexcept>

namespace yan_rpc {
enum class TaskPriority {
    LOW,
    NORMAL,
    HIGH
};//任务优先级
enum class ThreadPoolState{
    RUNNING, //运行
    PAUSED, //暂停
    SHUT_DOWN,//优雅关闭 完成所有任务 不接受新任务
    STOP //关闭所有线程
};
class Task {
public:
 Task () : priority_(TaskPriority:: NORMAL) {}
 Task (TaskPriority priority, std::function<void()>&& task) : priority_(priority), task_(std::move(task)) {


 }
bool IsVailed() const {
    return static_cast<bool>(task_);  //std::function 本身定义了一个显式布尔转换运算
}
void Execute() {
    if (IsVailed()) {
        task_();
    }
}
bool operator < (const Task& others) const {
    return priority_ < others.priority_;
}
private:
TaskPriority priority_; //任务优先级
std::function<void()> task_;//任务本体
};
struct ThreadPoolConfig {
    size_t core_threads = std::thread::hardware_concurrency(); //核心线程数
    size_t max_core_threads = std::thread::hardware_concurrency() * 2;
    size_t max_queue_size = 10; //队列最大任务数量 为0就是无限
    std::chrono::milliseconds keep_alive_time {8000};
};
class ThreadPool {
public:
struct Stat {
size_t task_finished = 0;
size_t alive_threads = 0;
}; //外部线程池信息
explicit ThreadPool(size_t threads_size); //固定线程数量
explicit ThreadPool(const ThreadPoolConfig& config);
~ThreadPool();
bool ShutDown( std::chrono::milliseconds wait_timeout_ms =
        std::chrono::milliseconds::max()); //良好退出 等待任务处理
void Pause(); //暂停
void Resume(); //恢复线程池
Stat GetStat() const;//获取信息
size_t WorkerSize() const;//获取线程池大小
size_t TasksSize()const; //获取任务大小
void Stop();
template<typename F, typename... Args>
//下发任务
auto Enquene(TaskPriority priority, F&&f, Args&& ... args) ->std::future<typename std::invoke_result<F, Args...>::type>;
private:
//配置
ThreadPoolConfig Config; 
//核心工作函数
void Work();
void ConjustThreadSize();//动态扩大线程数量
//线程类
std::vector<std::thread> workers_; //线程数组
std::atomic<size_t> current_threads_;
//同步类
std::condition_variable task_vari_; //任务可用条件变量
std::condition_variable queue_vari_; //队列是否已满条件变量
mutable std::mutex queue_mtx_; //队列锁
mutable std::mutex data_mtx_;
mutable std::mutex vector_mtx_;
std::atomic<bool> stop_; //停止标记
//信息类
std::atomic<size_t> task_finished_; //完成的任务数量
std::atomic<size_t> active_threads_; //存活线程数量 
std::atomic<ThreadPoolState> state_;
std::priority_queue<Task> tasks_; //任务队列
}; //核心类线程池
template<typename F, typename... Args>
auto ThreadPool::Enquene(TaskPriority priority, F&&f, Args&& ... args) ->std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto fun = task->get_future();
    {
        std::unique_lock<std::mutex> um(queue_mtx_);
        if (Config.max_queue_size > 0) {
            queue_vari_.wait(um, [this](){return stop_ || tasks_.size() < Config.max_queue_size || state_ == ThreadPoolState::SHUT_DOWN;});
              
        }
           if (stop_ || state_ == ThreadPoolState::SHUT_DOWN ) {
                throw std::runtime_error("ThreadPool Stop Failed To Enqueue");
            }
        tasks_.emplace(priority,[task](){(*task)();});
        task_vari_.notify_one();
        std::string priority_str;
        if (priority == TaskPriority::LOW) {
            priority_str = "LOW";
        } else if (priority == TaskPriority::NORMAL) {
            priority_str = "NORMAL";
        } else {
            priority_str = "HIGH";
        }
        LOGGER_INFO("ThreadPool enqueing task with {}, current tasks size{}/{}, current thread size{}/{}", priority_str, tasks_.size(), Config.max_queue_size, active_threads_.load(),  current_threads_.load());
        //如果任务数量大于线程数量 线程数量小于最大线程数量调用动态扩展
        if (tasks_.size() > current_threads_.load() && current_threads_.load() < Config.max_core_threads) {
            ConjustThreadSize();
        }
    }
    return fun;
}
}//namespace yan_rpc