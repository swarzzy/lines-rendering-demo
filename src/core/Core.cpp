#include "SDL_Platform.h"
#include "../../ext/SDL2/include/SDL_syswm.h"

#include "Array.h"
//#include "../Intrinsics.h"
#include "CoreUtilities.h"

#include <errno.h>

#include "../ImGui/ImGuiApi.h"

#include <heapapi.h>

template<typename T>
constexpr T Max(T a, T b) {
    return a > b ? a : b;
}

template<typename T>
constexpr T Min(T a, T b) {
    return a < b ? a : b;
}

template<typename T>
constexpr T Clamp(T x, T min, T max) {
    return Min(Max(x, min), max);
}

f64 CoreGetTimeStamp()
{
    f64 frequency = (f64)SDL_GetPerformanceFrequency();
    f64 time = (f64)SDL_GetPerformanceCounter();
    time = time / frequency;
    return time;
}

void CoreGatherMouseMovement(CoreContext *context)
{
    if (context->state.input.activeApp)
    {
        int mousePositionX;
        int mousePositionY;

        // TODO(swarzzy): This function in not what I actually need. It gets mouse
        // coordinates from event messgaes, but I want to use immediate OS getter
        // (i.e. GetCursorPos (SDL version: SDL_GetGlobalMouseState)). But GetCursorPos
        // returns screen-relative coordinates and i need window-relative.
        // So for now just go with SDL_GetMouseState.
        SDL_GetMouseState(&mousePositionX, &mousePositionY);

        if ((mousePositionX != context->mousePosX) || (mousePositionY != context->mousePosY))
        {
            i32 offsetX = mousePositionX - context->mousePosX;
            i32 offsetY = -(mousePositionY - context->mousePosY);

            context->state.input.mouseFrameOffsetX = offsetX / (f32)context->state.currentDisplayParams.width;
            context->state.input.mouseFrameOffsetY = offsetY / (f32)context->state.currentDisplayParams.height;

            context->mousePosX = mousePositionX;
            context->mousePosY = mousePositionY;
            context->state.input.mouseX = (f32)mousePositionX;
            context->state.input.mouseY = (f32)mousePositionY;
            context->state.input.mouseX = mousePositionX / (f32)context->state.currentDisplayParams.width;
            context->state.input.mouseY = (context->state.currentDisplayParams.height - mousePositionY) / (f32)context->state.currentDisplayParams.height;
        }
    }
}

