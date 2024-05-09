#pragma once

// NOTE: This file is shared by both platform layer and game library
// It defines plaform API. When platform calls game code it passes PlatformState
// structure to the game. This structure contains pointers to all platform functions
// (including OpenGL calls) and other necessary stuff such as current input state.

#include "Common.h"
#include "../renderer/RendererAPI.h"
#include "../ImGui/ImGuiApi.h"

#if defined(PLATFORM_WINDOWS)
#define GAME_CODE_ENTRY __declspec(dllexport)
#elif defined(PLATFORM_LINUX)
#define GAME_CODE_ENTRY
#error Unsupported OS
#endif

typedef enum
{
    GameInvoke_Init,
    GameInvoke_Update,
    GameInvoke_Render,
} GameInvoke;

typedef void (__cdecl GameUpdateAndRenderFn)(struct _CoreState*, GameInvoke);

typedef uptr FileHandle;

typedef enum
{
    OpenFileMode_Read,
    OpenFileMode_Write
} OpenFileMode;

typedef enum
{
    CoreParameter_VSync,
    CoreParameter_DisplayMode,
    CoreParameter_DisplayParams,
    CoreParameter_FpsLockMode
} CoreParameter;

typedef struct
{
    CoreParameter param;
    union
    {
        VSyncMode vsync;
        DisplayMode displayMode;
        DisplayParams displayParams;
        struct { u32 targetFramerate; } fpsLockMode;
    };
} CoreParameterData;

inline CoreParameterData CreateCoreParameter(CoreParameter p)
{
    CoreParameterData data = {0};
    data.param = p;
    return data;
}

inline CoreParameterData CreateCoreParameter_VSync(VSyncMode mode)
{
    CoreParameterData data = {0};
    data.param = CoreParameter_VSync;
    data.vsync = mode;
    return data;
}

inline CoreParameterData CreateCoreParameter_DisplayMode(DisplayMode mode)
{
    CoreParameterData data = {0};
    data.param = CoreParameter_DisplayMode;
    data.displayMode = mode;
    return data;
}

inline CoreParameterData CreateCoreParameter_DisplayParams(DisplayParams params)
{
    CoreParameterData data = {0};
    data.param = CoreParameter_DisplayParams;
    data.displayParams = params;
    return data;
}

typedef enum
{
    CoreLogLevel_Trace,
    CoreLogLevel_Info,
    CoreLogLevel_Warning,
    CoreLogLevel_Error,
    CoreLogLevel_Fatal,
} CoreLogLevel;

typedef struct
{
    void* memory;
    uptr actualSize;
    uptr pageSize;
} PagesAllocationResult;

typedef struct
{
    FileHandle(*OpenFile)(const char* filename, OpenFileMode mode);
    i64(*GetFileSize)(FileHandle handle);
    i64(*ReadFile)(FileHandle handle, void* buffer, i64 bufferSize);
    b32(*CloseFile)(FileHandle handle);

    struct MemoryHeap*(*CreateHeap)();
    void(*DestroyHeap)(struct MemoryHeap* heap);
    void*(*HeapAlloc)(struct MemoryHeap* heap, uptr size, b32 zero);
    void*(*HeapRealloc)(struct MemoryHeap* heap, void* p, uptr size, b32 zero);
    void(*HeapFree)(struct MemoryHeap* heap, void* ptr);

    PagesAllocationResult(*AllocatePages)(uptr desiredSize);

    void(*SetParameter)(const CoreParameterData* param);

    void(*WriteLog)(CoreLogLevel logLevel, const char* tags, u32 tagsCount, const char* format, va_list vlist);
} CoreAPI;

typedef struct
{
    u8 pressedNow;
    u8 wasPressed;
} KeyState;

typedef struct
{
    u8 pressedNow;
    u8 wasPressed;
} MouseButtonState;

typedef struct
{
    KeyState keys[256];
    MouseButtonState mouseButtons[5];
    b32 mouseInWindow;
    b32 activeApp;
    // All mouse position values are normalized
    f32 mouseX;
    f32 mouseY;
    f32 mouseFrameOffsetX;
    f32 mouseFrameOffsetY;
    // Not normalized
    i32 scrollOffset;
    i32 scrollFrameOffset;
} InputState;

// NOTE: Platform State is supposed to be read only.
// Use PlatformParameter to change platform behaviour
typedef struct _CoreState
{
    CoreAPI coreAPI;
    RendererAPI* rendererAPI;

    ImGuiContext* imguiContext;
    ImGuiApi* imgui;

    InputState input;

    u64 tickCount;
    u64 frameCount;
    f32 updateAbsDeltaTime;
    f32 updateDeltaTime;
    f32 renderDeltaTime;
    f32 renderLag;

    DisplayParams currentDisplayParams;
    DisplayMode currentDisplayMode;

    VSyncMode vsyncMode;
    u32 targetFramerate;
    u32 availableDisplayConfigsCount;
    DisplayParams* availableDisplayConfigs;
} CoreState;
