// nocheckin
#define CPLUSPLUS

#undef APIENTRY

#include "Common.h"
#include "CoreAPI.h"
#include "Core.h"

#include <windows.h>
// DPI awareness stuff
#include <shellscalingapi.h>
// For timeBeginPeriod
#include <mmsystem.h>

#include "../ImGui/ImGuiApi.h"

static LARGE_INTEGER GlobalPerformanceFrequency;

f64 Win32GetTimestamp()
{
    f64 time = 0.0;
    LARGE_INTEGER currentTime = {};
    QueryPerformanceCounter(&currentTime);
    time = (f64)(currentTime.QuadPart) / (f64)GlobalPerformanceFrequency.QuadPart;
    return time;
}

void WaitForTargetFps(f64 frameBeginTime, u32 targetFps)
{
    f64 frameTime = Win32GetTimestamp() - frameBeginTime;
    f64 requestedFrameTime = 1.0 / targetFps;

    while (frameTime < requestedFrameTime)
    {
        DWORD timeToWait = (DWORD)((requestedFrameTime - frameTime) * 1000.0);
        if (timeToWait > 0)
        {
            Sleep(timeToWait);
        }
        frameTime = Win32GetTimestamp() - frameBeginTime;
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    CoreContext context {};

    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    QueryPerformanceFrequency(&GlobalPerformanceFrequency);

    UINT sleepGranularityMs = 1;
    auto granularityWasSet = (timeBeginPeriod(sleepGranularityMs) == TIMERR_NOERROR);

    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    LogPrint("%s\n", PLATFORM_WINDOW_HEADER_VALUE);

    HMODULE gameLibrary = LoadLibraryW(L"Game.dll");
    GameUpdateAndRenderFn* gameUpdateAndRenderProc = (GameUpdateAndRenderFn*)GetProcAddress(gameLibrary, "GameUpdateAndRender");
    Assert(gameUpdateAndRenderProc, "Failed to load game library.\n");

    HMODULE imguiLibrary = LoadLibraryW(L"ImGui.dll");
    GetImGuiAPIProc* imguiGetApiProc = (GetImGuiAPIProc*)GetProcAddress(imguiLibrary, IMGUI_GET_API_PROC_NAME);
    Assert(imguiGetApiProc, "Failed to load ImGUI library.\n");

    CoreInit(&context, PLATFORM_WINDOW_HEADER_VALUE, gameUpdateAndRenderProc, imguiGetApiProc);

    f64 lastTimeStamp = Win32GetTimestamp();
    f64 lag = 0.0;

    while(context.running)
    {
        f64 currentTimeStep = Win32GetTimestamp();
        f64 deltaTime = currentTimeStep - lastTimeStamp;
        lastTimeStamp = currentTimeStep;
        lag += deltaTime;

        const f64 updateDelay = 1.0 / 60.0;
        while (lag >= updateDelay)
        {
            CoreMainLoopProcessInput(&context);
            CoreMainLoopUpdate(&context, updateDelay);
            lag -= updateDelay;
        }

        CoreMainLoopRender(&context, (f32)(lag / updateDelay));

        if (granularityWasSet && context.state.vsyncMode == VSyncMode_Disabled && context.state.targetFramerate > 0)
        {
            WaitForTargetFps(currentTimeStep, context.state.targetFramerate);
        }
    }
    return 0;
}

#include "Core.cpp"
#include "CoreUtilities.cpp"
