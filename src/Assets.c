#include "Assets.h"
#include "Memory.h"

// TODO: Do this really thread local?
static _ThreadLocal MemoryStack* __stbiMemoryStackPtr = NULL;

#define STBI_MALLOC(size) __stbiMalloc(size)
static void* __stbiMalloc(size_t size)
{
    //Log_Trace("STB", "STB Image just ate %llu memory\n", size);
    return mmStackPush(__stbiMemoryStackPtr, size);
}

#define STBI_REALLOC(ptr, size) __stbiRealloc(ptr, size)
static void* __stbiRealloc(void* ptr, size_t size)
{
    //Log_Trace("STB", "STB Image just ate %llu memory\n", size);
    void* newMem = mmStackPush(__stbiMemoryStackPtr, size);
    if (ptr != NULL)
    {
        mmCopy(newMem, ptr, size);
    }
    return newMem;
}

#define STBI_FREE(ptr) (void)(ptr)

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_DXT_IMPLEMENTATION
#include <stb/stb_dxt.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_MALLOC(size, context) mmStackPush(context, size)
#define STBIR_FREE(ptr, context) ((void)(ptr), (void)(context))
#include <stb/stb_image_resize.h>

const int _ImageChannelsPerFormat[] =
{
    1, 4, 4, 4, 4
};

const ImageFormat _ImageFormatForChannels[] =
{
    ImageFormat_Unknown, ImageFormat_R, ImageFormat_Unknown, ImageFormat_RGB, ImageFormat_RGBA
};

LoadedFileData resLoadFile(CoreAPI* core, const char* filename, MemoryStack* stack)
{
    LoadedFileData result = {0};

    FileHandle handle = core->OpenFile(filename, OpenFileMode_Read);
    if (handle == 0)
    {
        Log_Error("LoadFile", "Failed to open file %s", filename);
        return result;
    }

    i64 fileSize = core->GetFileSize(handle);
    char* buffer = mmStackPush(stack, fileSize);
    if (buffer == NULL)
    {
        Log_Error("LoadFile", "Failed to open file %s. File is bigger than read temp buffer.", filename);
        return result;
    }

    i64 bytesRead = core->ReadFile(handle, buffer, fileSize);
    core->CloseFile(handle);

    result.size = fileSize;
    result.data = buffer;

    return result;
}

ImageData resResizeImage(ImageData sourceImage, u32 newWidth, u32 newHeight, MemoryStack* stack)
{
    Assert(sourceImage.format == ImageFormat_RGBA);
    Assert(newWidth % 4 == 0);
    Assert(newHeight % 4 == 0);

    ImageData result = {0};

    byte* data = mmStackPush(stack, newWidth * newHeight * 4);

    int filter = STBIR_FILTER_MITCHELL;
    int colorspace = STBIR_COLORSPACE_LINEAR;
    int r = stbir_resize_uint8_generic((unsigned char*)sourceImage.data, sourceImage.width, sourceImage.height, 0, data, newWidth, newHeight, 0, 4, 0, 0, STBIR_EDGE_ZERO, filter, colorspace, stack);
    if (r == 0)
    {
        // Error
        return result;
    }

    Log_Trace("Image Loader", "Resized image \"%s\" %lux%lu -> %lux%lu\n", sourceImage.name, sourceImage.width, sourceImage.height, newWidth, newHeight);

    result = sourceImage;
    result.width = newWidth;
    result.height = newHeight;
    result.data = (byte*)data;
    result.dataSize = newWidth * newHeight * 4;
    result.name = utf8CopyString(sourceImage.name, stack);
    return result;
}

ImageData resLoadImageFromFile(CoreAPI* core, const char* path, u32 numChannels, MemoryStack* stack)
{
    Assert(numChannels >= 1 && numChannels <= 4);
    ImageData result = {0};

    LoadedFileData fileData = resLoadFile(core, path, stack);
    if (fileData.data == NULL)
    {
        Log_Error("Image Loader", "Failed to read image file \"%s\".\n", path);
        return result;
    }

    __stbiMemoryStackPtr = stack;
    int width, height, n;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load_from_memory(fileData.data, (int)fileData.size, &width, &height, &n, numChannels);
    __stbiMemoryStackPtr = NULL;

    if (data == NULL)
    {
        Log_Error("Image Loader", "Failed to load image \"%s\".\n", path);
    }

    Log_Trace("Image Loader", "Loaded image from file \"%s\".\n", path);

    result.width = width;
    result.height = height;
    result.format = _ImageFormatForChannels[numChannels];
    result.dataSize = height * width * numChannels;
    result.data = data;
    result.name = utf8CopyString(path, stack);
    return result;
}

