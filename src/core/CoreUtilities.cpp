#include "CoreUtilities.h"

#include "../../ext/SDL2/include/SDL.h"

// TODO:
// TODO: On windows we definetly want our custom file io system sice SDL is doing all sorts
// TODO: of crazines and memory allocation hell in its implementation
// TODO:

#if defined(PLATFORM_WINDOWS)

#include <windows.h>

#define WIN32_PAGE_SIZE Kilobytes(4) // [https://devblogs.microsoft.com/oldnewthing/20210510-00/?p=105200]

PagesAllocationResult PlatformAllocatePagesInternal(uptr desiredSize, bool overflowProtection)
{
    uptr numPages = desiredSize / WIN32_PAGE_SIZE + ((desiredSize % WIN32_PAGE_SIZE) == 0 ? 0 : 1);

    void* memory = VirtualAlloc(0, WIN32_PAGE_SIZE * (numPages + (overflowProtection ? 2 : 0)) , MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (overflowProtection)
    {
        unsigned long ignored;
        VirtualProtect(memory, WIN32_PAGE_SIZE, PAGE_NOACCESS, &ignored);
        VirtualProtect(((char*)memory) + WIN32_PAGE_SIZE * (numPages + 1), WIN32_PAGE_SIZE, PAGE_NOACCESS, &ignored);
    }

    PagesAllocationResult result;
    result.memory = ((char*)memory) + WIN32_PAGE_SIZE;
    result.actualSize = numPages * WIN32_PAGE_SIZE;
    result.pageSize = WIN32_PAGE_SIZE;

    return result;
}

PagesAllocationResult PlatformAllocatePages(uptr desiredSize)
{
    // TODO: Disable pages protection in release build.
    return PlatformAllocatePagesInternal(desiredSize, true);
}
#endif

FileHandle PlatformOpenFile(const char* filename, OpenFileMode mode)
{
    FileHandle handle {};

    const char* modeString = nullptr;
    switch (mode)
    {
    case OpenFileMode_Write: { modeString = "wb"; } break;
    case OpenFileMode_Read: { modeString = "rb"; } break;
        InvalidDefault();
    }

    SDL_RWops* rwops = SDL_RWFromFile(filename, modeString);

    handle = (FileHandle)rwops;
    return handle;
}

i64 PlatformGetFileSize(FileHandle handle)
{
    i64 result = -1;

    SDL_RWops* ops = (SDL_RWops*)handle;

    if (SDL_RWseek(ops, 0, RW_SEEK_END))
    {
        result = SDL_RWtell(ops);
        SDL_RWseek(ops, 0, RW_SEEK_SET);
    }

    return result;
}

i64 PlatformReadFile(FileHandle handle, void* buffer, i64 bufferSize)
{
    Assert(bufferSize > 0);
    i64 result = -1;

    SDL_RWops* ops = (SDL_RWops*)handle;
    SDL_RWseek(ops, 0, RW_SEEK_SET);
    auto read = SDL_RWread(ops, buffer, (size_t)bufferSize, 1);
    if (read == 1)
    {
        result = read;
    }
    return result;
}

b32 PlatformCloseFile(FileHandle handle)
{
    b32 result = false;

    SDL_RWops* ops = (SDL_RWops*)handle;
    if (SDL_RWclose(ops))
    {
        result = true;
    }

    return result;
}
