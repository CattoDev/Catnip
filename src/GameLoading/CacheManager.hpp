#ifndef __CATNIP_CACHEMANAGER__
#define __CATNIP_CACHEMANAGER__

#include <Geode/Geode.hpp>

#include <Utils.hpp>
#include "CachedTexture.hpp"

class CacheManager {
public:
    static ghc::filesystem::path getCachePath();
    static ghc::filesystem::path getCacheFiletype(ghc::filesystem::path);

    static void cacheProcessedImage(CCTexture2DThreaded*);
    static bool loadCachedTexture(ghc::filesystem::path, CachedTexture&);
};

#endif