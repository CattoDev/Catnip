#include <Geode/Geode.hpp>

#include <ThreadPool/ThreadPool.hpp>
#include <Utils.hpp>
#include <Profiler/Profiler.hpp>
#include "TextureAtlas.hpp"

#include <mutex>

// hooks
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJEffectManager.hpp>
#include <Geode/modify/CCSpriteBatchNode.hpp>
#include <Geode/modify/CCSprite.hpp>

using namespace geode::prelude;

bool g_allowQuadUpdate = true;

#define MBO(type, obj, offset) *reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(obj) + offset)

struct CatnipGameObjectUpdateData {
    float m_totalTime;
    gd::map<short, bool>* m_hasColors;
    CCPoint m_cameraPosition;
    float pulseSize;
    int m_activeEnterEffect;
    bool m_isDead;
    bool isBGColorDark;

    float m_halfScreenWidth;
    float m_halfScreenWidthCameraPos;
    float playerPosX;
    float playerPosX_rightSide;
    float screenRightOffset;
    float m_mirrorTransition;

    ccColor3B bgColorHSV;
    ccColor3B curBGColorHSV;
    ccColor3B LBGColor;

    bool unk460; // lol

    std::array<bool, 1100>* m_opacityExistsForGroup;

    CCDictionary* m_opacityActionsForGroup;
};

std::mutex g_batchNodeLock;

class $modify(CatnipPlayLayer, PlayLayer) {
    std::unique_ptr<CatnipThreadPool> threadPool;

    float getMeteringValue() {
        if(m_isAudioMeteringSupported) {
            return FMODAudioEngine::sharedEngine()->m_pulse1;
        }
        else {
            return m_audioEffectsLayer->m_unk1B0;
        }
    }

    bool isFlipping() {
        return m_mirrorTransition != 0 && m_mirrorTransition != 1.f;
    }

    // TODO: recreate
    ccColor3B transformColor(ccColor3B const& src, float h, float s, float v) {
        cocos2d::ccColor3B ret;
        reinterpret_cast<void(__thiscall*)(cocos2d::ccColor3B const&, cocos2d::ccColor3B*, cocos2d::ccHSVValue)>(base::get() + 0x26a60)(src, &ret, cchsv(h, s, v, true, true));
        __asm add esp, 0x14

        return ret;
    }

    static float groupOpacityMod(GameObject* self, const CatnipGameObjectUpdateData& objData) {
        float mod = 1.f;

        for(size_t i = 0; i < self->m_alphaGroupCount; i++) {
            float opacityModForGroup = 1.f;
            const short ag = self->m_alphaGroups->at(i);

            if(objData.m_opacityExistsForGroup->at(ag)) {
                auto opacAct = objData.m_opacityActionsForGroup->objectForKey(ag);

                if(opacAct) {
                    opacityModForGroup = as<OpacityEffectAction*>(opacAct)->m_opacity;
                }
                else {
                    opacityModForGroup = 1.f;
                }
            }
            else opacityModForGroup = 1.f;

            mod *= opacityModForGroup;

            if(mod <= 0) {
                mod = 0.f;
                break;
            }
        }

        return mod;
    }

    static float opacityModForMode(GameObject* obj, int colorID, bool mainColor, const CatnipGameObjectUpdateData& objData) {
        float mod = 1.f;

        auto actionSprite = mainColor ? obj->m_colorActionSpriteBase : obj->m_colorActionSpriteDetail;
        unsigned int opacity = 0;

        if(actionSprite) {
            opacity = as<unsigned int>(actionSprite->m_opacity);
        }

        if(colorID <= 0 || opacity > 249) {
            mod = 1.f;
        }
        else {
            mod = as<float>(opacity) * 0.004f;
        }

        if(obj->m_alphaGroupCount) {
            mod *= CatnipPlayLayer::groupOpacityMod(obj, objData);
        }
        
        return mod;
    }

