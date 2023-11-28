#ifndef __CATNIP_PROFILER__ 
#define __CATNIP_PROFILER__ 

#include <string>
#include <functional>

struct CNProfilerStat {
    std::string funcName;
    long long executionTime; // in nanoseconds
    float percentage = 0;
};

namespace CNProfiler {
    void setTitle(std::string title);
    void pushStat(CNProfilerStat stat);
    long long timeClassFunc(std::function<void()> func);
    void setMaxTime(long long time);
}

// we will mainly use macros so it doesn't compile with the Profiler if we're not in debug
#ifdef CN_DEBUG
#define CN_FUNCDEF(funcDef, funcCode) inline funcDef funcCode
#define CN_DBGCODE(code) code
#else
#define CN_FUNCDEF(funcDef, funcCode) inline funcDef {}
#define CN_DBGCODE(code)
#endif

CN_FUNCDEF(void CNPROF_TITLE(std::string title), { CNProfiler::setTitle(title); })
CN_FUNCDEF(void CNPROF_PUSHSTAT(const CNProfilerStat& stat), { CNProfiler::pushStat(stat); })
CN_FUNCDEF(long long CNPROF_TIMECLASSVOIDFUNC(std::function<void()> func), { return CNProfiler::timeClassFunc(func); })
CN_FUNCDEF(void CNPROF_SETMAXTIME(long long time), { CNProfiler::setMaxTime(time); });

#endif