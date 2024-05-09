#pragma once

#include "renderer/RendererAPI.h"
#include "Rect.h"

typedef struct
{
    u32 vertexCount;
    u32 indexCount;
    u32 vertexOffset;
    u32 indexOffset;
    RenderVertex* vertexBuffer;
    u32* indexBuffer;
} GeometryBuffer;


typedef struct
{
    Font* font;
    const char32* data;
    u32 dataCount;
    f32 height;
    u32 color;
} TextDrawBatch;

typedef struct
{
    f32 horzAlignment;
    f32 vertAlignment;
} TextDrawParams;

#define DefaultColor_White MakeVector4(1.0f, 1.0f, 1.0f, 1.0f)
#define DefaultColor32_White (0xffffffff)
#define DefaultColor32_Black (0xff000000)

u32 gfxPackColor(Vector4 color);
Vector4 gfxUnpackColor(u32 color);

Vector4 gfxColorToLinear(Vector4 color);
Vector4 gfxColorFromLinear(Vector4 color);

void gfxStartGeometryBatch(GeometryBuffer* buffer);
void gfxEmitPathGeometry(GeometryBuffer* buffer, Vector2* points, u32 pointsCount, f32 thickness, u32 color);
void gfxEmitQuadGeometry(GeometryBuffer* buffer, Vector2 min, Vector2 max, Vector2 uv0, Vector2 uv1, u32 vertexColor);

RenderCommandEntry* rcmdPushCommand(RenderCommandBuffer* commandBuffer);
void rcmdPushGeometryBatch(RenderCommandBuffer* commandBuffer, GeometryBuffer* buffer, Matrix4x4* transform);
void rcmdSetQuadMaterial(RenderCommandBuffer* commandBuffer, TextureDescriptor textureId, SamplerDescriptor sampler, Vector4 color);
void rcmdClear(RenderCommandBuffer* commandBuffer, RenderClearFlags flags, Vector4 color, f32 depth);

Rectangle2D gfxCalcTextBoundingBox(Rectangle2D rect, TextDrawBatch* batches, u32 count, TextDrawParams params, u32* outLinesCount);
void gfxEmitTextBoxGeometry(GeometryBuffer* buffer, Rectangle2D rect, TextDrawBatch* batches, u32 count, TextDrawParams params);