    static float getRelativeMod(float posX, float f1, float f2, float f3, const CatnipGameObjectUpdateData& objData) {
        float relMod = 1;
        if(posX <= objData.m_halfScreenWidthCameraPos) {
            relMod = objData.m_halfScreenWidthCameraPos - posX;
            relMod -= f3;
            relMod = objData.m_halfScreenWidth - relMod;
            relMod *= f2;
        }
        else {
            relMod = (objData.m_halfScreenWidthCameraPos + objData.m_halfScreenWidth - posX) * f1;
        }

        return std::clamp(relMod, 0.f, 1.f);
    }

    static void setGlowOpacity(GameObject* obj, GLubyte opacity) {
        if(obj->m_glowSprite) {
            obj->m_glowSprite->setOpacity(opacity);
            obj->m_glowSprite->setChildOpacity(opacity);
        }
    }

    static ccColor3B getMixedColor(ccColor3B col1, ccColor3B col2, float t) {
        ccColor3B mixedColor;

        mixedColor.r = std::clamp(as<short>((1.f - t) * as<float>(col2.r) + as<float>(col1.r) * t), as<short>(0), as<short>(255));
        mixedColor.g = std::clamp(as<short>((1.f - t) * as<float>(col2.g) + as<float>(col1.g) * t), as<short>(0), as<short>(255));
        mixedColor.b = std::clamp(as<short>((1.f - t) * as<float>(col2.b) + as<float>(col1.b) * t), as<short>(0), as<short>(255));

        return mixedColor;
    }

    static int getColorMode(GJSpriteColor* color) {
        return color->m_colorID && color->m_colorID != color->m_defaultColorID ? color->m_colorID : color->m_defaultColorID; 
    }

    static void addMainSpriteToParent(GameObject* obj) {
        // blending stuff
        /*obj->m_unknownVisibility347 = obj->shouldBlendColor(obj->m_baseColor, true);

        if(obj->m_detailSprite) {
            if(CatnipPlayLayer::getColorMode(obj->m_detailColor) == 1012) {

            }
            else {

            }
        }

        // add to batch node
        int zLayer = as<int>(obj->m_zLayer);

        // z order stuff

        const int parentMode = obj->getParentMode();
        auto parent = as<CCSpriteBatchNode*>(PlayLayer::get()->parentForZLayer(zLayer, obj->m_unknownVisibility347, parentMode)); 

        //CNLog("{}, {}, {}", zLayer, obj->m_unknownVisibility347, parentMode); 

        if(parent) {
            std::lock_guard l(g_batchNodeLock);
            obj->removeFromParentAndCleanup(false);
            parent->addChild(obj, obj->m_gameZOrder);

            //CNLog("{} < {}", parent->getTextureAtlas()->getTotalQuads(), parent->getTextureAtlas()->getCapacity());  
        }
        else {
            CNLog("FAIL???");
        }*/

        std::lock_guard l(g_batchNodeLock);
        obj->addMainSpriteToParent(false);                                        
    }

    static void activateObject(GameObject* obj) {
        obj->m_shouldHide = false;

        if(!obj->m_active && !obj->m_sleeping) {
            obj->m_active = true;

            if(!obj->m_invisible) {
                obj->setVisible(true);

                // register state object

                // add main sprite
                CatnipPlayLayer::addMainSpriteToParent(obj);

                // run action
                /*if(obj->m_unk1 && obj->m_myAction) {
                    if(obj->m_runActionWithTag) {
                        auto c = obj->getChildByTag(1);

                        if(c && !c->getActionByTag(11)) {
                            c->runAction(obj->m_myAction);
                        }
                    }
                    else if(!obj->getActionByTag(11)) {
                        obj->runAction(obj->m_myAction);
                    }
                }*/
            }
        }
    }

    static void deactivateObject(GameObject* obj) {
        /*if(obj->m_shouldHide) {
            obj->m_active = false;

            obj->setVisible(false);

            std::lock_guard l(g_batchNodeLock);
            obj->removeFromParentAndCleanup(false);
        }
        else {
            obj->m_shouldHide = true;
        }*/

        std::lock_guard l(g_batchNodeLock);
        obj->deactivateObject(false);
    }