void CorePollEvents(CoreContext *context)
{
    CoreGatherMouseMovement(context);

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        context->imgui->ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type)
        {

        case SDL_WINDOWEVENT: {
            switch (event.window.event)
            {

            case SDL_WINDOWEVENT_SIZE_CHANGED: {
            }
            break;

            case SDL_WINDOWEVENT_ENTER: {
                context->state.input.mouseInWindow = true;
            }
            break;

            case SDL_WINDOWEVENT_LEAVE: {
                context->state.input.mouseInWindow = false;
            }
            break;

            case SDL_WINDOWEVENT_FOCUS_GAINED: {
                // TODO(swarzzy): This probably does not correspond to win32 api
                // implementation of platform layer. In win32 this flag is toggled
                // on WM_ACTIVEAPP event, but it seems like SDL throws this event
                // on WM_ACTIVE. SDL doc says that this event is for keyboard focus
                // [https://wiki.libsdl.org/SDL_WindowEventID]
                context->state.input.activeApp = true;
            }
            break;

            case SDL_WINDOWEVENT_FOCUS_LOST: {
                context->state.input.activeApp = false;
            }
            break;

            default: {
            }
            break;
            }
        }
        break;

        case SDL_MOUSEBUTTONDOWN: {
            MouseButton button = CoreMouseButtonConvert(event.button.button);
            context->state.input.mouseButtons[(u32)button].wasPressed = context->state.input.mouseButtons[(u32)button].pressedNow;
            context->state.input.mouseButtons[(u32)button].pressedNow = true;
            //printf("Mouse button pressed: %s\n", ToString(button));
        }
        break;

        case SDL_MOUSEBUTTONUP: {
            MouseButton button = CoreMouseButtonConvert(event.button.button);
            context->state.input.mouseButtons[(u32)button].wasPressed = context->state.input.mouseButtons[(u32)button].pressedNow;
            context->state.input.mouseButtons[(u32)button].pressedNow = false;
            //printf("Mouse button released: %s\n", ToString(button));
        }
        break;

        case SDL_MOUSEWHEEL: {
            // TODO(swarzzy): Check for SDL_MOUSEWHEEL_NORMAL and SDL_MOUSEWHEEL_FLIPPED
            // TODO(swarzzy): x!
            i32 steps = event.wheel.y;
            context->state.input.scrollOffset = steps;
            context->state.input.scrollFrameOffset = steps;
        }
        break;

        case SDL_KEYDOWN: {
            // TODO(swarzzy): Is SDL can throw KEYDOWN event which has state SDL_RELEASED???
            // We need test this
            Assert(event.key.state == SDL_PRESSED);

            // TODO(swarzzy): What it the real difference between scancodes and keycodes
            // and which of them we sould use

            // TODO(swarzzy): Repeat count

            u32 keyCode = (u32)CoreKeycodeConvert(event.key.keysym.sym);
            u16 repeatCount = event.key.repeat;
            //context->state.input.keys[keyCode].wasPressed = context->state.input.keys[keyCode].pressedNow;
            context->state.input.keys[keyCode].pressedNow = true;
        }
        break;

        case SDL_KEYUP: {
            Assert(event.key.state == SDL_RELEASED);

            u32 keyCode = (u32)CoreKeycodeConvert(event.key.keysym.sym);
            u16 repeatCount = event.key.repeat;
            //context->state.input.keys[keyCode].wasPressed = context->state.input.keys[keyCode].pressedNow;
            context->state.input.keys[keyCode].pressedNow = false;
        }
        break;

        case SDL_QUIT: {
            context->running = false;
        }
        break;
        default: {
        }
        break;
        }
    }
}

void CoreResizeWindow(CoreContext *context, DisplayMode mode, DisplayParams p)
{
    DisplayParams result{};
    SDL_SetWindowFullscreen(context->window, 0);

    if (mode == DisplayMode_BorderlessWindow)
    {
        SDL_SetWindowFullscreen(context->window, SDL_WINDOW_FULLSCREEN_DESKTOP);

        int w, h;
        SDL_GetWindowSize(context->window, &w, &h);

        result.width = w;
        result.height = h;
    }
    else
    {
        SDL_SetWindowSize(context->window, p.width, p.height);
        result = p;
    }

    context->state.currentDisplayMode = mode;
    context->state.currentDisplayParams = result;
}

void CoreResizeRendererBuffers(CoreContext *context)
{
    int w, h;
    SDL_GetWindowSize(context->window, &w, &h);

    DisplayParams params{};
    params.width = w;
    params.height = h;
    context->renderer->SetDisplayParams(params);
}

void CoreGetDisplayModes(CoreContext *context, DArray<DisplayParams> *buffer)
{
    int numModes = SDL_GetNumDisplayModes(0);

    if (numModes < 1)
        return;

    SDL_DisplayMode systemDisplayMode{};
    if (SDL_GetCurrentDisplayMode(0, &systemDisplayMode) != 0)
        return;

    DArray<SDL_DisplayMode> allModes(&context->allocator);
    allModes.Reserve(numModes);

    for (int i = 0; i < numModes; i++)
    {
        SDL_DisplayMode mode{};
        if (SDL_GetDisplayMode(0, i, &mode) == 0)
        {
            allModes.PushBack(mode);
        }
    }

    ForEach(&allModes, it)
    {
        if (buffer->FindFirst([&](auto i) { return i->width == it->w && i->height == it->h; }))
            continue;

        if (it->w >= 800 && it->h >= 600 && it->refresh_rate == systemDisplayMode.refresh_rate)
        {
            DisplayParams params{};
            params.width = it->w;
            params.height = it->h;
            buffer->PushBack(params);
        }
    }
    EndEach();

    allModes.FreeBuffers();
}

