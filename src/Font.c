#include "Font.h"

#include "core/Memory.h"
#include "Logging.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>

#define STBTT_malloc(x,u)  mmStackPush(u, x)
#define STBTT_free(x,u)    ((void)(u),(void)(x))

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

u32 CalcGlyphTableLength(CodepointRange* ranges, u32 rangeCount)
{
    u32 totalCodepointCount = 0;
    for (u32 i = 0; i < rangeCount; i++)
    {
        Assert(ranges[i].end > ranges[i].begin);
        totalCodepointCount += ranges[i].end - ranges[i].begin + 1;
    }

    return totalCodepointCount + 1; // One for dummy char
}

typedef struct
{
    u32 codepoint;
    int width;
    int height;
    int xoff;
    int yoff;
    f32 advance;
    f32 leftBearing;
    u32 xBitmap;
    u32 yBitmap;

    // Bounding box
    int x0;
    int y0;
    int x1;
    int y1;

    char* bitmap;
} GlyphBitmapInfo;

Font LoadFont(MemoryStack* tempStack, void* fileBytes, f32 height, CodepointRange* ranges, u32 rangeCount, const char* _fontName)
{
    // [https://github.com/nothings/stb/blob/master/tests/sdf/sdf_test.c]
    const f32 OnEdgeValue = 128.0f;
    const f32 PixelDistScale = 64.0f;
    const f32 PixelDistScaleOffset = 30.0f;

    const char* fontName = _fontName ? _fontName : "";

    Font result = {0};

    stbtt_fontinfo font;
    font.userdata = tempStack;
    if (!stbtt_InitFont(&font, fileBytes, 0))
    {
        return result;
    }

    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

    f32 scale = stbtt_ScaleForPixelHeight(&font, height);

    u32 codepointsCount = CalcGlyphTableLength(ranges, rangeCount);
    u32 kernTableLength = (u32)stbtt_GetKerningTableLength(&font);
    Log_Info("FontLoader", "Font \"%s\" has %lu kerning table entries\n", fontName, kernTableLength);

    int fontBBoxMinX;
    int fontBBoxMinY;
    int fontBBoxMaxX;
    int fontBBoxMaxY;
    stbtt_GetFontBoundingBox(&font, &fontBBoxMinX, &fontBBoxMinY, &fontBBoxMaxX, &fontBBoxMaxY);

    GlyphBitmapInfo* bitmaps = mmStackPush(tempStack, sizeof(GlyphBitmapInfo) * codepointsCount + 1); // 1 for "missing" glyph

    // Render glyphs bitmaps

    GlyphBitmapInfo* missingGlyphInfo = bitmaps + 0;
    mmSet(missingGlyphInfo, 0, sizeof(GlyphBitmapInfo));
    missingGlyphInfo->codepoint = 9633; // 'WHITE SQUARE' (U+25A1)
    missingGlyphInfo->bitmap = stbtt_GetGlyphSDF(&font, scale, 0, 5, (unsigned char)OnEdgeValue, PixelDistScale, &(missingGlyphInfo->width), &(missingGlyphInfo->height), &(missingGlyphInfo->xoff), &(missingGlyphInfo->yoff));
    int missingGlyphAdvance;
    int missingGlyphLeftBearing;
    stbtt_GetGlyphHMetrics(&font, 0, &missingGlyphAdvance, &missingGlyphLeftBearing);
    missingGlyphInfo->advance = missingGlyphAdvance * scale;
    missingGlyphInfo->leftBearing = missingGlyphLeftBearing * scale;
    if (missingGlyphInfo->bitmap == NULL)
    {
        Log_Warn("FontLoader", "\"Missing Character\" glyph for font \"%s\" is missing\n", fontName);
    }

    u32 bitmapIndex = 1;
    for (u32 i = 0; i < rangeCount; i++)
    {
        CodepointRange* range = ranges + i;
        for (u32 codepoint = range->begin; codepoint <= range->end; codepoint++)
        {
            int glyphIndex = stbtt_FindGlyphIndex(&font, (int)codepoint);
            GlyphBitmapInfo* info = bitmaps + bitmapIndex;
            info->codepoint = codepoint;
            info->bitmap = stbtt_GetGlyphSDF(&font, scale, glyphIndex, 5, (unsigned char)OnEdgeValue, PixelDistScale, &(info->width), &(info->height), &(info->xoff), &(info->yoff));

            int advance, leftBearing;
            stbtt_GetGlyphHMetrics(&font, glyphIndex, &advance, &leftBearing);

            info->advance = advance * scale;
            info->leftBearing = leftBearing * scale;

            stbtt_GetGlyphBox(&font, glyphIndex, &(info->x0), &(info->y0), &(info->x1), &(info->y1));
            bitmapIndex++;
        }
    }

    u32 btimapsCount = bitmapIndex;

    // Pack rects on an atlas (and pick atlas size).

    stbrp_rect* rects = mmStackPush(tempStack, sizeof(stbrp_rect) * btimapsCount);
    u32 nodesCount = Megabytes(2) / sizeof(stbrp_node);
    stbrp_node* nodes = mmStackPush(tempStack, sizeof(stbrp_node) * nodesCount);

    for (u32 i = 0; i < btimapsCount; i++)
    {
        rects[i].w = bitmaps[i].width;
        rects[i].h = bitmaps[i].height;
    }

    u32 maxTextureSize = 8192;
    u32 usedTextureSize = 0;
    for (u32 dim = 128; dim < maxTextureSize; dim *= 2)
    {
        stbrp_context packContext;
        stbrp_init_target(&packContext, dim, dim, nodes, nodesCount);
        int packResult = stbrp_pack_rects(&packContext, rects, btimapsCount);

        if (packResult == 1)
        {
            usedTextureSize = dim;
            break;
        }

        for (u32 i = 0; i < btimapsCount; i++)
        {
            rects[i].x = 0;
            rects[i].y = 0;
            rects[i].was_packed = 0;
        }
    }

    if (usedTextureSize == 0)
    {
        Log_Error("FontLoader", "Unable to pack font \"%s\"! All glyphs do not fit to maximum allowed atlas (%lux%lu)\n", fontName, maxTextureSize, maxTextureSize);
        return result;
    }

    Log_Info("FontLoader", "Packed font \"%s\" to %lux%lu atlas\n", fontName, usedTextureSize, usedTextureSize);

    // Copy glyphs bitmaps to atlas

    char* bitmap = mmStackPush(tempStack, usedTextureSize * usedTextureSize);
    mmSet(bitmap, 0, usedTextureSize * usedTextureSize);

    for (u32 i = 0; i < btimapsCount; i++)
    {
        stbrp_rect* rect = rects + i;
        GlyphBitmapInfo* info = bitmaps + i;

        if (info->bitmap == NULL)
        {
            continue;
        }

        u32 numPixels = (u32)(rect->w * rect->h);
        for (u32 j = 0; j < numPixels; j++)
        {
            u32 y = j / rect->w;
            // It looks like glyph coords are top->bottom but our coords are bottom->top
            y = rect->h - y;
            u32 x = j % rect->w;
            u32 yBitmap = rect->y + y;
            u32 xBitmap = rect->x + x;

            info->xBitmap = xBitmap;
            info->yBitmap = yBitmap;
            bitmap[yBitmap * usedTextureSize + xBitmap] = info->bitmap[j];
        }
        stbtt_FreeSDF(info->bitmap, tempStack);
    }

    // Prepare font data

    FontGlyphInfo* glyphsInfo = mmStackPush(tempStack, sizeof(FontGlyphInfo) * btimapsCount);
    u16* glyphsTable = mmStackPush(tempStack, sizeof(u16) * FONT_GLYPHS_TABLE_SIZE);
    mmSet(glyphsTable, 0, sizeof(u16) * FONT_GLYPHS_TABLE_SIZE);
    f32 uvScale = 1.0f / usedTextureSize;

    for (u32 i = 0; i < btimapsCount; i++)
    {
        GlyphBitmapInfo* tempInfo = bitmaps + i;
        FontGlyphInfo* info = glyphsInfo + i;

        if (tempInfo->codepoint > u16_Max)
        {
            // TODO: Support full unicode.
            Log_Error("FontLoader", "Codepoint %lu in font \"%s\" exceeds u16_Max. It will be discarded\n", tempInfo->codepoint, fontName);
            continue;
        }

        // Not sure why exactly this convertion is neccesary.
        // It looks like Glyph Bitmap -> Rect Packer -> Our coords
        // conversions were messed up. I didn't got exactly what was wrong
        // but this way everything works ok ^_^
        info->uv0.x = ((i32)tempInfo->xBitmap - (i32)tempInfo->width) * uvScale;
        info->uv0.y = tempInfo->yBitmap * uvScale;
        info->uv1.x = tempInfo->xBitmap * uvScale;
        info->uv1.y = (tempInfo->yBitmap + tempInfo->height) * uvScale;
        info->min.x = (f32)tempInfo->xoff;
        info->min.y = -((f32)tempInfo->yoff + tempInfo->height);
        info->max.x = (f32)tempInfo->xoff + tempInfo->width;
        info->max.y = -(f32)tempInfo->yoff;
        info->boxMin = v2Scale(MakeVector2((f32)tempInfo->x0, (f32)tempInfo->y0), scale);
        info->boxMax = v2Scale(MakeVector2((f32)tempInfo->x1, (f32)tempInfo->y1), scale);
        info->advance = tempInfo->advance;
        info->leftBearing = tempInfo->leftBearing;

        glyphsTable[tempInfo->codepoint] = i;
    }

    result.ascent = ascent * scale;
    result.descent = descent * scale;
    result.lineGap = lineGap * scale;
    result.scaleFactor = scale;
    result.bBoxMin.x = fontBBoxMinX * scale;
    result.bBoxMin.y = fontBBoxMinY * scale;
    result.bBoxMax.x = fontBBoxMaxX * scale;
    result.bBoxMax.y = fontBBoxMaxY * scale;
    result.bitmap = bitmap;
    result.bitmapDim = usedTextureSize;
    result.glyphCount = btimapsCount;
    result.glyphs = glyphsInfo;
    result.glyphsTable = glyphsTable;

    result.sdfBakeParams.x = OnEdgeValue;
    result.sdfBakeParams.x = PixelDistScale;

    result.sdfDrawParams.x = OnEdgeValue;
    result.sdfDrawParams.y = PixelDistScale + PixelDistScaleOffset;
    result.bakedHeight = height;
    result.bakedHeightRcp = 1.0f / height;

    return result;
}