    static void updateGameObjectSprites(GameObject* obj, const CatnipGameObjectUpdateData& objData) {
        // update main color
        if(obj->m_baseColorID || obj->m_pulseGroupCount || obj->m_alphaGroupCount) {
            obj->setObjectColor(obj->colorForMode(obj->m_baseColorID, true));
            obj->m_baseColor->m_opacity = CatnipPlayLayer::opacityModForMode(obj, obj->m_baseColorID, true, objData);
        }

        // update secondary color
        if(obj->m_detailSprite && (obj->m_detailColorID || obj->m_groupCount)) {
            obj->setChildColor(obj->colorForMode(obj->m_detailColorID, false));
            obj->m_detailColor->m_opacity = CatnipPlayLayer::opacityModForMode(obj, obj->m_detailColorID, false, objData);
        }

        auto hasColorsMap = *(objData.m_hasColors);

        if(obj->m_active && !hasColorsMap.empty()) {
            if(hasColorsMap[as<short>(obj->m_baseColorID)] || (obj->m_detailSprite && hasColorsMap[as<short>(obj->m_detailColorID)])) {
                //obj->addMainSpriteToParent(false);
                //obj->addColorSpriteToParent(false);
            }
        }

        //obj->activateObject();
        CatnipPlayLayer::activateObject(obj);
    }

    static void updateGameObjectTS(GameObject* obj, const CatnipGameObjectUpdateData& objData) {
        if(obj->m_isAnimated) {
            obj->updateSyncedAnimation(objData.m_totalTime);
        }

        auto realPos = obj->getRealPosition();
        if(realPos.x <= objData.m_halfScreenWidthCameraPos) {
            realPos.x += obj->m_rectXCenterMaybe;
        }
        else {
            realPos.x -= obj->m_rectXCenterMaybe;
        }
        float relMod = CatnipPlayLayer::getRelativeMod(realPos.x, 0.014285714f, 0.014285714f, 0.f, objData);

        if(MBO(bool, obj, 0x2E2)) { // this offset is so specific wtf
            as<AnimatedGameObject*>(obj)->updateChildSpriteColor(objData.bgColorHSV);
        }

        if(obj->m_objectID == 143) {
            obj->setGlowColor(objData.curBGColorHSV);
        }

        if(obj->m_unk364) {
            return;
        }

        // update pulsing
        if(obj->m_useAudioScale) {
            float pulseSize = objData.pulseSize;

            if(obj->m_customAudioScale) {
                pulseSize = (obj->m_maxAudioScale - obj->m_minAudioScale) * (pulseSize - 0.1f) + obj->m_minAudioScale;
            }

            obj->setRScale(pulseSize);
        }

        // update enter/fade
        //obj->setOpacity((obj->m_ignoreFade || obj->m_unk365 && (!obj->m_unk368 || !obj->m_unk3b0) && obj->m_activeEnterEffect <= 1 && objData.m_activeEnterEffect == 1) ? 255u : as<GLubyte>(relMod * 255.f));
    
        const bool forceOpac = obj->m_ignoreFade || obj->m_unk365 && (!obj->m_unk368 || !obj->m_unk3b0) && obj->m_activeEnterEffect <= 1 && objData.m_activeEnterEffect == 1;
        obj->setOpacity(forceOpac ? 255u : as<GLubyte>(255.f * relMod));

        // glow color
        if(obj->m_glowUserBackgroundColour) {
            obj->setGlowColor(obj->m_useSpecialLight ? objData.bgColorHSV : objData.curBGColorHSV);
        }

        // invisible objects
        if(obj->m_invisibleMode) {
            relMod = CatnipPlayLayer::getRelativeMod(realPos.x, 0.02f, 0.014285714f, 0, objData);

            if(objData.m_isDead) {
                obj->setGlowColor(objData.curBGColorHSV);
            }
            else {
                float opacVal = 0;
                float v99 = realPos.x;

                if(realPos.x > objData.m_cameraPosition.x + objData.playerPosX_rightSide) {
                    opacVal = realPos.x - objData.m_cameraPosition.x;
                    v99 = objData.screenRightOffset;
                    opacVal -= objData.playerPosX_rightSide;
                }
                else {
                    opacVal = objData.m_cameraPosition.x + objData.playerPosX;
                    opacVal = opacVal - realPos.x;
                    v99 = objData.playerPosX - 30.f;
                }

                v99 = std::max(v99, 1.f);

                opacVal /= v99;
                opacVal = std::clamp(opacVal, 0.f, 1.f);

                float mainOpac = std::min(0.05f + opacVal * 0.95f, relMod);
                float glowOpac = std::min(0.15f + opacVal * 0.85f, relMod);

                obj->setOpacity(as<int>(mainOpac * 255.f));
                CatnipPlayLayer::setGlowOpacity(obj, as<int>(glowOpac * 255.f));

                if(mainOpac <= .8f) {
                    obj->setGlowColor(objData.curBGColorHSV);
                }
                else {
                    obj->setGlowColor(CatnipPlayLayer::getMixedColor(
                        objData.curBGColorHSV, 
                        objData.isBGColorDark ? ccc3(255, 255, 255) : objData.LBGColor,
                        0.7f + (1.f - (mainOpac - 0.8f) / 0.2f) * 0.3f
                    ));
                }
            }
        }

        // enter effect
        //PlayLayer::get()->applyEnterEffect(obj); // temp
        obj->setPosition(obj->getRealPosition()); // temp

        // update obj data
        bool isFlipping = objData.m_mirrorTransition != 0.f && objData.m_mirrorTransition != 1.f;
        if(!isFlipping) {
            if(objData.unk460) {
                obj->setFlipX(obj->m_startFlipX);
                obj->setFlipY(obj->m_startFlipY);
                obj->setRotation(obj->m_rotation);
                obj->setPosition(obj->getPosition());
            }
            return;
        }

        // mirror portal anim
    }

