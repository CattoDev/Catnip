// improved with the help of https://github.com/dtrugman/ThreadPool/blob/master/ThreadPool.hpp

#include "ThreadPool.hpp"
#include <Utils.hpp>

// debug
#include <Geode/loader/Log.hpp>

#define ThreadLock std::unique_lock<std::mutex> _tLock(threadLock);
#define ResultLock std::unique_lock<std::mutex> _rLock(resultLock);

CatnipThreadPool::CatnipThreadPool() {
    threadCount = getMaxCatnipThreads();
}

CatnipThreadPool::CatnipThreadPool(unsigned int tCount) {
    threadCount = tCount;
}

CatnipThreadPool::~CatnipThreadPool() {
    this->terminatePool();
}

unsigned int CatnipThreadPool::getThreadCount() {
    return threadCount;
}

void CatnipThreadPool::setContainResults(bool toggled) {
    containResults = toggled;
}

void CatnipThreadPool::workerLoop() {
    geode::log::debug("Thread {} started.", std::this_thread::get_id());

    while(true) {
        // get task
        CatnipTask task;
        bool set = false;
        {
            //std::unique_lock<std::mutex> _tLock(threadLock);
            ThreadLock
            if(tasksQueue.empty()) {
                if(scheduleShutdown) {
                    break;
                }

                waitVar.notify_one();
                condVar.wait(_tLock);
            }
            else {
                threadsBusy++;
                task = std::move(tasksQueue.front());
                tasksQueue.pop();
                set = true;
            }
        }

        // execute task
        if(set) {
            if(task.func()) {
                // successful
                if(containResults) {
                    {
                        ResultLock
                        resultIdxQueue.push(task.idx);
                    }
                } 
            }
            else {
                // failed

            }

            threadsBusy--;  
        }
    }

    geode::log::debug("Thread {} finished.", std::this_thread::get_id());
}

void CatnipThreadPool::queueTask(std::function<bool()> task, int taskIndex) {
    ThreadLock
    tasksQueue.emplace(CatnipTask { taskIndex, std::move(task) });

    condVar.notify_one();
}

void CatnipThreadPool::startPool() {
    // clear results
    if(containResults) {
        std::queue<int> empty;
        std::swap(resultIdxQueue, empty);
    }

    if(workers.empty()) {
        for(size_t i = 0; i < this->getThreadCount(); i++) {
            workers.push_back(std::thread(&CatnipThreadPool::workerLoop, this));
        }
    }
    else {
        condVar.notify_all();
    }
}

void CatnipThreadPool::terminatePool() {
    {
        ThreadLock
        scheduleShutdown = true;
    }

    condVar.notify_all();

    for(auto& w : workers) {
        w.join();
    }
    workers.clear();

    scheduleShutdown = false;
}

void CatnipThreadPool::waitForTasks() {
    std::unique_lock lock(threadLock);
    waitVar.wait(lock, [this] { return this->tasksQueue.empty() && threadsBusy == 0; });
}

bool CatnipThreadPool::poolFinished() {
    ThreadLock
    return this->tasksQueue.empty() && threadsBusy == 0;
}

int CatnipThreadPool::getFinishedResultIdx(bool& finished) {
    finished = false;

    int idx = -1;

    ResultLock
    if(!resultIdxQueue.empty()) {
        idx = resultIdxQueue.front();
        resultIdxQueue.pop();
    }
    else if(this->poolFinished()) finished = true;

    return idx;
}