#include "Utils.hpp"

#include "../libraries/png/png.h"
#include <filesystem>

unsigned char* getFileData(const char* filePath, unsigned long *bytesRead) {
    *bytesRead = 0;
    unsigned char* fileData = NULL;

    FILE *fp = fopen(filePath, "rb");

    fseek(fp, 0, SEEK_END);
    *bytesRead = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fileData = new unsigned char[*bytesRead];
    *bytesRead = fread(fileData, sizeof(unsigned char), *bytesRead, fp);

    fclose(fp);

    return fileData;
}

void writeFileData(const char* filePath, unsigned char* data, unsigned long dataSize) {
    FILE *fp = fopen(filePath, "wb");

    fwrite(data, sizeof(data[0]), dataSize, fp);

    fclose(fp);
}

typedef struct 
{
    unsigned char* data;
    int size;
    int offset;
}tImageSource;

static void pngReadCallback(png_structp png_ptr, png_bytep data, png_size_t length)
{
    tImageSource* isource = (tImageSource*)png_get_io_ptr(png_ptr);

    if((int)(isource->offset + length) <= isource->size)
    {
        memcpy(data, isource->data + isource->offset, length);
        isource->offset += length;
    }
    else
    {
        png_error(png_ptr, "pngReaderCallback failed");
    }
}

#define CC_RGB_PREMULTIPLY_ALPHA(vr, vg, vb, va) \
    (unsigned)(((unsigned)((unsigned char)(vr) * ((unsigned char)(va) + 1)) >> 8) | \
    ((unsigned)((unsigned char)(vg) * ((unsigned char)(va) + 1) >> 8) << 8) | \
    ((unsigned)((unsigned char)(vb) * ((unsigned char)(va) + 1) >> 8) << 16) | \
    ((unsigned)(unsigned char)(va) << 24))

