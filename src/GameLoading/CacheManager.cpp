#include "CacheManager.hpp"

ghc::filesystem::path CacheManager::getCachePath() {
    auto cacheDir = Mod::get()->getSaveDir() / "cached";
    auto cacheDirStr = cacheDir.c_str();

    // create folder if it doesn't exist
    if(!ghc::filesystem::exists(cacheDirStr)) {
        ghc::filesystem::create_directory(cacheDirStr);
    }

    return cacheDir;
}

ghc::filesystem::path CacheManager::getCacheFiletype(ghc::filesystem::path _fileName) {
    std::string fileName = _fileName.string();

    size_t lastPeriod = fileName.find_last_of(".");
    if(lastPeriod != fileName.npos) {
        fileName = fileName.substr(0, lastPeriod);
    }

    fileName.append(".cnimg");
    
    return fileName;
}

void CacheManager::cacheProcessedImage(CCTexture2DThreaded* texData) {
    // get path
    auto filePath = getCachePath() / getCacheFiletype(texData->filePath.filename());

    // save data to file
    CachedTexture cTex;

    cTex.textureData = texData->textureData;
    cTex.pixelFormat = texData->pixelFormat;
    cTex.textureDataSize = texData->dataSize;
    cTex.width = texData->width;
    cTex.height = texData->height;

    cTex.saveFile(filePath.string());
}

bool CacheManager::loadCachedTexture(ghc::filesystem::path filePath, CachedTexture& cTex) {
    // get filename
    auto fileName = getCacheFiletype(filePath.filename());

    // get cache path
    auto cachePath = getCachePath() / fileName;

    // get texture
    return cTex.loadFile(cachePath.string());
}