    static void handleObject(GameObject* obj, const CatnipGameObjectUpdateData& objData, const bool& fullReset) {
        if(!obj->m_shouldHide) {
            CatnipPlayLayer::updateGameObjectSprites(obj, objData);
        }
        else if(!fullReset) {
            CatnipPlayLayer::deactivateObject(obj);
        }
    }

    /*
        HOOKS
    */
    void updateVisibility() {
        auto dir = CCDirector::sharedDirector();
        auto winSize = dir->getWinSize();
        float screenRight = dir->getScreenRight();

        // effect manager
        m_effectManager->m_time = m_totalTime;
        m_effectManager->processColors();

        ccColor3B playerColor1 = m_effectManager->activeColorForIndex(1005);
        m_effectManager->calculateLightBGColor(playerColor1);

        // pulse
        float pulseSize = this->getMeteringValue();

        // lock pulse
        if(m_isPracticeMode || m_isMute) {
            pulseSize = .5f;
        }

        m_player1->m_meteringValue = pulseSize;
        m_player2->m_meteringValue = pulseSize;

        // variables
        CatnipGameObjectUpdateData objData;

        objData.pulseSize = pulseSize;
        objData.m_totalTime = m_totalTime;
        objData.m_hasColors = &m_hasColors;
        objData.m_halfScreenWidth = screenRight / 2.f;
        objData.m_halfScreenWidthCameraPos = m_cameraPosition.x + objData.m_halfScreenWidth;
        objData.m_activeEnterEffect = m_activeEnterEffect;
        objData.m_cameraPosition = m_cameraPosition;
        objData.unk460 = unk460;
        objData.m_mirrorTransition = m_mirrorTransition;
        objData.m_opacityExistsForGroup = &m_effectManager->m_opactiyExistsForGroup; // good job whoever named this lol
        objData.m_opacityActionsForGroup = m_effectManager->m_opacityActionsForGroup;

        bool isFlipping = this->isFlipping();
        const int sectCount = m_sectionObjects->count();
        const int firstVisibleSection = std::clamp(as<int>(floorf(m_cameraPosition.x / 100.f) - 1.f), 0, sectCount);
        const int lastVisibleSection = std::clamp(as<int>(ceilf((m_cameraPosition.x + winSize.width) / 100.f) + 1.f), 0, sectCount);

        // get furthest points
        const int furthestLeftSection = std::min(firstVisibleSection, m_firstVisibleSection);
        const int furthestRightSection = std::max(lastVisibleSection, m_lastVisibleSection);
        
        CCRect screenViewRect(m_cameraPosition.x, m_cameraPosition.y, winSize.width, winSize.height); 

        ccColor3B curBGColor = m_background->getColor();
        objData.bgColorHSV = this->transformColor(curBGColor, 0, -0.3f, .4f);

        objData.LBGColor = m_effectManager->activeColorForIndex(1007);

        const ccColor3B BGColor = m_effectManager->activeColorForIndex(1000);
        objData.curBGColorHSV = this->transformColor(BGColor, 0, -0.2f, .2f);

        objData.isBGColorDark = BGColor.r + BGColor.g + BGColor.b < 150;
        objData.m_isDead = m_player1->m_isDead;

        objData.playerPosX = winSize.width / 2 - 75.f;
        objData.playerPosX_rightSide = objData.playerPosX + 110.f; // I have no idea what this is supposed to be called
        objData.screenRightOffset = screenRight - objData.playerPosX_rightSide - 90.f;

        auto pool = m_fields->threadPool.get();

        // get object count
        int objectsInVisibleSections = 0;
        for(size_t i = furthestLeftSection; i <= furthestRightSection; i++) {
            if(i >= 0 && i < sectCount) {
                auto objArr = as<CCArray*>(m_sectionObjects->objectAtIndex(i));

                objectsInVisibleSections += objArr->data->num;

                // copy array
                copyArrayToArray(objArr, m_objectsToUpdate);
            }
        }

        // calculate chunk
        const int threadCount = pool->getThreadCount();
        int chunkSize = objectsInVisibleSections / threadCount;
        bool singleThread = false;

        if(chunkSize == 0 && objectsInVisibleSections > 0) {
            chunkSize = objectsInVisibleSections;
            singleThread = true;
        }

        g_allowQuadUpdate = false;
        TextureAtlas::setAtlasQueueing(true);

        // update objects
        for(size_t i = 0; i < threadCount; i++) {
            const int from = chunkSize * i;

            int to = chunkSize * (i + 1);
            if(i + 1 >= threadCount) {
                to = objectsInVisibleSections;
            }

            pool->queueTask([this, from, to, &objData, &firstVisibleSection, &lastVisibleSection, &furthestLeftSection, &furthestRightSection, &sectCount, &screenViewRect] {
                for(size_t i = from; i < to; i++) {
                    auto obj = as<GameObject*>(m_objectsToUpdate->objectAtIndex(i));

                    const int section = obj->m_section;

                    // hide object from last frame
                    if(section >= m_lastVisibleSection && section <= m_firstVisibleSection) {
                        obj->m_shouldHide = true;
                    }

                    // object not toggled off
                    if(!obj->m_groupDisabled) {
                        const CCRect& texRect = obj->getObjectTextureRect();
                        const bool intersectsSW = screenViewRect.intersectsRect(texRect);

                        // object in edges of visible sections
                        if(section <= firstVisibleSection + 1 || section >= lastVisibleSection - 1) {
                            // object in view or (idk)
                            if(intersectsSW || obj->m_unk2e8) {
                                obj->m_shouldHide = false;
                            }
                            else {
                                obj->m_shouldHide = true;
                            }
                        }
                        else {
                            // object in visible sections
                            if(obj->m_active || intersectsSW || obj->m_unk2e8) {
                                obj->m_shouldHide = false;
                            }
                            else {
                                obj->m_shouldHide = true;
                            }
                        }
                    }
                    else {
                        obj->m_shouldHide = true;
                    }

                    // update object
                    CatnipPlayLayer::handleObject(obj, objData, m_fullReset);
                    //CatnipPlayLayer::updateGameObjectTS(obj, objData);
                }
                
                return true;
            });

            if(singleThread) break;
        }

        pool->waitForTasks();

        g_allowQuadUpdate = true;
        TextureAtlas::setAtlasQueueing(false);
        TextureAtlas::mapAtlasBuffers();

        // get objects to update
        #if 0
        for(size_t i = furthestLeftSection; i <= furthestRightSection; i++) {
            if(i >= 0 && i < sectCount) {
                auto objArr = as<CCArray*>(m_sectionObjects->objectAtIndex(i));

                for(size_t c = 0; c < objArr->count(); c++) {
                    auto obj = as<GameObject*>(objArr->objectAtIndex(c));

                    const int section = obj->m_section;

                    // hide object from last frame
                    if(section >= m_lastVisibleSection && section <= m_firstVisibleSection) {
                        obj->m_shouldHide = true;
                    }

                    // object not toggled off
                    if(!obj->m_groupDisabled) {
                        const CCRect& texRect = obj->getObjectTextureRect();
                        const bool intersectsSW = screenViewRect.intersectsRect(texRect);

                        // object in edges of visible sections
                        if(section <= firstVisibleSection + 1 || section >= lastVisibleSection - 1) {
                            // object in view or (idk)
                            if(intersectsSW || obj->m_unk2e8) {
                                obj->m_shouldHide = false;
                            }
                            else {
                                obj->m_shouldHide = true;
                            }
                        }
                        else {
                            // object in visible sections
                            if(obj->m_active || intersectsSW || obj->m_unk2e8) {
                                obj->m_shouldHide = false;
                            }
                            else {
                                obj->m_shouldHide = true;
                            }
                        }
                    }
                    else {
                        obj->m_shouldHide = true;
                    }

                    m_objectsToUpdate->addObject(obj);
                }
            }
        }

        // chunk size
        const int objCount = m_objectsToUpdate->data->num;
        const int threadCount = pool->getThreadCount();
        int chunkSize = objCount / threadCount;

        g_allowQuadUpdate = false;
        TextureAtlas::setAtlasQueueing(true);

        for(size_t i = 0; i < threadCount; i++) {
            const int from = chunkSize * i;

            int to = chunkSize * (i + 1);
            if(i + 1 >= threadCount) {
                to = objCount;
            }

            pool->queueTask([this, from, to, &objData] {
                // process chunk
                for(size_t i = from; i < to; i++) {
                    auto obj = as<GameObject*>(m_objectsToUpdate->data->arr[i]);

                    // update object
                    CatnipPlayLayer::handleObject(obj, objData, m_fullReset);
                    CatnipPlayLayer::updateGameObjectTS(obj, objData);
                }
                
                return true;
            });
        }

        pool->waitForTasks();

        g_allowQuadUpdate = true;
        TextureAtlas::setAtlasQueueing(false);
        TextureAtlas::mapAtlasBuffers();

        #endif

        // deactivate processed objects outside visible sections
        for(size_t i = 0; i < m_processedGroups->count(); i++) {
            auto obj = as<GameObject*>(m_processedGroups->objectAtIndex(i));
            if(obj->m_section < firstVisibleSection || obj->m_section > lastVisibleSection) {
                obj->deactivateObject(true);
            }
        }
        m_processedGroups->removeAllObjects();

        // finish
        m_fullReset = false;

        m_firstVisibleSection = firstVisibleSection;
        m_lastVisibleSection = lastVisibleSection;
        unk460 = false;
        unk464->removeAllObjects();

        m_hasColors.clear();

        // removeAllObjects iterates every object, releases it and sets num to 0
        // since we copy the array we can't retain every member due to performance
        // so we just reset num to 0
        // I'm sure this will cause no memory leaks!
        m_objectsToUpdate->data->num = 0;
    }

