#include <Geode/Geode.hpp>

#include <Geode/modify/LoadingLayer.hpp>
#include <Geode/modify/CCFileUtils.hpp>

#include "CCDictMaker.hpp"
#include "CCTextureCacheExtra.hpp"
#include "CCSpriteFrameCacheExtra.hpp"
#include "CachedTexture.hpp"
#include "CacheManager.hpp"
#include <Loader/CatnipLoader.hpp>

#include <Utils.hpp>
#include <ThreadPool/ThreadPool.hpp>

using namespace geode::prelude;

struct DictLoadData {
    CCDictionary* instance = nullptr;
    ghc::filesystem::path filePath;
    ghc::filesystem::path fileName;
};

std::mutex texCacheLock;
class $modify(CatnipLoadingLayer, LoadingLayer) {
    CCLabelBMFont* loadingLabel = nullptr;
    bool cacheTextures = false;

    std::vector<std::pair<std::string, CCTexture2D*>> allTextures;

    static std::string getPlistPathFromSheet(std::string sheetPath) {
        std::string plistPath = sheetPath;

        // literally just replace .png with .plist
        size_t lastPeriod = sheetPath.find_last_of(".");
        if(lastPeriod != sheetPath.npos) {
            plistPath = sheetPath.substr(0, lastPeriod);
        }
        plistPath.append(".plist");

        return plistPath;
    }

    void initTextures(CatnipThreadPool& tPool, std::vector<CCTexture2DThreaded*>& texDataVec) {
        while(true) {
            bool poolFinished;

            int idx = tPool.getFinishedResultIdx(poolFinished);

            if(idx >= 0) {
                auto texData = texDataVec.at(idx);

                texData->texture->initWithData(texData->textureData, texData->pixelFormat, texData->width, texData->height, texData->imageSize);

                // cleanup
                delete [] texData->textureData;

                // cache
                m_fields->allTextures.push_back({ texData->fileName, texData->texture });

                auto texDict = CCTextureCacheExtra::get()->getTexturesDict();
                texDict->setObject(texData->texture, texData->filePath.string().c_str());

                CC_SAFE_DELETE(texData);
            }
            else if(poolFinished) break;
        }

        texDataVec.clear();

        log::debug("Textures initialized!");
    }

    void arrayCleanup() {
        for(auto& tex : m_fields->allTextures) {
            tex.second->release();
        }

        m_fields->allTextures.clear();
    }

    static bool createDictionary(DictLoadData* dictData) {
        CCDictMaker maker;
        auto dictPath = getPlistPathFromSheet(dictData->filePath.string());
        dictData->instance = maker.dictionaryWithContentsOfFile(dictPath.c_str());

        return true;
    }

    void addSpecialDicts(std::vector<std::string>& specialPlists) {
        CCAnimateFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile(specialPlists.front().c_str());
        specialPlists.erase(specialPlists.begin());

        for(auto& plist : specialPlists) {
            CCContentManager::sharedManager()->addDict(plist.c_str(), false);
        }
    }

    static bool createTextureFromCache(CCTexture2DThreaded* data) {
        CachedTexture cTex;
        
        CNLog("Creating {} from cache...", data->filePath);
        bool success = CacheManager::loadCachedTexture(data->filePath, cTex);

        if(success) {
            data->texture = new CCTexture2D();
            data->textureData = cTex.textureData;
            data->dataSize = cTex.textureDataSize;
            data->width = cTex.width;
            data->height = cTex.height;
            data->imageSize = CCSizeMake((float)(data->width), (float)(data->height));
            data->successfulLoad = true;
        }
        else {
            log::warn("Could not find cache for {}!", data->filePath);
        }

        return success;
    }

    static bool createTextureTask(CCTexture2DThreaded* data) {

        if(!data->useCache || !createTextureFromCache(data)) {
            CNLog("Creating {} from resources...", data->filePath);

            createTextureFromFileTS(data);
            
            // cache image
            if(data->g_cachingEnabled && data->createCache) {
                CNLog("Caching image {}", data->filePath);
                CacheManager::cacheProcessedImage(data);
            }
        }

        if(data->successfulLoad) {
            return true;
        }

        return false;
    }
 
