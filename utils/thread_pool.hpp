#pragma once

#include <algorithm>
#include <functional>
#include <thread>
#include <utility>

template <typename T>
class Thread_Pool {
public:
    using Work_Func = std::function<T>;
    static constexpr int THREAD_MAX_CNT = 10;

private:
    int thread_cnt;
    Work_Func work_func;
public:
    Thread_Pool() = delete;
    Thread_Pool(int c, Work_Func f) : work_func(f) {
        thread_cnt = std::min(c, THREAD_MAX_CNT);
    }

    template <typename ...Arg>
    void start(Arg&&... arg) {
        for(int i = 0;i < thread_cnt; ++i) {
            std::thread(work_func, arg...).detach();
        }
    }
};