    bool init(GJGameLevel* level) {
        auto catnip = Mod::get();
        bool enabled = catnip->getSettingValue<bool>("mt-playlayer");

        // get hook
        for(auto& hook : catnip->getHooks()) {
            if(hook->getDisplayName() == "PlayLayer::updateVisibility") {
                // toggle hook
                enabled ? (void)catnip->enableHook(hook) : (void)catnip->disableHook(hook);

                break;
            }
        }

        // start thread pool
        if(enabled) {
            m_fields->threadPool = std::make_unique<CatnipThreadPool>(getMaxCatnipThreads() - 1);

            auto pool = m_fields->threadPool.get();
            pool->setContainResults(false);

            // create threads
            pool->startPool();
            pool->waitForTasks();

            CNLog("PlayLayer: Initialized CatnipThreadPool");
        }

        // change profiler title
        CNPROF_TITLE("PlayLayer execution times");

        return PlayLayer::init(level);
    }
};

// debug
#include <Geode/modify/UILayer.hpp>

class $modify(UILayer) {
    void keyDown(enumKeyCodes key) {
        // zoom out
        if(key == enumKeyCodes::KEY_S) {
            if(auto pLayer = PlayLayer::get()) {
                pLayer->m_objectLayer->setScale(.5f);
            }
        }

        // toggle profiler hooks
        /*if(key == enumKeyCodes::KEY_P) {
            std::vector<geode::Hook*> hooks;
            const std::vector<std::string> hookNames = { "" };

            geode::Hook* hook = nullptr;
            for(auto& h : Mod::get()->getHooks()) {
                if(!h->getDisplayName().compare("PlayLayer::updateVisibility")) {
                    hook = h;
                    break;
                }
            }
        }*/

        UILayer::keyDown(key);
    }

