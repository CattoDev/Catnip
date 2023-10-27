#ifndef __CATNIP_THREADPOOLTYPES__
#define __CATNIP_THREADPOOLTYPES__

#include <thread>
#include <functional>
#include <any>

class CatnipThreadPool;

struct ThreadPoolWorker {
    std::thread thread;
    bool finished = false;
};

struct DataLoadingType {
    std::any data;
    std::function<bool(std::any&)> process;
    bool valid = true;

    DataLoadingType() {}
    DataLoadingType(std::any d, std::function<bool(std::any&)> func) {
        data = d;
        process = func;
    }
    ~DataLoadingType() {}
};

#endif