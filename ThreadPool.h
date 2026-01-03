#include <queue>
#include <thread>
#include <iostream>
#include <string>
#include <functional>
#include <mutex>
#include <condition_variable>

using namespace std;

class ThreadPool {
private:
    queue<function<void()>> tasks;
    vector<thread> threads;   
    mutex mx;
    condition_variable cv;
    bool stop = false;

public:
    ThreadPool(int size) {  
        for(int i=0;i<size;i++) {
            threads.push_back(std::thread([this] {
                while(true) {
                  function<void()> task = this->popTask();
                  if(!task) {
                    return;
                  }
                  task();
                }
            }));
        }
    }

    ~ThreadPool() {
        cout << "destructor called \n";
        shutDown();
    }

    // method to stop the thread pool.
    void shutDown() {
        unique_lock<mutex> lock(mx);
        stop = true;
        lock.unlock();
        cv.notify_all();

        // joining all the threads so the program will wait till every thread completes its execution. 
        for(thread &t: threads) {
            if(t.joinable()) {
                t.join();
            }
        }
    }

    // method to get a task from the queue. 
    function<void()> popTask() {
        // locks other threads from accessing this code block and only allows current thread to execution.
        std::unique_lock<mutex> lock(mx);

        // puts current thread to sleep and removes the lock atomically in single step, if the predicate returns false.
        // if the predicate returns true, wakes the thread if its already sleeping and locks the mutex. 
        cv.wait(lock, [this] {return stop || !tasks.empty();}); 
       
        // if there are no tasks and we wanted to stop then only it will stop. 
        if (tasks.empty() && stop) {
            return NULL;
        }

        auto task = tasks.front();
        tasks.pop();

        lock.unlock();
        return task;
    }

    void addTask(function<void()> task) {
        // locking the queue till current thread finishes adding a task 
        std::unique_lock<mutex> lock(mx);
        tasks.emplace(task);
        lock.unlock();

        cv.notify_one(); // notifing 1 thread 
    }

};
