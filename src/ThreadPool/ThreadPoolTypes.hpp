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

struct DataLoadingStruct {

};

struct DataLoadingType {
    DataLoadingStruct* data;
    std::function<bool(DataLoadingStruct*)> process;

    DataLoadingType() {}
    DataLoadingType(DataLoadingStruct* d, std::function<bool(DataLoadingStruct*)> func) {
        data = d;
        process = func;
    }
    ~DataLoadingType() {
        CC_SAFE_DELETE(data);
    }
};

#endif