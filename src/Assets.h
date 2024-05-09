#pragma once

typedef struct
{
    uptr size;
    byte* data;
} LoadedFileData;

typedef enum
{
    ImageFormat_R = 0,
    ImageFormat_RGB,
    ImageFormat_RGBA,
    ImageFormat_RGB_DXT1,
    ImageFormat_RGBA_DXT5,

    ImageFormat_Unknown,
    _ImageFormat_Count
} ImageFormat;

typedef struct
{
    u32 width;
    u32 height;
    ImageFormat format;
    uptr dataSize;
    byte* data;
    char* name;
} ImageData;

LoadedFileData resLoadFile(CoreAPI* core, const char* filename, MemoryStack* stack);
ImageData resResizeImage(ImageData sourceImage, u32 newWidth, u32 newHeight, MemoryStack* stack);

ImageData resLoadImageFromFile(CoreAPI* core, const char* path, u32 numChannels, MemoryStack* stack);
ImageData resCompressImageDXT1(ImageData srcImage, MemoryStack* stack);
ImageData resCompressImageDXT5(ImageData srcImage, MemoryStack* stack);
