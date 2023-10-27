#include <Geode/Geode.hpp>

#include <Geode/modify/LoadingLayer.hpp>

#include "CCDictMaker.hpp"
#include "CCTextureCacheExtra.hpp"
#include "CCSpriteFrameCacheExtra.hpp"

#include <Utils.hpp>
#include <ThreadPool/ThreadPool.hpp>
//#include <BS_thread_pool.hpp>
#include <future>

using namespace geode::prelude;

struct DictLoadData {
	CCDictionary* instance;
	std::string filePath;
};

std::mutex texCacheLock;
class $modify(CatnipLoadingLayer, LoadingLayer) {
	bool geodeLoaded = false;
	CCLabelBMFont* loadingLabel;
	int loadingStage = 0;

	std::vector<std::pair<std::string, CCTexture2D*>> allTextures;

	std::string getFullPath(std::string _resPath, std::string fileName, bool plist = false) {
		std::string fullPath = _resPath + fileName;
		std::string fileExt = plist ? ".plist" : ".png";

		// quality (TODO)
		if(ghc::filesystem::exists(fullPath + std::string("-uhd") + fileExt)) {
			fullPath.append("-uhd");
		}
		else if(ghc::filesystem::exists(fullPath + std::string("-hd") + fileExt)) {
			fullPath.append("-hd");
		}

		fullPath.append(fileExt);

		return fullPath;
	}

	static std::string getPlistPathFromSheet(std::string sheetPath) {
		// literally just replace .png with .plist
		std::string plistPath = sheetPath.substr(0, sheetPath.length() - 3) + std::string("plist");

		return plistPath;
	}

	void _initTextures(std::vector<std::future<CCTexture2DThreaded>>& texFutures) {
		for(auto& future : texFutures) {
			auto texData = future.get();

			// init
			texData.texture->initWithData(texData.textureData, texData.pixelFormat, texData.width, texData.height, texData.imageSize);

			// cleanup
			delete [] texData.textureData;

			// cache
			m_fields->allTextures.push_back({ texData.filePath, texData.texture });

			auto texDict = CCTextureCacheExtra::get()->getTexturesDict();
			texDict->setObject(texData.texture, texData.filePath.c_str());

			log::debug("Initialized texture {}", texData.filePath);
		}

		texFutures.clear();
	}

	void initTextures(CatnipThreadPool& tPool) {
		while(true) {
			auto _data = tPool.tryGetFinishedResult();

			if(_data.valid) {
				auto texData = std::any_cast<CCTexture2DThreaded>(_data.data);

				// init
				texData.texture->initWithData(texData.textureData, texData.pixelFormat, texData.width, texData.height, texData.imageSize);

				// cleanup
				delete [] texData.textureData;

				// cache
				m_fields->allTextures.push_back({ texData.filePath, texData.texture });

				auto texDict = CCTextureCacheExtra::get()->getTexturesDict();
				texDict->setObject(texData.texture, texData.filePath.c_str());
			}
			else if(tPool.poolFinished()) break;
		}
	}

	void arrayCleanup() {
		for(auto& tex : m_fields->allTextures) {
			tex.second->release();
		}

		m_fields->allTextures.clear();
	}

	static CCDictionary* createDictionary(std::string _dictPath) {
		CCDictMaker maker;
		auto dictPath = getPlistPathFromSheet(_dictPath);
		auto dict = maker.dictionaryWithContentsOfFile(dictPath.c_str());

		//log::debug("Loaded plist: {}", dictPath);

		return dict;
	}

	void addSpecialDicts(std::vector<std::string>& specialPlists) {
		CCAnimateFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile(specialPlists.front().c_str());
		specialPlists.erase(specialPlists.begin());

		for(auto& plist : specialPlists) {
			CCContentManager::sharedManager()->addDict(plist.c_str(), false);
		}
	}

