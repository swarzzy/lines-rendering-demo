#include "core/Common.h"
#include "core/CoreAPI.h"
//#include "MetaprogramRuntime.h"

#include <stdlib.h>
//#include "../ext/meow_hash/meow_hash_x64_aesni.h"

//#include "Reflection.h"

//#if !defined(META_PASS)
//#include "Reflection.Generated.h"
//#endif

#define Assert(x) assert(x)

#include "Game.h"
#include "ImGui/ImGuiApi.h"

void* MallocAllocate(uptr size, b32 clear, uptr alignment, void* allocatorData)
{
    void* m = malloc(size);
    if (clear) memset(m, 0, size);
    return m;
}

void MallocDeallocate(void* ptr, void* allocatorData)
{
    free(ptr);
}

GAME_CODE_ENTRY void __cdecl GameUpdateAndRender(CoreState* core, GameInvoke command)
{
    switch (command)
    {
    case GameInvoke_Init:
    {
        //Allocator allocator =
        //{
        //    .allocatorData = nullptr,
        //    .Allocate = MallocAllocate,
        //    .Deallocate = MallocDeallocate
        //};

        //Reflection_Init(&allocator);

        IMGUI_CHECKVERSION(core->imgui);
        core->imgui->igSetCurrentContext(core->imguiContext);

        GameInit(core);
    } break;

    case GameInvoke_Update:
    {
        GameUpdate(core);
    } break;

    case GameInvoke_Render:
    {
        //bool foo = true;
        //core->imgui->igShowDemoWindow(&foo);

        GameRender(core);
    } break;

        InvalidDefault();
    }
}

#include "Game.c"

//#if !defined(META_PASS)
//#include "Reflection.Generated.c"
//#endif
