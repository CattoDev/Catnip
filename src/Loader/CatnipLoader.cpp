#include "CatnipLoader.hpp"

using namespace geode::prelude;

CatnipLoader* g_instance = nullptr;

std::string writeTimeToString(ghc::filesystem::file_time_type time) {
    time_t t = std::chrono::system_clock::to_time_t(time);
    std::string ts = std::ctime(&t);
    ts.resize(ts.length() - 1);

    return ts;
}

template<>
struct json::Serialize<TexCacheSaving> {
    static TexCacheSaving from_json(json::Value const& value) {
        TexCacheSaving data;

        auto vec = value["file-writetimes"].as_array();

        for(auto& obj : vec) {
            data.fileWriteTimes.push_back({ obj["file"].as_string(), obj["path"].as_string(), obj["lastWriteTime"].as_string() });
        }

        return data;
    }

    static json::Value to_json(TexCacheSaving const& value) {
        auto obj = json::Object();

        json::Array vec;

        for(auto& cData : value.fileWriteTimes) {
            auto timeObj = json::Object();

            timeObj["file"] = cData.fileName;
            timeObj["path"] = cData.filePath;
            timeObj["lastWriteTime"] = cData.writeTimeStr;

            vec.push_back(timeObj);
        } 

        obj["file-writetimes"] = vec;

        return obj;
    }
};

CatnipLoader* CatnipLoader::get() {
    if(!g_instance) {
        g_instance = new CatnipLoader();
    }

    return g_instance;
}

void CatnipLoader::setup() {
    m_cacheSavingData = Mod::get()->getSavedValue<TexCacheSaving>("tex-cache-data", TexCacheSaving {});
     
    // determine quality
    int sf = static_cast<int>(CCDirector::sharedDirector()->getContentScaleFactor());

    switch(sf) {
        case 1:
            m_loadedQuality = 2;
            break;

        case 2:
            m_loadedQuality = 1;
            break;

        default:
            m_loadedQuality = 0;
            break;
    }
}

bool CatnipLoader::getFullPathForFile(std::string fileName, std::string fileExt, ghc::filesystem::path& retPath) {
    bool found = false;

    // get path
    auto utils = CCFileUtils::sharedFileUtils();
    for(std::string p : utils->getSearchPaths()) {
        ghc::filesystem::path path;
        if(this->getFileAtPath(p, fileName, path, fileExt)) {
            retPath = path;
            found = true;
            break;
        }
    }

    return found;
}

bool CatnipLoader::getFileAtPath(ghc::filesystem::path directory, std::string fileName, ghc::filesystem::path& retPath, std::string extensionCut) {
    const std::vector<std::string> qualExt = { "-uhd", "-hd", "" };
    bool found = false;

    for(size_t i = m_loadedQuality; i < qualExt.size(); i++) {
        auto& ext = qualExt[i];
        std::string name = fileName;
        
        // quality extension
        name.append(ext);
        name.append(extensionCut);

        // check if exists
        const auto path = directory / name;
        if(ghc::filesystem::exists(path)) {
            retPath = path;
            found = true;
            break;
        }
    }

    return found;
}

bool CatnipLoader::fileHasBeenModified(ghc::filesystem::path filePath, ghc::filesystem::path _fileName) {
    // get filename
    std::string fileName = _fileName.string();

    std::string curTimeStr = writeTimeToString(ghc::filesystem::last_write_time(filePath));
    
    bool modified = true;
    bool cached = false;

    for(auto& cData : m_cacheSavingData.fileWriteTimes) {
        if(!cData.fileName.compare(fileName)) {
            cached = true;

            if(!cData.writeTimeStr.compare(curTimeStr) && !filePath.string().compare(cData.filePath)) {
                modified = false;
            }

            cData.writeTimeStr = curTimeStr;
            cData.filePath = filePath.string();
        }
    }

    // set last write time
    if(!cached) {
        m_cacheSavingData.fileWriteTimes.push_back({ fileName, filePath.string(), curTimeStr });
    }

    return modified;
}

void CatnipLoader::saveCachedTexData() {
    Mod::get()->setSavedValue("tex-cache-data", m_cacheSavingData);

    (void)Mod::get()->saveData();
}

bool CatnipLoader::fileIsCurrentQuality(ghc::filesystem::path filePath) {
    auto fileName = filePath.filename().string();

    // get rid of extension
    size_t lastPeriod = fileName.find_last_of(".");
    if(lastPeriod != fileName.npos) {
        fileName = fileName.substr(0, lastPeriod);
    }

    // check quality
    bool result = false;
    
    size_t lastDash = fileName.find_last_of("-");
    if(lastDash != fileName.npos) {
        // get quality extension
        std::string qualExt = fileName.substr(lastDash);

        result = (
            (!qualExt.compare("-uhd") && m_loadedQuality == 0) // high
        ||  (!qualExt.compare("-hd") && m_loadedQuality == 1) // medium
        );
    }
    else {
        // low
        if(m_loadedQuality == 2) {
            result = true;
        }
    }

    return result;
}