	void startThreadedLoading() {
		auto fileUtils = CCFileUtils::sharedFileUtils();
		auto texCache = CCTextureCacheExtra::get();
		auto sprCache = CCSpriteFrameCacheExtra::get();
		
		CatnipTimer::start();

		// get absolute resouces path
		std::string resPath;
		for(auto& path : fileUtils->getSearchPaths()) {
			if(std::strstr(path.c_str(), "Resources")) {
				resPath = std::string(path.c_str());
				break;
			}
		}

		// spritesheets
		const std::vector<std::string> gameSheets = { "GJ_GameSheet", "GJ_GameSheet02", "GJ_GameSheet03", "GJ_GameSheet04", "GJ_GameSheetGlow", "FireSheet_01", "GJ_ShopSheet", "CCControlColourPickerSpriteSheet" };
		const int sheetCount = gameSheets.size();
		std::vector<std::string> gameSheetPaths;

		// every texture
		#define SVec std::vector<std::string>
		#define Pair std::make_pair
		const std::vector<std::pair<SVec, CCTexture2DPixelFormat>> textures = {
			Pair(gameSheets, CCTexture2D::defaultAlphaPixelFormat()), // gamesheets
			Pair(SVec { "smallDot", "square02_001" }, CCTexture2D::defaultAlphaPixelFormat()),
			Pair(SVec { "GJ_gradientBG", "edit_barBG_001", "GJ_button_01", "slidergroove2", "sliderBar2" }, CCTexture2DPixelFormat::kTexture2DPixelFormat_RGBA8888), // these use a specific pixel format
			Pair(SVec { "GJ_square01", "GJ_square02", "GJ_square03", "GJ_square04", "GJ_square05", "gravityLine_001" }, CCTexture2D::defaultAlphaPixelFormat()),
			Pair(SVec { "goldFont", "bigFont" }, CCTexture2D::defaultAlphaPixelFormat()) // fonts
		};

		// separate plists
		SVec specialPlists = { "Robot_AnimDesc.plist", "glassDestroy01.plist", "coinPickupEffect.plist", "explodeEffect.plist" };

		this->updateLoadingLabel("Loading textures");

		// start thread pool (-1 cuz we do stuff in this thread aswell)
		CatnipThreadPool tPool(std::thread::hardware_concurrency() - 1);

		tPool.startPool();

		// queue textures
		bool firstIter = true;
		for(auto& t : textures) {
			for(size_t i = 0; i < t.first.size(); i++) {
				CCTexture2DThreaded texData;

				texData.filePath = this->getFullPath(resPath, t.first[i]);
				texData.pixelFormat = t.second;

				if(firstIter) {
					gameSheetPaths.push_back(texData.filePath);
				}

				tPool.queueTask(DataLoadingType(texData, [](std::any& _d) {
					auto data = std::any_cast<CCTexture2DThreaded>(_d);
					createTextureFromFileTS(data);

					if(data.successfulLoad) {
						_d = data;
						return true;
					}

					return false;
				}));
			}
			firstIter = false;
		}
		tPool.tasksAdded();

		// OpenGL is single threaded so textures have to be initialized in the main thread
		ObjectToolbox::sharedState();

		this->initTextures(tPool);

		// finish loading textures
		tPool.waitForTasks();


		tPool.startPool();
		// load spriteframe plists
		{
			for(auto& path : gameSheetPaths) {
				DictLoadData data;
				data.filePath = path;

				tPool.queueTask(DataLoadingType(data, [](std::any& _d) {
					auto data = std::any_cast<DictLoadData>(_d);

					data.instance = CatnipLoadingLayer::createDictionary(data.filePath);

					_d = data;
					
					return true;
				}));
			}
			tPool.tasksAdded();

			this->addSpecialDicts(specialPlists);

			while(true) {
				auto _data = tPool.tryGetFinishedResult();

				if(_data.valid) {
					auto dictData = std::any_cast<DictLoadData>(_data.data);

					for(auto& t : m_fields->allTextures) {
						if(!dictData.filePath.compare(t.first)) {
							sprCache->addSpriteFramesWithDict(dictData.instance, t.second);
							break;
						}
					}
				}
				else if(tPool.poolFinished()) break;
			}

			tPool.waitForTasks();
		}

		// other
		CCTextInputNode::create(200, 50, "Temp", 1, "bigFont.fnt");

		// cleanup
        this->arrayCleanup();

		this->updateLoadingLabel("Loading finished");

		// finish loading
		m_loadStep = 9;
		this->loadAssets();

		log::info("Game loaded in {}ms", CatnipTimer::endWithStr());
	}

	void updateLoadingLabel(const char* status) {
		loadingLabel->setCString(fmt::format("Catnip [{}t]: {}", std::thread::hardware_concurrency(), status).c_str());

		// refresh label
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
		loadingLabel = CCLabelBMFont::create("Catnip: Loading started", "goldFont.fnt");
		loadingLabel->setPosition(winSize.width / 2, 15);
		loadingLabel->setScale(.4);
		this->addChild(loadingLabel, 10);
	
		return true;
	}

	void loadAssets() {
		if(m_loadStep != 0) {
			// load geode
			if(m_fields->geodeLoaded) {
				m_loadStep = 14;
			}
			else {
				m_loadStep = 9;
				m_fields->geodeLoaded = true;
			}

			LoadingLayer::loadAssets();

			return;
		}

		this->startThreadedLoading();
	}
	
	static void onModify(auto& self) {
        (void)self.setHookPriority("LoadingLayer::loadAssets", 1024); // just incase
    }
};