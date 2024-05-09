#pragma once

#include "Common.h"
#include "CoreAPI.h"

PagesAllocationResult PlatformAllocatePages(uptr desiredSize);

FileHandle PlatformOpenFile(const char* filename, OpenFileMode mode);
i64 PlatformGetFileSize(FileHandle handle);
i64 PlatformReadFile(FileHandle handle, void* buffer, i64 bufferSize);
b32 PlatformCloseFile(FileHandle handle);
