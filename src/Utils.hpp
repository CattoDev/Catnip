#ifndef __CATNIP_UTILS__
#define __CATNIP_UTILS__

#include <Geode/Geode.hpp>
#include <chrono>

#include <ThreadPool/ThreadPoolTypes.hpp>

using namespace geode::prelude;

struct CCTexture2DThreaded : DataLoadingStruct {
	CCTexture2D* texture = nullptr;
	unsigned char* textureData;
	int width, height;
	CCSize imageSize;
    CCTexture2DPixelFormat pixelFormat;
    bool dataAltered = false;
    unsigned long dataSize = 0;
    ghc::filesystem::path filePath;
    std::string fileName;
    bool successfulLoad = true;
    bool useCache = false;
    bool createCache = false;
    bool g_cachingEnabled = false;
};

namespace CatnipTimer {
    void start();
    void end();
    std::string endWithStr();
};

unsigned char* getFileData(const char* filePath, unsigned long *bytesRead);
void writeFileData(const char* filePath, unsigned char* data, unsigned long dataSize);
void createTextureFromFileTS(CCTexture2DThreaded*);

#endif