extern RendererAPI* InitializeRenderer_D3D11(CoreState* coreContext, uptr hwnd, DisplayParams desiredDisplayParams, DisplayParams* actualDisplayParams, struct ID3D11Device** device, struct ID3D11DeviceContext** deviceContext);

void InitGraphicsD3D11(CoreContext *context, const char *windowHeader, DisplayParams params, DisplayMode displayMode)
{
    HWND windowHandle = nullptr;
    context->window = SDL_CreateWindow(windowHeader, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, params.width, params.height, 0);
    auto hwnd = GetActiveWindow();

    DisplayParams actualParams {};
    struct ID3D11Device* device = nullptr;
    struct ID3D11DeviceContext* deviceContext = nullptr;
    context->renderer = InitializeRenderer_D3D11(&context->state, (uptr)hwnd, params, &actualParams, &device, &deviceContext);
    Assert(context->renderer);
    context->device = device;
    context->deviceContext = deviceContext;

    CoreResizeWindow(context, displayMode, actualParams);
}

MemoryHeap* CreateHeap()
{
    MemoryHeap* heap = (MemoryHeap*)HeapCreate(0, 0, 0);
    return heap;
}

void DestroyHeap(MemoryHeap* heap)
{
    HeapDestroy((HANDLE)heap);
}

#if defined(COMPILER_MSVC)
__declspec(restrict)
#endif
void *coreHeapAlloc(MemoryHeap *heap, uptr size, b32 zero)
{
    return HeapAlloc((HANDLE)heap, zero ? HEAP_ZERO_MEMORY : 0, size);
}

#if defined(COMPILER_MSVC)
__declspec(restrict)
#endif
void *coreHeapRealloc(MemoryHeap *heap, void *p, uptr size, b32 zero)
{
    return HeapReAlloc((HANDLE)heap, zero ? HEAP_ZERO_MEMORY : 0, p, size);
}

void coreHeapFree(MemoryHeap *heap, void *ptr)
{
    HeapFree((HANDLE)heap, 0, ptr);
}

static CoreContext *_GlobalCoreContext;

void CoreSetParameter(const CoreParameterData *param)
{
    auto context = _GlobalCoreContext;
    switch (param->param)
    {
    case CoreParameter_VSync: {
        context->renderer->SetVsyncMode(param->vsync);
        context->state.vsyncMode = param->vsync;
    }
    break;

    case CoreParameter_DisplayMode: {
        CoreResizeWindow(context, param->displayMode, context->state.currentDisplayParams);
        CoreResizeRendererBuffers(context);
    }
    break;

    case CoreParameter_DisplayParams: {
        CoreResizeWindow(context, _GlobalCoreContext->state.currentDisplayMode, param->displayParams);
        CoreResizeRendererBuffers(context);
    }
    break;

    case CoreParameter_FpsLockMode: {
        context->state.targetFramerate = param->fpsLockMode.targetFramerate == 0 ? 0 : Max(param->fpsLockMode.targetFramerate, 15u);
    }
    break;

    default: {
    }
    break;
    }
}

void CoreWriteLog(CoreLogLevel logLevel, const char* tags, u32 tagsCount, const char* format, va_list vlist)
{
    if (tags != NULL)
    {
        printf("[%s] ", tags);
    }
    vprintf(format, vlist);
}


