#pragma once

#include "core/Common.h"
#include "core/Memory.h"

typedef struct
{
    u32 begin;
    u32 end;
} CodepointRange;

typedef struct
{
    Vector2 uv0;
    Vector2 uv1;

    // Draw rect coords (SDF quad).
    Vector2 min;
    Vector2 max;

    // Bounding box.
    Vector2 boxMin;
    Vector2 boxMax;

    f32 advance;
    f32 leftBearing;
} FontGlyphInfo;

typedef struct
{
    f32 ascent;
    f32 descent;
    f32 lineGap;
    u32 bitmapDim;
    Vector2 bBoxMin;
    Vector2 bBoxMax;
    void* bitmap;
    u32 glyphCount;
    FontGlyphInfo* glyphs;
    f32 scaleFactor;
    f32 bakedHeight;
    f32 bakedHeightRcp;

    Vector2 sdfDrawParams;
    Vector2 sdfBakeParams;

    // Unicode codepoint -> glyphIndex;
    // TODO: Use hash table for full unicode support.
#define FONT_GLYPHS_TABLE_SIZE u16_Max
    u16* glyphsTable;
} Font;

Font LoadFont(MemoryStack* tempStack, void* fileBytes, f32 height, CodepointRange* ranges, u32 rangeCount, const char* fontName);
