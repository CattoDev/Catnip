#include <Geode/Geode.hpp>

using namespace geode::prelude;

class CCTextureCacheExtra : public CCTextureCache {
public:
    CCDictionary*& getTexturesDict() {
        return m_pTextures;
    }

    static CCTextureCacheExtra* get() {
        return as<CCTextureCacheExtra*>(CCTextureCache::sharedTextureCache());
    }
};