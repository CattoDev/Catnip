#include "TextureAtlas.hpp"
#include <mutex>

#include <Geode/modify/CCTextureAtlas.hpp>

using namespace geode::prelude;

std::mutex g_threadLock;
std::vector<CCTextureAtlas*> g_queuedAtlases;
bool g_queueAtlases = false;

class $modify(CCTextureAtlasPriv, CCTextureAtlas) {
    void _setupIndices()
    {
        if (m_uCapacity == 0)
            return;

        for( unsigned int i=0; i < m_uCapacity; i++)
        {
            m_pIndices[i*6+0] = i*4+0;
            m_pIndices[i*6+1] = i*4+1;
            m_pIndices[i*6+2] = i*4+2;

            // inverted index. issue #179
            m_pIndices[i*6+3] = i*4+3;
            m_pIndices[i*6+4] = i*4+2;
            m_pIndices[i*6+5] = i*4+1;
        }
    }

    bool resizeCapacity(unsigned int newCapacity) {
        //log::debug("resize to {}", newCapacity);

        if(!g_queueAtlases) {
            return CCTextureAtlas::resizeCapacity(newCapacity);
        }

        if( newCapacity == m_uCapacity ){
            return true;
        }
        unsigned int uOldCapactiy = m_uCapacity; 
        // update capacity and totolQuads
        m_uTotalQuads = std::min(m_uTotalQuads, newCapacity);
        m_uCapacity = newCapacity;

        ccV3F_C4B_T2F_Quad* tmpQuads = NULL;
        GLushort* tmpIndices = NULL;
        
        // when calling initWithTexture(fileName, 0) on bada device, calloc(0, 1) will fail and return NULL,
        // so here must judge whether m_pQuads and m_pIndices is NULL.
        if (m_pQuads == NULL)
        {
            tmpQuads = (ccV3F_C4B_T2F_Quad*)malloc( m_uCapacity * sizeof(m_pQuads[0]) );
            if (tmpQuads != NULL)
            {
                memset(tmpQuads, 0, m_uCapacity * sizeof(m_pQuads[0]) );
            }
        }
        else
        {
            tmpQuads = (ccV3F_C4B_T2F_Quad*)realloc( m_pQuads, sizeof(m_pQuads[0]) * m_uCapacity );
            if (tmpQuads != NULL && m_uCapacity > uOldCapactiy)
            {
                memset(tmpQuads+uOldCapactiy, 0, (m_uCapacity - uOldCapactiy)*sizeof(m_pQuads[0]) );
            }
        }

        if (m_pIndices == NULL)
        {    
            tmpIndices = (GLushort*)malloc( m_uCapacity * 6 * sizeof(m_pIndices[0]) );
            if (tmpIndices != NULL)
            {
                memset( tmpIndices, 0, m_uCapacity * 6 * sizeof(m_pIndices[0]) );
            }
            
        }
        else
        {
            tmpIndices = (GLushort*)realloc( m_pIndices, sizeof(m_pIndices[0]) * m_uCapacity * 6 );
            if (tmpIndices != NULL && m_uCapacity > uOldCapactiy)
            {
                memset( tmpIndices+uOldCapactiy, 0, (m_uCapacity-uOldCapactiy) * 6 * sizeof(m_pIndices[0]) );
            }
        }

        if( ! ( tmpQuads && tmpIndices) ) {
            CCLOG("cocos2d: CCTextureAtlas: not enough memory");
            CC_SAFE_FREE(tmpQuads);
            CC_SAFE_FREE(tmpIndices);
            CC_SAFE_FREE(m_pQuads);
            CC_SAFE_FREE(m_pIndices);
            m_uCapacity = m_uTotalQuads = 0;
            return false;
        }

        m_pQuads = tmpQuads;
        m_pIndices = tmpIndices;


        _setupIndices();
        //mapBuffers();

        // queue mapping
        TextureAtlas::queueBufferMapping(this);

        m_bDirty = true;

        return true;
    }

    void _mapBuffers() {
        ccGLBindVAO(0);
    
        glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(m_pQuads[0]) * m_uCapacity, m_pQuads, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_pIndices[0]) * m_uCapacity * 6, m_pIndices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        CHECK_GL_ERROR_DEBUG();
    }
};

void TextureAtlas::queueBufferMapping(CCTextureAtlas* atlas) {
    std::lock_guard l(g_threadLock);

    g_queuedAtlases.push_back(atlas);
}

void TextureAtlas::mapAtlasBuffers() {
    for(auto atlas : g_queuedAtlases) {
        as<CCTextureAtlasPriv*>(atlas)->_mapBuffers();
    }

    g_queuedAtlases.clear();
}

void TextureAtlas::setAtlasQueueing(bool toggle) {
    g_queueAtlases = toggle;
}