    void keyUp(enumKeyCodes key) {
        if(key == enumKeyCodes::KEY_S) {
            if(auto pLayer = PlayLayer::get()) {
                pLayer->m_objectLayer->setScale(1.f);
            }
        }

        // toggle hook
        if(key == enumKeyCodes::KEY_D) {
            geode::Hook* hook = nullptr;
            for(auto& h : Mod::get()->getHooks()) {
                if(!h->getDisplayName().compare("PlayLayer::updateVisibility")) {
                    hook = h;
                    break;
                }
            }

            if(hook) {
                if(hook->isEnabled()) {
                    (void)Mod::get()->disableHook(hook);
                }
                else {
                    (void)Mod::get()->enableHook(hook);
                }
            }
        }

        UILayer::keyUp(key);
    }
};

class $modify(CCSprite) {
    // make it "thread-safe"
    void updateColor() {
        if(g_allowQuadUpdate) {
            CCSprite::updateColor();
            return;
        }

        ccColor4B color4 = { _displayedColor.r, _displayedColor.g, _displayedColor.b, _displayedOpacity };
    
        // special opacity for premultiplied textures
        if(m_bOpacityModifyRGB) {
            color4.r *= _displayedOpacity / 255.0f;
            color4.g *= _displayedOpacity / 255.0f;
            color4.b *= _displayedOpacity / 255.0f;
        }

        m_sQuad.bl.colors = color4;
        m_sQuad.br.colors = color4;
        m_sQuad.tl.colors = color4;
        m_sQuad.tr.colors = color4;

        if(m_pobBatchNode) {
            // CCSpriteBatchNode::draw calls updateTransform, which updates the quads
            this->setDirty(true);
        }
    }
};

