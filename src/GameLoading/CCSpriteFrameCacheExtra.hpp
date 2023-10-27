#include <Geode/Geode.hpp>

using namespace geode::prelude;

class CCSpriteFrameCacheExtra : public CCSpriteFrameCache {
public:
    static CCSpriteFrameCacheExtra* get() {
        return as<CCSpriteFrameCacheExtra*>(CCSpriteFrameCache::sharedSpriteFrameCache());
    }

    void addSpriteFramesWithDict(CCDictionary*, CCTexture2D*);
};