    void startThreadedLoading() {
        this->updateLoadingLabel("Loading started");

        auto fileUtils = CCFileUtils::sharedFileUtils();
        auto texCache = CCTextureCacheExtra::get();
        auto sprCache = CCSpriteFrameCacheExtra::get();

        m_fields->cacheTextures = Mod::get()->getSettingValue<bool>("preprocessed-images");

        CatnipTimer::start();

        // setup Loader
        CatnipLoader::get()->setup();

        // every texture
        auto cnl = CatnipLoader::get();
        auto textures = cnl->getAllTexturesToLoad();
        CatnipLoader::SVec specialPlists = { "Robot_AnimDesc.plist", "glassDestroy01.plist", "coinPickupEffect.plist", "explodeEffect.plist" };

        this->updateLoadingLabel("Loading textures");

        // start thread pool (-1 cuz we do stuff in this thread aswell)
        CatnipThreadPool tPool(getMaxCatnipThreads() - 1);

        tPool.startPool();

        // queue textures
        std::string qualStr = cnl->getQualityExt();

        std::vector<CCTexture2DThreaded*> texDataVec;
        texDataVec.reserve(textures.size());

        int idx = 0;
        for(TexListMeta& meta : textures) {
            auto texData = new CCTexture2DThreaded();

            texData->fileName = meta.fileName;
            texData->filePath = meta.imagePath;
            texData->pixelFormat = meta.pixFmt;
            texData->g_cachingEnabled = m_fields->cacheTextures;

            if(m_fields->cacheTextures && !meta.forbidCache) {
                texData->useCache = !cnl->fileHasBeenModified(meta.imagePath, meta.fileName + qualStr + ".png");
                texData->createCache = true;
            }

            texDataVec.push_back(texData);

            tPool.queueTask([texData] { 
                return CatnipLoadingLayer::createTextureTask(texData);
            }, idx);

            idx++;
        }

        // OpenGL is single threaded so textures have to be initialized in the main thread
        ObjectToolbox::sharedState();

        this->initTextures(tPool, texDataVec);

        // finish loading textures
        tPool.waitForTasks();

        // save cache data
        if(m_fields->cacheTextures) {
            cnl->saveCachedTexData();
        }

        this->updateLoadingLabel("Loading plists");

        // load plists
        std::vector<DictLoadData*> dictDatas;
        idx = 0;
        {
            for(TexListMeta& meta : textures) {
                if(meta.plistPath.empty()) {
                    continue;
                }

                auto data = new DictLoadData();

                data->fileName = meta.fileName;
                data->filePath = meta.plistPath;
                
                dictDatas.push_back(data);

                tPool.queueTask([data] {
                    return CatnipLoadingLayer::createDictionary(data);
                }, idx);

                idx++;
            }

            this->addSpecialDicts(specialPlists);

            while(true) {
                bool poolFinished;

                int idx = tPool.getFinishedResultIdx(poolFinished);

                if(idx >= 0) {
                    auto dictData = dictDatas.at(idx);

                    for(auto& t : m_fields->allTextures) {
                        if(!dictData->fileName.compare(t.first)) {
                            sprCache->addSpriteFramesWithDict(dictData->instance, t.second);
                            break;
                        }
                    }

                    CC_SAFE_DELETE(dictData);
                }
                else if(poolFinished) {
                    break;
                }
            }

            tPool.waitForTasks();
        }

        // end thread pool
        tPool.terminatePool();

        // other
        CCTextInputNode::create(200, 50, "Temp", 1, "bigFont.fnt");

        // cleanup
        this->arrayCleanup();

        // load geode
        if(!Mod::get()->getSettingValue<bool>("mt-mod-tex")) {
            Loader::get()->updateResources(true);
        }

        this->updateLoadingLabel("Loading finished");
    }

    void updateLoadingLabel(const char* status) {
        std::string str = fmt::format("Catnip [{} threads]: {}", getMaxCatnipThreads(), status);
        m_fields->loadingLabel->setCString(str.c_str());

        CNLog("Loading label: {}", str);

        // redraw frame
        CCDirector::sharedDirector()->getRunningScene()->visit();
        CCEGLView::sharedOpenGLView()->swapBuffers();
    }

    /*
        HOOKS
    */
    bool init(bool fromReload) {
        if(!LoadingLayer::init(fromReload))
            return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // loading label
        m_fields->loadingLabel = CCLabelBMFont::create("Catnip: Setting up", "goldFont.fnt");
        m_fields->loadingLabel->setPosition(winSize / 2 + CCPoint(0, -60));
        m_fields->loadingLabel->setScale(.4);
        this->addChild(m_fields->loadingLabel, 10);

        // hide loading bar
        for(size_t i = 0; i < this->getChildrenCount(); i++) {
            if(auto spr = typeinfo_cast<CCSprite*>(this->getChildren()->objectAtIndex(i))) {
                if(spr->getZOrder() == 3) {
                    spr->setVisible(false);
                    break;
                }
            }
        }
    
        return true;
    }

    void loadAssets() {
        this->startThreadedLoading();

        // finish loading
        m_loadStep = 14;
        LoadingLayer::loadAssets();

        log::info("Game loaded in {}ms", CatnipTimer::endWithStr());
    }
    
    /*static void onModify(auto& self) {
        (void)self.setHookPriority("LoadingLayer::loadAssets", 0); // just incase
    }*/
};

class $modify(CCFileUtils) {
    gd::string fullPathForFilename(const char* fileName, bool unk) {
        auto path = CCFileUtils::fullPathForFilename(fileName, unk);

        // fix path cuz cocos is fucking stupid
        std::string fixedPath = path;

        for(size_t i = 0; i < fixedPath.length(); i++) {
            char c = fixedPath[i];

            if(c == '/') {
                fixedPath[i] = '\\';
            }
        }

        return gd::string(fixedPath);
    }
};

// testing
#include <Geode/modify/CCTextureCache.hpp>

std::mutex addImageLock;
class $modify(CCTextureCache) {
    CCTexture2D* addImage(const char* imgPath, bool idk) {
        addImageLock.lock();

        std::string pathKey = CCFileUtils::sharedFileUtils()->fullPathForFilename(imgPath, false);
        if(!m_pTextures->objectForKey(pathKey.c_str())) {
            log::warn("IMAGE NOT PRELOADED: {}", pathKey);
        }

        auto res = CCTextureCache::addImage(imgPath, idk);
        addImageLock.unlock();

        return res;
    }
};	