bool CatnipLoader::fileIsImage(std::string fileName) {
    return !strcmp(fileName.substr(fileName.length() - 4).c_str(), ".png");
}

std::string CatnipLoader::getQualityExt() {
    const std::vector<std::string> qualExt = { "-uhd", "-hd", "" };

    return qualExt[m_loadedQuality];
}

CatnipLoader::TexturesVec CatnipLoader::getAllTexturesToLoad() {
    // hardcoded filenames
    auto textures = TexturesVec {
        { "GJ_GameSheet" }, { "GJ_GameSheet02" }, { "GJ_GameSheet03" }, { "GJ_GameSheet04" }, { "GJ_GameSheetGlow" }, { "FireSheet_01" }, { "GJ_ShopSheet" }, { "CCControlColourPickerSpriteSheet", kCCTexture2DPixelFormat_RGBA8888 }, // sheets
        { "smallDot" }, { "square01_001" }, { "square02_001" }, { "square02b_001" },
        { "GJ_gradientBG", kCCTexture2DPixelFormat_RGBA8888 }, { "edit_barBG_001", kCCTexture2DPixelFormat_RGBA8888 }, { "GJ_button_01", kCCTexture2DPixelFormat_RGBA8888 }, { "slidergroove2", kCCTexture2DPixelFormat_RGBA8888 }, { "sliderBar2", kCCTexture2DPixelFormat_RGBA8888 }, 
        { "GJ_square01" }, { "GJ_square02" }, { "GJ_square03" }, { "GJ_square04" }, { "GJ_square05" },
        { "bigFont" }, { "chatFont" }, // fonts
        { "groundSquare_01_001" }, { "streak_01_001" }, { "loadingCircle" }, { "square02b_small" }, 
        { "GJ_button_02" }, { "GJ_button_03" }, { "GJ_button_04" }, { "GJ_button_05" }, { "GJ_button_06" } 
    };

    // set paths
    for(TexListMeta& meta : textures) {
        // image path
        ghc::filesystem::path imgPath;
        if(this->getFullPathForFile(meta.fileName, ".png", imgPath)) {
            meta.imagePath = imgPath;
        }
        else {
            log::error("Failed to find image for {}!", meta.fileName);
        }

        // plist path
        ghc::filesystem::path plistPath;
        if(this->getFullPathForFile(meta.fileName, ".plist", plistPath)) {
            meta.plistPath = plistPath;
        }
        // not every image has a plist
    }

    // get mod textures
    if(Mod::get()->getSettingValue<bool>("mt-mod-tex")) {
        auto ldr = Loader::get();
        auto ldrMod = ldr->getModImpl();
        auto mods = ldr->getAllMods();

        ghc::filesystem::path resPath;

        // mod resources
        for(auto const& mod : mods) {
            // geode.loader has a specific path
            ghc::filesystem::path sheetResPath;
            if(mod == ldrMod) {
                resPath = geode::dirs::getGeodeResourcesDir();
                sheetResPath = resPath;
            }
            else {
                resPath = mod->getResourcesDir();
                sheetResPath = resPath.parent_path();
            }

            size_t qualExtLen = this->getQualityExt().length();

            // is mod enabled
            if(ldrMod->getSavedValue<bool>(fmt::format("should-load-{}", mod->getID())) || mod == ldrMod) {
                log::debug("searching path: {}", resPath);

                if(ghc::filesystem::exists(resPath)) {
                    // load spritesheets
                    if(!mod->getMetadata().getSpritesheets().empty()) {
                        for(std::string& sheet : mod->getMetadata().getSpritesheets()) {
                            std::string sheetExt = sheet + this->getQualityExt();

                            TexListMeta meta(sheet);
                            meta.imagePath = sheetResPath / (sheetExt + ".png");
                            meta.plistPath = sheetResPath / (sheetExt + ".plist");
                            meta.forbidCache = true;

                            textures.push_back(meta);
                        }
                    }
                    
                    // load images
                    if(Mod::get()->getSettingValue<bool>("preload-mod-tex")) {
                        // iterate files
                        for(ghc::filesystem::path const& file : ghc::filesystem::directory_iterator(resPath)) {
                            // image is current quality
                            std::string fileName = file.filename().string();
                            if(this->fileIsImage(fileName) &&this->fileIsCurrentQuality(file)) {
                                std::string withoutExt = fileName;
                                withoutExt.resize(withoutExt.length() - 4 - qualExtLen);

                                TexListMeta meta(mod->getID() + "\\" + withoutExt);
                                meta.imagePath = file;
                                meta.forbidCache = true;

                                textures.push_back(meta);
                            }
                        }
                    }
                }
            }
        }
    }

    return textures;
}