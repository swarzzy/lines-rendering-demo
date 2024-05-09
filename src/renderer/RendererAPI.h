#pragma once

#include "MetaprogramRuntime.h"

#include "../core/Common.h"
#include "../Math.h"

typedef enum
{
    VSyncMode_Disabled = 0,
    VSyncMode_Full,
    VSyncMode_Half
} VSyncMode;

typedef enum
{
    DisplayMode_Window = 0,
    DisplayMode_BorderlessWindow,
} DisplayMode;

typedef struct
{
    u32 width;
    u32 height;
} DisplayParams;

typedef enum
{
    RenderCommand_Clear,
    RenderCommand_DrawMeshImmediate,
    RenderCommand_SetMaterial
} RenderCommand;

typedef enum
{
    RenderClearFlags_Color = 0x1,
    RenderClearFlags_Depth = 0x2
} RenderClearFlags;

typedef struct
{
    Vector3 position;
    Vector2 uv;
    u32 vertexColor;
} RenderVertex;

typedef enum
{
    TextureFormat_R8,
    TextureFormat_sRGB_DXT1,
    TextureFormat_sRGBA_DXT5,
    TextureFormat_SRGB24_A8
} TextureFormat;

typedef enum
{
    TextureFiltering_Point,
    TextureFiltering_Bilinear
} TextureFiltering;

typedef struct
{
    TextureFiltering filtering;
} TextureSamplerSettings;

typedef struct
{
    u64 data0;
    u64 data1;
} TextureDescriptor;

typedef struct
{
    u64 data0;
} SamplerDescriptor;

typedef struct
{
    TextureDescriptor id;
    u32 width;
    u32 height;
    TextureFormat format;
} Texture2D;

typedef enum
{
    RenderMaterialType_Texture,
    RenderMaterialType_TextSDF,
} RenderMaterialType;

typedef struct
{
    RenderCommand command;
    union
    {
        struct
        {
            u32 vertexCount;
            u32 indexCount;
            RenderVertex* vertices;
            u32* indices;
            Matrix4x4* transform; // TODO: Store it in appropriate place.
        } drawMeshImmediate;

        struct
        {
            Vector4 color;
            float depth;
            RenderClearFlags flags;
        } clear;

        struct
        {
            RenderMaterialType type;
            Vector4 color;
            TextureDescriptor textureId;
            SamplerDescriptor sampler;
            Vector4 sdfParams;
        } setMaterial;
    };
} RenderCommandEntry;

typedef struct
{
    u32 renderCommandsCount;
    RenderCommandEntry* commands;
} RenderCommandBuffer;

typedef struct metaprogram_visible attribute(RendererAPI)
{
    DisplayParams(*SetDisplayParams)(DisplayParams displayParams);
    void(*SetVsyncMode)(VSyncMode mode);
    void(*SwapScreenBuffers)();
    void(*SetViewport)(Vector2 min, Vector2 dimensions);
    void(*ExecuteCommandBuffer)(RenderCommandBuffer* buffer);
    Texture2D(*LoadTexture2D)(u32 width, u32 height, TextureFormat format, void* data, uptr dataSize);
    SamplerDescriptor (*CreateSampler)(TextureSamplerSettings sampler);

    void(*UnloadTexture2D)(TextureDescriptor id);
    void(*BeginFrame)();
    void(*EndFrame)();
} RendererAPI;

#define OPENGL_RENDERER_INITIALIZE_PROC_NAME "InitializeRenderer_OpenGL"
typedef RendererAPI*(InitializeRendererOpenGLProc)(void* coreState, void* procLoaderProc);

#define D3D11_RENDERER_INITIALIZE_PROC_NAME "InitializeRenderer_D3D11"
typedef RendererAPI*(InitializeRendererD3D11Proc)(void* coreState, uptr hwnd, DisplayParams desiredDisplayParams, DisplayParams* actualDisplayParams, struct ID3D11Device** device, struct ID3D11DeviceContext** deviceContext);