ImageData resCompressImageDXT(ImageData srcImage, MemoryStack* stack, i32 compressAlpha)
{
    Assert(srcImage.format == ImageFormat_RGBA);

    ImageData result = {0};

    if (((srcImage.width % 4) != 0) || ((srcImage.height % 4) != 0))
    {
        Log_Error("Image Loader", "DXT1 compression requires texture dimensions to be multiple of 4.\n");
        return result;
    }

    uptr orginalImageDataSize = srcImage.width * srcImage.height * 4;
    uptr blockSize = compressAlpha ? 16 : 8;
    Assert(orginalImageDataSize % (compressAlpha ? 4 : 8) == 0); // Ensure all correct.
    uptr compressedImageSize = orginalImageDataSize / (compressAlpha ? 4 : 8);
    byte* compressedData = mmStackPush(stack, compressedImageSize);
    byte* outPtr = compressedData;
    byte buffer[16 * 4]; // block of 4x4 pixels.

    for (u32 y = 0; y < srcImage.height; y += 4)
    {
        for (u32 x = 0; x < srcImage.width; x += 4)
        {
            mmCopy(buffer + 0  * 4, srcImage.data + ((y + 0) * srcImage.width + x) * 4, 16);
            mmCopy(buffer + 4  * 4, srcImage.data + ((y + 1) * srcImage.width + x) * 4, 16);
            mmCopy(buffer + 8  * 4, srcImage.data + ((y + 2) * srcImage.width + x) * 4, 16);
            mmCopy(buffer + 12 * 4, srcImage.data + ((y + 3) * srcImage.width + x) * 4, 16);

            stb_compress_dxt_block(outPtr, buffer, compressAlpha, STB_DXT_HIGHQUAL);

            outPtr += blockSize;
        }
    }

    Assert(outPtr <= compressedData + compressedImageSize);

    char orginalImageDataSizeStr[128];
    char compressedImageSizeStr[128];
    utf8PrettySize(orginalImageDataSizeStr, 128, orginalImageDataSize);
    utf8PrettySize(compressedImageSizeStr, 128, compressedImageSize);
    Log_Trace("Image Loader", "Compressed image \"%s\" with DXT1 (%s -> %s)\n", srcImage.name, orginalImageDataSizeStr, compressedImageSizeStr);

    result.width = srcImage.width;
    result.height = srcImage.height;
    result.format = compressAlpha ? ImageFormat_RGBA_DXT5 : ImageFormat_RGB_DXT1;
    result.dataSize = compressedImageSize;
    result.data = compressedData;
    result.name = utf8CopyString(srcImage.name, stack);

    return result;
}

ImageData resCompressImageDXT1(ImageData srcImage, MemoryStack* stack)
{
    return resCompressImageDXT(srcImage, stack, 0);
}

ImageData resCompressImageDXT5(ImageData srcImage, MemoryStack* stack)
{
    return resCompressImageDXT(srcImage, stack, 1);
}

#if false
    Assert(n == numChannels);

    if (format == ImageFormat_RGB_DXT1)
    {
        if (!mmIsPowerOfTwo(width) || !mmIsPowerOfTwo(height))
        {
            Log_Error("Image Loader", "Non-POT textures are not allowed with DXT1 compression. Image \"%s\".\n", path);
            return result;
        }

        u32 pixelsCount = width * height;
        u32 originalSize = pixelsCount * 4;
        u32 compressedSize = originalSize / 8;

        byte* compressed = mmStackPush(stack, compressedSize);
        byte* outPtr = compressed;
        byte buffer[512];

        for (i32 y = 0; y <= height; y += width * 4 * 4)
        {
            for (i32 x = 0; x <= width; x += 4 * 4)
            {
                mmCopy(buffer + 000, data + x + y + (width * 4 * 0), 128);
                mmCopy(buffer + 128, data + x + y + (width * 4 * 1), 128);
                mmCopy(buffer + 256, data + x + y + (width * 4 * 2), 128);
                mmCopy(buffer + 384, data + x + y + (width * 4 * 3), 128);

                stb_compress_dxt_block(outPtr, buffer, 0, STB_DXT_HIGHQUAL);
                outPtr += 64;
                Assert(outPtr <= compressed + compressedSize);
            }
        }

        result.width = width;
        result.height = height;
        result.format = format;
        result.dataSize = compressedSize;
        result.data = compressed;
    }
    else
    {
        result.width = width;
        result.height = height;
        result.format = format;
        result.dataSize = width * height * numChannels;
        result.data = data;
    }

    return result;
}
#endif
