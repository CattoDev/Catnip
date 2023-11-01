#ifndef __CATNIP_CACHEDTEXTURE__
#define __CATNIP_CACHEDTEXTURE__

#include <string>
#include <Geode/cocos/textures/CCTexture2D.h>

class CachedTexture {
public:
    cocos2d::CCTexture2DPixelFormat pixelFormat;
    unsigned char* textureData;
    unsigned long textureDataSize;
    unsigned short width;
    unsigned short height;

public:
    void saveFile(std::string);
    bool loadFile(std::string);
};

#endif