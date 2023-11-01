#include "CachedTexture.hpp"

#include "Utils.hpp"
#include <filesystem>

void CachedTexture::saveFile(std::string filePath) {
    log::debug("Saving cached texture to file: {}", filePath);

    // open file
    FILE* fp = fopen(filePath.c_str(), "wb");

    // write image data
    fwrite(textureData, sizeof(unsigned char), textureDataSize, fp);

    // footer data
    {
        // pixelFormat + width + height (5 bytes)
        constexpr int footerSize = 5;
        unsigned char footer[footerSize];

        // pixel format
        footer[0] = static_cast<unsigned char>(pixelFormat);

        // width
        footer[1] = width & 0xff;
        footer[2] = (width >> 8) & 0xff;

        // height
        footer[3] = height & 0xff;
        footer[4] = (height >> 8) & 0xff;

        // write
        fwrite(footer, sizeof(unsigned char), footerSize, fp);
    }

    // close file
    fclose(fp);
}

bool CachedTexture::loadFile(std::string filePath) {
    if(!std::filesystem::exists(filePath)) {
        return false;
    }

    auto fileData = getFileData(filePath.c_str(), &textureDataSize);

    if(!fileData || textureDataSize == 0) {
        CC_SAFE_DELETE_ARRAY(fileData);
        return false;
    }

    // get footer data
    pixelFormat = static_cast<cocos2d::CCTexture2DPixelFormat>(fileData[textureDataSize - 5]);
    width = ((fileData[textureDataSize - 3] << 8) & 0xff00) | (fileData[textureDataSize - 4] & 0xff);
    height = ((fileData[textureDataSize - 1] << 8) & 0xff00) | (fileData[textureDataSize - 2] & 0xff);

    textureDataSize -= 5;

    // set texture data
    textureData = new unsigned char[textureDataSize];
    std::memcpy(textureData, fileData, textureDataSize);

    // cleanup
    delete [] fileData;

    return true;
}