void CoreInit(CoreContext *context, const char *windowName, GameUpdateAndRenderFn *gameUpdateAndRenderProc, GetImGuiAPIProc* imguiGetApiProc)
{
    Assert(gameUpdateAndRenderProc);
    context->gameUpdateAndRenderProc = gameUpdateAndRenderProc;

    Assert(imguiGetApiProc);
    context->imgui = imguiGetApiProc();
    Assert(context->imgui);

    auto heap = CreateHeap();
    Allocator allocator = Allocator(MemoryHeapAllocAPI, MemoryHeapFreeAPI, (void *)heap);
    context->allocator = allocator;

    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0)
    {
        Assert(false, "[SDL] Initialization failed: %s", SDL_GetError());
    }

    _GlobalCoreContext = context;
    context->running = true;

    context->state.coreAPI.OpenFile = PlatformOpenFile;
    context->state.coreAPI.GetFileSize = PlatformGetFileSize;
    context->state.coreAPI.ReadFile = PlatformReadFile;
    context->state.coreAPI.CloseFile = PlatformCloseFile;

    context->state.coreAPI.CreateHeap = CreateHeap;
    context->state.coreAPI.DestroyHeap = DestroyHeap;
    context->state.coreAPI.HeapAlloc = coreHeapAlloc;
    context->state.coreAPI.HeapRealloc = coreHeapRealloc;
    context->state.coreAPI.HeapFree = coreHeapFree;

    context->state.coreAPI.AllocatePages = PlatformAllocatePages;

    context->state.coreAPI.SetParameter = CoreSetParameter;
    context->state.coreAPI.WriteLog = CoreWriteLog;

    DisplayParams displayParams{};
    displayParams.width = 1920;
    displayParams.height = 1080;

    DisplayMode displayMode = DisplayMode_Window;

    InitGraphicsD3D11(context, windowName, displayParams, displayMode);

    context->state.rendererAPI = context->renderer;

    context->renderer->SetVsyncMode(VSyncMode_Full);
    context->state.vsyncMode = VSyncMode_Full;

    DArray<DisplayParams> displayConfigs(&context->allocator);
    CoreGetDisplayModes(context, &displayConfigs);
    context->state.availableDisplayConfigs = displayConfigs.Data();
    context->state.availableDisplayConfigsCount = displayConfigs.Count();

    IMGUI_CHECKVERSION(context->imgui);
    auto imguiContext = context->imgui->igCreateContext(nullptr);
    Assert(imguiContext);
    context->state.imguiContext = imguiContext;
    context->imgui->igStyleColorsDark(nullptr);

    context->imgui->ImGui_ImplSDL2_InitForD3D(context->window);
    context->imgui->ImGui_ImplDX11_Init(context->device, context->deviceContext);

    context->state.imgui = context->imgui;

    gameUpdateAndRenderProc(&context->state, GameInvoke_Init);
}

void NewFrameForImGui(CoreContext *context)
{
    context->imgui->ImGui_ImplDX11_NewFrame();
    context->imgui->ImGui_ImplSDL2_NewFrame();
    context->imgui->igNewFrame();
}

void EndFrameForImGui(CoreContext *context)
{
    context->imgui->igRender();

    context->imgui->ImGui_ImplDX11_RenderDrawData(context->imgui->igGetDrawData());
}

void CoreMainLoopProcessInput(CoreContext *context)
{
    for (u32 keyIndex = 0; keyIndex < ArrayCount(context->state.input.keys); keyIndex++)
    {
        context->state.input.keys[keyIndex].wasPressed = context->state.input.keys[keyIndex].pressedNow;
    }

    for (u32 mbIndex = 0; mbIndex < ArrayCount(context->state.input.mouseButtons); mbIndex++)
    {
        context->state.input.mouseButtons[mbIndex].wasPressed = context->state.input.mouseButtons[mbIndex].pressedNow;
    }

    CorePollEvents(context);
}

void CoreMainLoopUpdate(CoreContext *context, f32 deltaTime)
{
    context->state.tickCount++;
    context->state.updateDeltaTime = deltaTime;
    context->state.updateAbsDeltaTime = deltaTime;

    context->gameUpdateAndRenderProc(&context->state, GameInvoke_Update);
}

int VSyncToGLSwapInterval(VSyncMode mode)
{
    int swapInterval = 0;
    switch (mode)
    {
        case VSyncMode_Disabled: { swapInterval = 0; } break;
        case VSyncMode_Full: { swapInterval = 1; } break;
        case VSyncMode_Half: { swapInterval = 2; } break;
        default: {} break;
    }

    return swapInterval;
}

