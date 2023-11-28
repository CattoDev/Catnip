#ifndef __CATNIP_THREADPOOLTYPES__
#define __CATNIP_THREADPOOLTYPES__

#include <thread>
#include <functional>

struct CatnipTask {
    int idx;
    std::function<bool()> func;
};

#endif