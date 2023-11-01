#ifndef __CATNIP_LOADER__
#define __CATNIP_LOADER__

#include <Geode/Geode.hpp>

#include <vector>
#include <string>

struct TexCacheData {
    std::string fileName;
    std::string filePath;
    std::string writeTimeStr;
};
struct TexCacheSaving {
    std::vector<TexCacheData> fileWriteTimes;
};
struct TexListMeta {
    std::string fileName;
    cocos2d::CCTexture2DPixelFormat pixFmt;
    ghc::filesystem::path imagePath;
    ghc::filesystem::path plistPath;
    bool forbidCache = false;

    TexListMeta(std::string file, cocos2d::CCTexture2DPixelFormat fmt = cocos2d::CCTexture2D::defaultAlphaPixelFormat()) {
        fileName = file;
        pixFmt = fmt;
    }
};

class CatnipLoader {
public:
    using SVec = std::vector<std::string>;
    using TexturesVec = std::vector<TexListMeta>;

public:
    TexCacheSaving m_cacheSavingData;
    size_t m_loadedQuality;

public:
    static CatnipLoader* get();
    void setup();

    bool getFullPathForFile(std::string, std::string, ghc::filesystem::path&);
    bool getFileAtPath(ghc::filesystem::path, std::string, ghc::filesystem::path&, std::string);
    bool fileHasBeenModified(ghc::filesystem::path, ghc::filesystem::path);
    void saveCachedTexData();
    bool fileIsCurrentQuality(ghc::filesystem::path);
    bool fileIsImage(std::string);
    std::string getQualityExt();
    TexturesVec getAllTexturesToLoad();
};

#endif