void createTextureFromFileTS(CCTexture2DThreaded* texData) {
    // get data
    unsigned long nDatalen = 0;
    unsigned char* pData = getFileData(texData->filePath.string().c_str(), &nDatalen);

    if(!pData || nDatalen == 0) {
        log::error("Failed to get file data for texture: {}", texData->filePath);
        
        texData->successfulLoad = false;
        return;
    }

    unsigned short m_nWidth, m_nHeight;
    int m_nBitsPerComponent;
    bool m_bHasAlpha = false, m_bPreMulti = false;

    unsigned char* m_pData;
    bool enoughMemory = true;

    // convert from png
    {
        #define PNGSIGSIZE  8

        png_byte        header[PNGSIGSIZE]   = {0}; 
        png_structp     png_ptr     =   0;
        png_infop       info_ptr    = 0;

        do 
        {
            // png header len is 8 bytes
            CC_BREAK_IF(nDatalen < PNGSIGSIZE);

            // check the data is png or not
            memcpy(header, pData, PNGSIGSIZE);
            CC_BREAK_IF(png_sig_cmp(header, 0, PNGSIGSIZE));

            // init png_struct
            png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
            CC_BREAK_IF(!png_ptr);

            // init png_info
            info_ptr = png_create_info_struct(png_ptr);
            CC_BREAK_IF(!info_ptr);

            CC_BREAK_IF(setjmp(png_jmpbuf(png_ptr)));

            // set the read call back function
            tImageSource imageSource;
            imageSource.data    = (unsigned char*)pData;
            imageSource.size    = nDatalen;
            imageSource.offset  = 0;
            png_set_read_fn(png_ptr, &imageSource, pngReadCallback);

            // read png header info

            // read png file info
            png_read_info(png_ptr, info_ptr);
            
            m_nWidth = png_get_image_width(png_ptr, info_ptr);
            m_nHeight = png_get_image_height(png_ptr, info_ptr);
            m_nBitsPerComponent = png_get_bit_depth(png_ptr, info_ptr);
            png_uint_32 color_type = png_get_color_type(png_ptr, info_ptr);
            
            // force palette images to be expanded to 24-bit RGB
            // it may include alpha channel
            if (color_type == PNG_COLOR_TYPE_PALETTE)
            {
                png_set_palette_to_rgb(png_ptr);
            }
            // low-bit-depth grayscale images are to be expanded to 8 bits
            if (color_type == PNG_COLOR_TYPE_GRAY && m_nBitsPerComponent < 8)
            {
                png_set_expand_gray_1_2_4_to_8(png_ptr);
            }
            // expand any tRNS chunk data into a full alpha channel
            if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
            {
                png_set_tRNS_to_alpha(png_ptr);
            }  
            // reduce images with 16-bit samples to 8 bits
            if (m_nBitsPerComponent == 16)
            {
                png_set_strip_16(png_ptr);            
            } 
            // expand grayscale images to RGB
            if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            {
                png_set_gray_to_rgb(png_ptr);
            }

            // read png data
            // m_nBitsPerComponent will always be 8
            m_nBitsPerComponent = 8;
            png_uint_32 rowbytes;
            png_bytep* row_pointers = (png_bytep*)malloc( sizeof(png_bytep) * m_nHeight );
            
            png_read_update_info(png_ptr, info_ptr);
            
            rowbytes = png_get_rowbytes(png_ptr, info_ptr);
            
            try {
                nDatalen = rowbytes * m_nHeight;
                m_pData = new unsigned char[nDatalen];
            }
            catch(std::bad_alloc& ex) {
                log::error("Not enough memory to load texture, delaying...");
                enoughMemory = false;
                break;
            }
            
            for (unsigned short i = 0; i < m_nHeight; ++i)
            {
                row_pointers[i] = m_pData + i*rowbytes;
            }
            png_read_image(png_ptr, row_pointers);

            png_read_end(png_ptr, NULL);
            
            png_uint_32 channel = rowbytes/m_nWidth;
            if (channel == 4)
            {
                m_bHasAlpha = true;

                // commented out cuz it causes weird texture bugs
                /*unsigned int *tmp = (unsigned int *)m_pData;
                for(unsigned short i = 0; i < m_nHeight; i++)
                {
                    for(unsigned int j = 0; j < rowbytes; j += 4)
                    {
                        *tmp++ = CC_RGB_PREMULTIPLY_ALPHA( row_pointers[i][j], row_pointers[i][j + 1], 
                                                        row_pointers[i][j + 2], row_pointers[i][j + 3] );
                    }
                }*/
                
                m_bPreMulti = true;
            }

            CC_SAFE_FREE(row_pointers);
        } while(0);

        if (png_ptr)
        {
            png_destroy_read_struct(&png_ptr, (info_ptr) ? &info_ptr : 0, 0);
        }
    }

    CC_SAFE_DELETE_ARRAY(pData);

    // init CCTexture2D
    if(!enoughMemory) {
        texData->successfulLoad = false;
    }
    else {
        unsigned char*            tempData = m_pData;
        unsigned int*             inPixel32  = NULL;
        unsigned char*            inPixel8 = NULL;
        unsigned short*           outPixel16 = NULL;
        CCSize                    imageSize = CCSizeMake((float)(m_nWidth), (float)(m_nHeight));
        CCTexture2DPixelFormat    _pixelFormat;
        size_t                    bpp = m_nBitsPerComponent;

        // compute pixel format
        if (m_bHasAlpha) {
            _pixelFormat = texData->pixelFormat;
        }
        else {
            if (bpp >= 8) {
                _pixelFormat = kCCTexture2DPixelFormat_RGB888;
            }
            else {
                _pixelFormat = kCCTexture2DPixelFormat_RGB565;
            }
        }
        
        // Repack the pixel data into the right format
        unsigned int length = m_nWidth * m_nHeight;

        if (_pixelFormat == kCCTexture2DPixelFormat_RGB565)
        {
            if (m_bHasAlpha)
            {
                // Convert "RRRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA" to "RRRRRGGGGGGBBBBB"
                nDatalen = m_nWidth * m_nHeight * 2;
                tempData = new unsigned char[nDatalen];
                outPixel16 = (unsigned short*)tempData;
                inPixel32 = (unsigned int*)m_pData;
                
                for(unsigned int i = 0; i < length; ++i, ++inPixel32)
                {
                    *outPixel16++ = 
                    ((((*inPixel32 >>  0) & 0xFF) >> 3) << 11) |  // R
                    ((((*inPixel32 >>  8) & 0xFF) >> 2) << 5)  |  // G
                    ((((*inPixel32 >> 16) & 0xFF) >> 3) << 0);    // B
                }

                texData->dataAltered = true;
            }
            else 
            {
                // Convert "RRRRRRRRRGGGGGGGGBBBBBBBB" to "RRRRRGGGGGGBBBBB"
                nDatalen = m_nWidth * m_nHeight * 2;
                tempData = new unsigned char[nDatalen];
                outPixel16 = (unsigned short*)tempData;
                inPixel8 = (unsigned char*)m_pData;
                
                for(unsigned int i = 0; i < length; ++i)
                {
                    *outPixel16++ = 
                    (((*inPixel8++ & 0xFF) >> 3) << 11) |  // R
                    (((*inPixel8++ & 0xFF) >> 2) << 5)  |  // G
                    (((*inPixel8++ & 0xFF) >> 3) << 0);    // B
                }

                texData->dataAltered = true;
            }    
        }
        else if (_pixelFormat == kCCTexture2DPixelFormat_RGBA4444)
        {
            // Convert "RRRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA" to "RRRRGGGGBBBBAAAA"
            nDatalen = m_nWidth * m_nHeight * 2;
            inPixel32 = (unsigned int*)m_pData;  
            tempData = new unsigned char[nDatalen];
            outPixel16 = (unsigned short*)tempData;
            
            for(unsigned int i = 0; i < length; ++i, ++inPixel32)
            {
                *outPixel16++ = 
                ((((*inPixel32 >> 0) & 0xFF) >> 4) << 12) | // R
                ((((*inPixel32 >> 8) & 0xFF) >> 4) <<  8) | // G
                ((((*inPixel32 >> 16) & 0xFF) >> 4) << 4) | // B
                ((((*inPixel32 >> 24) & 0xFF) >> 4) << 0);  // A
            }

            texData->dataAltered = true;
        }
        else if (_pixelFormat == kCCTexture2DPixelFormat_RGB5A1)
        {
            // Convert "RRRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA" to "RRRRRGGGGGBBBBBA"
            nDatalen = m_nWidth * m_nHeight * 2;
            inPixel32 = (unsigned int*)m_pData;   
            tempData = new unsigned char[nDatalen];
            outPixel16 = (unsigned short*)tempData;
            
            for(unsigned int i = 0; i < length; ++i, ++inPixel32)
            {
                *outPixel16++ = 
                ((((*inPixel32 >> 0) & 0xFF) >> 3) << 11) | // R
                ((((*inPixel32 >> 8) & 0xFF) >> 3) <<  6) | // G
                ((((*inPixel32 >> 16) & 0xFF) >> 3) << 1) | // B
                ((((*inPixel32 >> 24) & 0xFF) >> 7) << 0);  // A
            }

            texData->dataAltered = true;
        }
        else if (_pixelFormat == kCCTexture2DPixelFormat_A8)
        {
            // Convert "RRRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA" to "AAAAAAAA"
            nDatalen = m_nWidth * m_nHeight;
            inPixel32 = (unsigned int*)m_pData;
            tempData = new unsigned char[nDatalen];
            unsigned char *outPixel8 = tempData;
            
            for(unsigned int i = 0; i < length; ++i, ++inPixel32)
            {
                *outPixel8++ = (*inPixel32 >> 24) & 0xFF;  // A
            }

            texData->dataAltered = true;
        }
        
        if (m_bHasAlpha && _pixelFormat == kCCTexture2DPixelFormat_RGB888)
        {
            // Convert "RRRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA" to "RRRRRRRRGGGGGGGGBBBBBBBB"
            nDatalen = m_nWidth * m_nHeight * 3;
            inPixel32 = (unsigned int*)m_pData;
            tempData = new unsigned char[nDatalen];
            unsigned char *outPixel8 = tempData;
            
            for(unsigned int i = 0; i < length; ++i, ++inPixel32)
            {
                *outPixel8++ = (*inPixel32 >> 0) & 0xFF; // R
                *outPixel8++ = (*inPixel32 >> 8) & 0xFF; // G
                *outPixel8++ = (*inPixel32 >> 16) & 0xFF; // B
            }

            texData->dataAltered = true;
        }

        if(texData->dataAltered) {
            delete [] m_pData;
        }

        auto texture = new CCTexture2D();
        texture->m_bHasPremultipliedAlpha = m_bPreMulti;
        
        texData->texture = texture;
        texData->textureData = tempData;
        texData->imageSize = imageSize;
        texData->width = m_nWidth;
        texData->height = m_nHeight;
        texData->pixelFormat = _pixelFormat;
        texData->dataSize = nDatalen;
    }
}

std::chrono::high_resolution_clock::time_point timer;

void CatnipTimer::start() {
    timer = std::chrono::high_resolution_clock::now();
}

std::string CatnipTimer::endWithStr() {
    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - timer
    ).count();

    return fmt::format("{}", timeDiff);
}

void CatnipTimer::end() {
    log::info("{}ms", endWithStr());
}

void CatnipTimer::endNS() {
    auto timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now() - timer
    ).count();

    log::info("{}ns", timeDiff);
}

long long CatnipTimer::getResult() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now() - timer
    ).count();
}

unsigned int getMaxCatnipThreads() {
    return std::max(Mod::get()->getSettingValue<int64_t>("max-threads"), 2LL);
}

void copyArrayToArray(CCArray* from, CCArray* to) {
    // increase capacity
    if(to->data->num + from->data->num >= to->data->max) {
        to->data->max = to->data->num + from->data->num;

        CCObject** newArr = (CCObject**)realloc( to->data->arr, to->data->max * sizeof(CCObject*) );
        to->data->arr = newArr;
    }

    // copy array
    memcpy((void *)&to->data->arr[to->data->num], (void*)from->data->arr, sizeof(CCObject*) * from->data->num);

    to->data->num += from->data->num;
}