void CoreMainLoopRender(CoreContext *context, f32 lag)
{
    auto timestamp = CoreGetTimeStamp();
    auto frameTime = timestamp - context->lastRenderTime;
    context->lastRenderTime = timestamp;
    // If framerate lower than 15 fps just clamping delta time
    auto deltaTime = (f32)Clamp(frameTime, 0.0, 0.066);

    context->state.frameCount++;
    context->state.renderDeltaTime = deltaTime;
    context->state.renderLag = lag;

    NewFrameForImGui(context);

    context->renderer->SetViewport(MakeVector2(0.0f, 0.0f), MakeVector2((f32)context->state.currentDisplayParams.width, (f32)context->state.currentDisplayParams.height));
    context->gameUpdateAndRenderProc(&context->state, GameInvoke_Render);
    //context->renderer->Clear(MakeVector4(1.0f, 0.0f, 0.0f, 1.0f));

    EndFrameForImGui(context);
    context->renderer->SwapScreenBuffers();
}

Key CoreKeycodeConvert(i32 sdlKeycode)
{
    // TODO(swarzzy): Test this
    switch (sdlKeycode)
    {
    case SDLK_BACKSPACE:
        return Key_Backspace;
    case SDLK_TAB:
        return Key_Tab;
    case SDLK_CLEAR:
        return Key_Clear;
    case SDLK_RETURN:
        return Key_Enter;
    case SDLK_LSHIFT:
        return Key_LeftShift;
    case SDLK_LCTRL:
        return Key_LeftCtrl;
    case SDLK_LALT:
        return Key_LeftAlt;
    case SDLK_RSHIFT:
        return Key_RightShift;
    case SDLK_RCTRL:
        return Key_RightCtrl;
    case SDLK_RALT:
        return Key_RightAlt;
    case SDLK_PAUSE:
        return Key_Pause;
    case SDLK_CAPSLOCK:
        return Key_CapsLock;
    case SDLK_ESCAPE:
        return Key_Escape;
    case SDLK_SPACE:
        return Key_Space;
    case SDLK_PAGEUP:
        return Key_PageUp;
    case SDLK_PAGEDOWN:
        return Key_PageDown;
    case SDLK_END:
        return Key_End;
    case SDLK_HOME:
        return Key_Home;
    case SDLK_LEFT:
        return Key_Left;
    case SDLK_UP:
        return Key_Up;
    case SDLK_RIGHT:
        return Key_Right;
    case SDLK_DOWN:
        return Key_Down;
    case SDLK_PRINTSCREEN:
        return Key_PrintScreen;
    case SDLK_INSERT:
        return Key_Insert;
    case SDLK_DELETE:
        return Key_Delete;
    case SDLK_0:
        return Key_0;
    case SDLK_1:
        return Key_1;
    case SDLK_2:
        return Key_2;
    case SDLK_3:
        return Key_3;
    case SDLK_4:
        return Key_4;
    case SDLK_5:
        return Key_5;
    case SDLK_6:
        return Key_6;
    case SDLK_7:
        return Key_7;
    case SDLK_8:
        return Key_8;
    case SDLK_9:
        return Key_9;
    case SDLK_a:
        return Key_A;
    case SDLK_b:
        return Key_B;
    case SDLK_c:
        return Key_C;
    case SDLK_d:
        return Key_D;
    case SDLK_e:
        return Key_E;
    case SDLK_f:
        return Key_F;
    case SDLK_g:
        return Key_G;
    case SDLK_h:
        return Key_H;
    case SDLK_i:
        return Key_I;
    case SDLK_j:
        return Key_J;
    case SDLK_k:
        return Key_K;
    case SDLK_l:
        return Key_L;
    case SDLK_m:
        return Key_M;
    case SDLK_n:
        return Key_N;
    case SDLK_o:
        return Key_O;
    case SDLK_p:
        return Key_P;
    case SDLK_q:
        return Key_Q;
    case SDLK_r:
        return Key_R;
    case SDLK_s:
        return Key_S;
    case SDLK_t:
        return Key_T;
    case SDLK_u:
        return Key_U;
    case SDLK_v:
        return Key_V;
    case SDLK_w:
        return Key_W;
    case SDLK_x:
        return Key_X;
    case SDLK_y:
        return Key_Y;
    case SDLK_z:
        return Key_Z;
    case SDLK_LGUI:
        return Key_LeftSuper;
    case SDLK_RGUI:
        return Key_RightSuper;
    case SDLK_KP_0:
        return Key_Num0;
    case SDLK_KP_1:
        return Key_Num1;
    case SDLK_KP_2:
        return Key_Num2;
    case SDLK_KP_3:
        return Key_Num3;
    case SDLK_KP_4:
        return Key_Num4;
    case SDLK_KP_5:
        return Key_Num5;
    case SDLK_KP_6:
        return Key_Num6;
    case SDLK_KP_7:
        return Key_Num7;
    case SDLK_KP_8:
        return Key_Num8;
    case SDLK_KP_9:
        return Key_Num9;
    case SDLK_KP_MULTIPLY:
        return Key_NumMultiply;
    case SDLK_KP_PLUS:
        return Key_NumAdd;
    case SDLK_KP_MINUS:
        return Key_NumSubtract;
    case SDLK_KP_DIVIDE:
        return Key_NumDivide;
    case SDLK_KP_ENTER:
        return Key_NumEnter;
    // They are the same
    case SDLK_KP_DECIMAL:
        return Key_NumDecimal;
    case SDLK_KP_PERIOD:
        return Key_NumDecimal;

    case SDLK_F1:
        return Key_F1;
    case SDLK_F2:
        return Key_F2;
    case SDLK_F3:
        return Key_F3;
    case SDLK_F4:
        return Key_F4;
    case SDLK_F5:
        return Key_F5;
    case SDLK_F6:
        return Key_F6;
    case SDLK_F7:
        return Key_F7;
    case SDLK_F8:
        return Key_F8;
    case SDLK_F9:
        return Key_F9;
    case SDLK_F10:
        return Key_F10;
    case SDLK_F11:
        return Key_F11;
    case SDLK_F12:
        return Key_F12;
    case SDLK_F13:
        return Key_F13;
    case SDLK_F14:
        return Key_F14;
    case SDLK_F15:
        return Key_F15;
    case SDLK_F16:
        return Key_F16;
    case SDLK_F17:
        return Key_F17;
    case SDLK_F18:
        return Key_F18;
    case SDLK_F19:
        return Key_F19;
    case SDLK_F20:
        return Key_F20;
    case SDLK_F21:
        return Key_F21;
    case SDLK_F22:
        return Key_F22;
    case SDLK_F23:
        return Key_F23;
    case SDLK_F24:
        return Key_F24;
    case SDLK_NUMLOCKCLEAR:
        return Key_NumLock;
    case SDLK_SCROLLLOCK:
        return Key_ScrollLock;
    case SDLK_APPLICATION:
        return Key_Menu;
    case SDLK_SEMICOLON:
        return Key_Semicolon;
    case SDLK_EQUALS:
        return Key_Equal;
    case SDLK_COMMA:
        return Key_Comma;
    case SDLK_MINUS:
        return Key_Minus;
    case SDLK_PERIOD:
        return Key_Period;
    case SDLK_SLASH:
        return Key_Slash;
    case SDLK_BACKQUOTE:
        return Key_Tilde;
    case SDLK_LEFTBRACKET:
        return Key_LeftBracket;
    case SDLK_BACKSLASH:
        return Key_BackSlash;
    case SDLK_RIGHTBRACKET:
        return Key_RightBracket;
    case SDLK_QUOTE:
        return Key_Apostrophe;
    default:
        return Key_Invalid;
    }
}

MouseButton CoreMouseButtonConvert(u8 button)
{
    switch (button)
    {
    case SDL_BUTTON_LEFT:
        return MouseButton_Left;
    case SDL_BUTTON_RIGHT:
        return MouseButton_Right;
    case SDL_BUTTON_MIDDLE:
        return MouseButton_Middle;
    case SDL_BUTTON_X1:
        return MouseButton_XButton1;
    case SDL_BUTTON_X2:
        return MouseButton_XButton2;
        InvalidDefault();
    }
    // Dummy return for the compiler
    return MouseButton_Left;
}
