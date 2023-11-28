#ifndef __CATNIP_THREADPOOL__
#define __CATNIP_THREADPOOL__

#include "ThreadPoolTypes.hpp"

#include <mutex>
#include <queue>
#include <vector>
#include <condition_variable>
#include <future>
#include <memory>
#include <type_traits>

class CatnipThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<CatnipTask> tasksQueue;
    std::queue<int> resultIdxQueue;
    unsigned int threadCount;
    bool scheduleShutdown = false;
    bool containResults = true;
    std::atomic<int> threadsBusy = 0;
    std::mutex threadLock, resultLock, busyLock;
    std::condition_variable condVar, waitVar;

public:
    CatnipThreadPool();
    CatnipThreadPool(unsigned int);
    ~CatnipThreadPool();

    unsigned int getThreadCount();
    void setContainResults(bool);
    void workerLoop();
    void queueTask(std::function<bool()>, int idx = 0);
    void startPool();
    void terminatePool();
    void waitForTasks();
    bool poolFinished();
    int getFinishedResultIdx(bool&);
};

#endif