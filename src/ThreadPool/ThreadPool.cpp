#include "ThreadPool.hpp"

#define WorkerLock std::lock_guard<std::mutex> _lock1(workerLock);
#define DataLock std::lock_guard<std::mutex> _lock2(queueDataLock);
#define ResultLock std::lock_guard<std::mutex> _lock3(finishedQueueLock);

CatnipThreadPool::CatnipThreadPool(unsigned int tCount) {
    threadCount = tCount;
}

CatnipThreadPool::CatnipThreadPool() {
    threadCount = std::thread::hardware_concurrency();
}

CatnipThreadPool::~CatnipThreadPool() {
    this->terminatePool();
}

unsigned int CatnipThreadPool::getThreadCount() {
    return threadCount;
}

void CatnipThreadPool::workerLoop(ThreadPoolWorker* worker) {
    while(true) {
        // get queued data
        DataLoadingType data;
        bool set = false;
        {
            DataLock
            if(!dataQueue.empty()) {
                data = dataQueue.front();
                dataQueue.pop();
                set = true;
            }
        }

        // process data
        if(set) {
            if(data.process(data.data)) {
                ResultLock
                finishedQueue.push(data);
            } 
            else {
                // requeue
                DataLock
                dataQueue.push(data);
            }
        }
        else {
            WorkerLock
            if(!worker->finished && canStopWorking) {
                workersDone++;
                worker->finished = true;
            }
            
            if(scheduleShutdown) break;
        }
    }
}

void CatnipThreadPool::queueTask(DataLoadingType data) {
    DataLock
    dataQueue.push(data);
}

void CatnipThreadPool::startPool() {
    workersDone = 0;

    if(workers.size() >= this->getThreadCount()) {
        WorkerLock
        for(auto& w : workers) {
            w->finished = false;
        }
        
        return;
    }

    for(size_t i = 0; i < this->getThreadCount(); i++) {
        auto worker = new ThreadPoolWorker();
        worker->thread = std::thread(&CatnipThreadPool::workerLoop, this, worker);

        workers.push_back(worker);
    }
}

void CatnipThreadPool::terminatePool() {
    {
        WorkerLock
        scheduleShutdown = true;
    }

    for(auto& w : workers) {
        w->thread.join();
        delete w;
    }
}

void CatnipThreadPool::waitForTasks() {
    while(!poolFinished()) {}
}

bool CatnipThreadPool::poolFinished() {
    WorkerLock
    bool done = workersDone == this->getThreadCount();

    if(done) {
        canStopWorking = false;
    }

    return done;
}

DataLoadingType CatnipThreadPool::tryGetFinishedResult() {
    DataLoadingType data;
    data.valid = false;

    {
        ResultLock
        if(!finishedQueue.empty()) {
            data = finishedQueue.front();
            finishedQueue.pop();
        }
    }

    return data;
}

void CatnipThreadPool::tasksAdded() {
    WorkerLock
    canStopWorking = true;
}