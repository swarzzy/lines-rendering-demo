#pragma once

#include "Common.h"
#include "CoreAPI.h"
#include "Keys.h"

#define SDL_MAIN_HANDLED
#define HAVE_LIBC
#include "../../ext/SDL2/include/SDL.h"

#include "../ImGui/ImGuiApi.h"

#include "../renderer/RendererAPI.h"

#define LogPrint(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define Assert(expr, ...) assert(expr)
#define Debug_Assert(expr, ...) assert(expr)
// NOTE: Defined always
#define Panic(expr, ...) assert(expr)
#define Win32Call(xx) do { if (FAILED((xx))) {BreakDebug();}} while(false)

// NOTE: Allocator API
typedef void*(AllocateFn)(uptr size, b32 clear, uptr alignment, void* allocatorData);
typedef void(DeallocateFn)(void* ptr, void* allocatorData);

struct Allocator
{
    AllocateFn* allocate;
    DeallocateFn* deallocate;
    void* data;

    Allocator() = default;
    Allocator(AllocateFn* allocate, DeallocateFn* deallocate, void* data) : allocate(allocate), deallocate(deallocate), data (data) {}

    template <typename T> inline T* Alloc(bool clear = 1) { return (T*)allocate(sizeof(T), clear, 0, data); }
    template <typename T> inline T* Alloc(u32 count, bool clear = 1) { return (T*)allocate(sizeof(T) * count, clear, 0, data); }
    inline void* Alloc(uptr size, b32 clear, uptr alignment = 0) { return allocate(size, clear, alignment, data); }
    inline void Dealloc(void* ptr) { deallocate(ptr, data); }
};

struct CoreContext
{
    b32 running;

    SDL_Window* window;

    // Internal. Should not be used. Use values from PlatformState.input
    i32 mousePosX;
    i32 mousePosY;

    f64 lastUpdateTime;
    f64 lastRenderTime;

    CoreState state;

    RendererAPI* renderer;

    Allocator allocator;

    GameUpdateAndRenderFn* gameUpdateAndRenderProc;
    ImGuiApi* imgui;

    // Keep these for imgui
    struct ID3D11Device* device;
    struct ID3D11DeviceContext* deviceContext;
};

MemoryHeap* CreateHeap();
void DestroyHeap(MemoryHeap* heap);
void* coreHeapAlloc(MemoryHeap* heap, uptr size, b32 zero);
void* coreHeapRealloc(MemoryHeap* heap, void* p, uptr size, b32 zero);
void coreHeapFree(MemoryHeap* heap, void* ptr);

inline void* MemoryHeapAllocAPI(uptr size, b32 clear, uptr alignment, void* data) { return coreHeapAlloc((MemoryHeap*)data, (uptr)size, clear); }
inline void MemoryHeapFreeAPI(void* ptr, void* data) { coreHeapFree((MemoryHeap*)data, ptr); }

f64 CoreGetTimeStamp();
void CoreGatherMouseMovement(CoreContext* context, CoreState* platform);
void CorePollEvents(CoreContext* context);
void CoreInit(CoreContext *context, const char *windowName, GameUpdateAndRenderFn *gameUpdateAndRenderProc, GetImGuiAPIProc* imguiGetApiProc);

void CoreResizeWindow(CoreContext* context, DisplayMode mode, DisplayParams p);
void CoreResizeRendererBuffers(CoreContext* context);

void CoreMainLoopProcessInput(CoreContext* context);
void CoreMainLoopUpdate(CoreContext* context, f32 deltaTime);
void CoreMainLoopRender(CoreContext* context, f32 lag);

void CoreWriteLog(CoreLogLevel logLevel, const char* tags, u32 tagsCount, const char* format, va_list vlist);

Key CoreKeycodeConvert(i32 sdlKeycode);
MouseButton CoreMouseButtonConvert(u8 button);
