#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Utils
{
    class DelayedDispatcher
    {
    public:
        // C++23: move_only_function
        using Task = std::move_only_function<void()>;
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        static DelayedDispatcher& Get()
        {
            static DelayedDispatcher instance;
            return instance;
        }

        template <class Rep, class Period>
        void PostDelayed(std::chrono::duration<Rep, Period> delay, Task&& task)
        {
            const auto executeAt = Clock::now() + delay;

            {
                std::scoped_lock lock(m_mutex);
                m_queue.emplace(executeAt, std::move(task));
            }
            m_cv.notify_one();
        }

        void Stop()
        {
            m_worker.request_stop();
        }

    private:
        struct ScheduledTask
        {
            TimePoint time;
            // mutable because std::priority_queue return elements as const reference.
            mutable Task task; 

            // C++20 Spaceship operator for sort
            // invert > operator
            bool operator>(const ScheduledTask& other) const {
                return time > other.time;
            }
        };

        DelayedDispatcher()
        {
            m_worker = std::jthread([this](std::stop_token stoken) {
                RunLoop(std::move(stoken));
            });
        }

        ~DelayedDispatcher()
        {
            Stop();
        }

        void RunLoop(std::stop_token stoken)
        {
            while (!stoken.stop_requested()) {
                Task task_to_run;

                {
                    std::unique_lock lock(m_mutex);
                    
                    m_cv.wait(lock, stoken, [this] { return !m_queue.empty(); });

                    if (stoken.stop_requested()) {
                        return;
                    }

                    auto now = Clock::now();
                    auto& top_task = m_queue.top();

                    if (top_task.time <= now) {
                        task_to_run = std::move(top_task.task);
                        m_queue.pop();
                    } else {
                        auto sleep_until = top_task.time;
                        m_cv.wait_until(lock, stoken, sleep_until, [this, sleep_until] {
                            return !m_queue.empty() && m_queue.top().time < sleep_until;
                        });
                        continue; 
                    }
                }
                
                if (task_to_run) {
                    task_to_run();
                }
            }
        }

        std::priority_queue<ScheduledTask, std::vector<ScheduledTask>, std::greater<>> m_queue;
        std::mutex m_mutex;
        // condition_variable_any for std::stop_token
        std::condition_variable_any m_cv; 
        std::jthread m_worker;
    };
}