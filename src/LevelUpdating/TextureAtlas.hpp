#ifndef __CATNIP_TEXTUREATLAS__
#define __CATNIP_TEXTUREATLAS__

#include <Geode/Geode.hpp>

namespace TextureAtlas {
    void queueBufferMapping(geode::prelude::CCTextureAtlas*);
    void mapAtlasBuffers();

    void setAtlasQueueing(bool);
}

#endif