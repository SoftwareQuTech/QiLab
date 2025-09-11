/*
** Created on: 08.09.2025
** Created by: mrkolev
** Description: Conatains the IRunnable class
*/

#pragma once
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <deque>

template <typename T>
struct IRunnable
{
    std::deque<T> data_queue;
    std::mutex data_lock;
    std::condition_variable data_ready;

    void notify(T value)
    {
        {
            std::lock_guard<std::mutex> lock(data_lock);
            data_queue.push_back(std::move(value));
            //std::cout << "Produced: " << value.command << "\n";
        }
        data_ready.notify_one();
    }
    virtual void on_run() = 0;
    virtual void on_abort() = 0; 
};