/*
    PROFILER HOOKS
*/
CN_DBGCODE(
    class $modify(GJEffectManager) {
        void updateColorEffects(float dt) {
            auto time = CNPROF_TIMECLASSVOIDFUNC([this, dt] {
                GJEffectManager::updateColorEffects(dt);
            });

            CNPROF_PUSHSTAT({ "GJEffectManager::updateColorEffects", time });
        }

        void updatePulseEffects(float dt) {
            auto time = CNPROF_TIMECLASSVOIDFUNC([this, dt] {
                GJEffectManager::updatePulseEffects(dt);
            });

            CNPROF_PUSHSTAT({ "GJEffectManager::updatePulseEffects", time });
        }

        void updateOpacityEffects(float dt) {
            auto time = CNPROF_TIMECLASSVOIDFUNC([this, dt] {
                GJEffectManager::updateOpacityEffects(dt);
            });

            CNPROF_PUSHSTAT({ "GJEffectManager::updateOpacityEffects", time });
        }

        void updateSpawnTriggers(float dt) {
            auto time = CNPROF_TIMECLASSVOIDFUNC([this, dt] {
                GJEffectManager::updateSpawnTriggers(dt);
            });

            CNPROF_PUSHSTAT({ "GJEffectManager::updateSpawnTriggers", time });
        }
    };
)