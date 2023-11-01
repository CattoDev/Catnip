#ifndef __CATNIP_THREADPOOL__
#define __CATNIP_THREADPOOL__

#include "ThreadPoolTypes.hpp"

#include <mutex>
#include <queue>
#include <vector>

class CatnipThreadPool {
private:
    std::vector<ThreadPoolWorker*> workers;
    std::queue<DataLoadingType*> dataQueue, finishedQueue;
    unsigned int threadCount;
    bool scheduleShutdown = false;
    int workersDone = 0;
    std::mutex workerLock, queueDataLock, finishedQueueLock;

public:
    CatnipThreadPool();
    CatnipThreadPool(unsigned int);
    ~CatnipThreadPool();

    unsigned int getThreadCount();
    void workerLoop(ThreadPoolWorker*);
    void queueTask(DataLoadingType*);
    void startPool();
    void terminatePool();
    void waitForTasks();
    bool poolFinished();
    DataLoadingType* tryGetFinishedResult(bool&);
};

#endif