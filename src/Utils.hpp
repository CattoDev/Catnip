#ifndef __CATNIP_UTILS__
#define __CATNIP_UTILS__

#include <Geode/Geode.hpp>
#include <chrono>

#include <ThreadPool/ThreadPoolTypes.hpp>

using namespace geode::prelude;

struct CCTexture2DThreaded {
	CCTexture2D* texture = nullptr;
	unsigned char* textureData;
	int width, height;
	CCSize imageSize;
    CCTexture2DPixelFormat pixelFormat;
    bool dataAltered = false;
    unsigned long dataSize = 0;
    std::string filePath;
    bool successfulLoad = true;
};

namespace CatnipTimer {
    void start();
    void end();
    std::string endWithStr();
};

unsigned char* getFileData(const char* filePath, unsigned long *bytesRead);
void createTextureFromFileTS(CCTexture2DThreaded&);

#endif