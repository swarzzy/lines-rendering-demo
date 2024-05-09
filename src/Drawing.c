#include "Drawing.h"

#include "StringUtils.h"

typedef struct
{
    FontGlyphInfo* glyph;
    u32 color;
    f32 scale;
} LineCacheEntry;

typedef struct
{
    u32 charsCount;
    f32 width;
    f32 ascent;
    f32 descent;
    f32 lineGap;
    // indices to glyphs
    LineCacheEntry* lineCache;

    u32 lineCacheSize;
    f32 maxWidth;
    TextDrawBatch* batches;
    u32 batchesCount;
    u32 batchIndex;
    u32 charIndex;

} DrawTextState;

void gfxPrepareNextTextLineInternal(DrawTextState* state, bool writeLineCache);
void gfxSetupLineCacheInternal(DrawTextState* state);

u32 gfxPackColor(Vector4 color)
{
    byte r = (byte)(fClamp(0.0f, color.x, 1.0f) * 255.0f);
    byte g = (byte)(fClamp(0.0f, color.y, 1.0f) * 255.0f);
    byte b = (byte)(fClamp(0.0f, color.z, 1.0f) * 255.0f);
    byte a = (byte)(fClamp(0.0f, color.w, 1.0f) * 255.0f);

    u32 result = r | (g << 8) | (b << 16) | (a << 24);
    return result;
}

Vector4 gfxUnpackColor(u32 color)
{
    byte r = color & 0xff000000;
    byte g = color & 0x00ff0000;
    byte b = color & 0x0000ff00;
    byte a = color & 0x000000ff;

    const f32 d = 1.0f / 255.0f;
    Vector4 result = MakeVector4(r * d, g * d, b * d, a * d);
    return result;
}

// TODO: These are _EXTREMELY_ inefficient. Only for development.
f32 _gfxColorToLinear(f32 a)
{
    bool cutoff = a <  0.04045f;
    f32 higher = fPow((a + 0.055f) / 1.055f, 2.4f);
    f32 lower = a / 12.92f;
    return fLerp(higher, cutoff, lower);
}

f32 _gfxColorFromLinear(f32 a)
{
    bool cutoff = a < 0.0031308f;
    f32 higher = 1.055f * fPow(a, (f32)(1.0f / 2.4)) - 0.055f;
    f32 lower = a * 12.92f;
    return fLerp(higher, cutoff, lower);
}

Vector4 gfxColorToLinear(Vector4 color)
{
    // TODO: SIMDize
    Vector4 result;
    result.x = _gfxColorToLinear(color.x);
    result.y = _gfxColorToLinear(color.y);
    result.z = _gfxColorToLinear(color.z);
    result.w = color.w;

    return result;
}

Vector4 gfxColorFromLinear(Vector4 color)
{
    // TODO: SIMDize
    Vector4 result;
    result.x = _gfxColorFromLinear(color.x);
    result.y = _gfxColorFromLinear(color.y);
    result.z = _gfxColorFromLinear(color.z);
    result.w = color.w;

    return result;
}

void gfxStartGeometryBatch(GeometryBuffer* buffer)
{
    buffer->vertexOffset = buffer->vertexCount;
    buffer->indexOffset = buffer->indexCount;
}

void gfxEmitPathGeometry(GeometryBuffer* buffer, Vector2* points, u32 pointsCount, f32 thickness, u32 color)
{
    for (u32 i1 = 0; i1 < pointsCount - 1; i1++)
    {
        u32 i2 = (i1 + 1);
        Vector2 p1 = points[i1];
        Vector2 p2 = points[i2];

        Vector2 d = MakeVector2(p2.x - p1.x, p2.y - p1.y);
        d = v2Normalize(d);
        f32 dx = d.x * (thickness * 0.5f);
        f32 dy = d.y * (thickness * 0.5f);

        u32 vIndex = buffer->vertexCount;
        buffer->vertexBuffer[vIndex + 0].position = MakeVector3(p1.x + dy, p1.y - dx, 0.5f);
        buffer->vertexBuffer[vIndex + 0].uv = MakeVector2(0.0f, 0.0f);
        buffer->vertexBuffer[vIndex + 0].vertexColor = color;

        buffer->vertexBuffer[vIndex + 1].position = MakeVector3(p2.x + dy, p2.y - dx, 0.5f);
        buffer->vertexBuffer[vIndex + 1].uv = MakeVector2(0.0f, 0.0f);
        buffer->vertexBuffer[vIndex + 1].vertexColor = color;

        buffer->vertexBuffer[vIndex + 2].position = MakeVector3(p2.x - dy, p2.y + dx, 0.5f);
        buffer->vertexBuffer[vIndex + 2].uv = MakeVector2(0.0f, 0.0f);
        buffer->vertexBuffer[vIndex + 2].vertexColor = color;

        buffer->vertexBuffer[vIndex + 3].position = MakeVector3(p1.x - dy, p1.y + dx, 0.5f);
        buffer->vertexBuffer[vIndex + 3].uv = MakeVector2(0.0f, 0.0f);
        buffer->vertexBuffer[vIndex + 3].vertexColor = color;
        buffer->vertexCount += 4;

        vIndex -= buffer->vertexOffset;
        u32 iIndex = buffer->indexCount;
        buffer->indexBuffer[iIndex + 0] = vIndex + 0;
        buffer->indexBuffer[iIndex + 1] = vIndex + 1;
        buffer->indexBuffer[iIndex + 2] = vIndex + 2;
        buffer->indexBuffer[iIndex + 3] = vIndex + 0;
        buffer->indexBuffer[iIndex + 4] = vIndex + 2;
        buffer->indexBuffer[iIndex + 5] = vIndex + 3;
        buffer->indexCount += 6;
    }
}

void gfxEmitQuadGeometry(GeometryBuffer* buffer, Vector2 min, Vector2 max, Vector2 uv0, Vector2 uv1, u32 vertexColor)
{
    u32 vIndex = buffer->vertexCount;
    buffer->vertexBuffer[vIndex + 0].position = MakeVector3(min.x, min.y, 0.5f);
    buffer->vertexBuffer[vIndex + 0].uv = MakeVector2(uv0.x, uv0.y);
    buffer->vertexBuffer[vIndex + 0].vertexColor = vertexColor;
    buffer->vertexBuffer[vIndex + 1].position = MakeVector3(max.x, min.y, 0.5f);
    buffer->vertexBuffer[vIndex + 1].uv = MakeVector2(uv1.x, uv0.y);
    buffer->vertexBuffer[vIndex + 1].vertexColor = vertexColor;
    buffer->vertexBuffer[vIndex + 2].position = MakeVector3(max.x, max.y, 0.5f);
    buffer->vertexBuffer[vIndex + 2].uv = MakeVector2(uv1.x, uv1.y);
    buffer->vertexBuffer[vIndex + 2].vertexColor = vertexColor;
    buffer->vertexBuffer[vIndex + 3].position = MakeVector3(min.x, max.y, 0.5f);
    buffer->vertexBuffer[vIndex + 3].uv = MakeVector2(uv0.x, uv1.y);
    buffer->vertexBuffer[vIndex + 3].vertexColor = vertexColor;
    buffer->vertexCount += 4;

    vIndex -= buffer->vertexOffset;
    u32 iIndex = buffer->indexCount;
    buffer->indexBuffer[iIndex + 0] = vIndex + 0;
    buffer->indexBuffer[iIndex + 1] = vIndex + 1;
    buffer->indexBuffer[iIndex + 2] = vIndex + 2;
    buffer->indexBuffer[iIndex + 3] = vIndex + 2;
    buffer->indexBuffer[iIndex + 4] = vIndex + 3;
    buffer->indexBuffer[iIndex + 5] = vIndex + 0;
    buffer->indexCount += 6;
}

void gfxEmitTextBoxGeometry(GeometryBuffer* buffer, Rectangle2D rect, TextDrawBatch* batches, u32 count, TextDrawParams params)
{
    u32 fitLinesCount = 0;
    Rectangle2D boundingBox = gfxCalcTextBoundingBox(rect, batches, count, params, &fitLinesCount);
    Vector2 drawPosition = MakeVector2(rect.min.x, boundingBox.max.y);
    f32 maxWidth = rect.max.x - rect.min.x;

    DrawTextState state = {0};
    state.maxWidth = maxWidth;
    state.batches = batches;
    state.batchesCount = count;
    gfxSetupLineCacheInternal(&state);

    for (u32 i = 0; i < fitLinesCount; i++)
    {
        gfxPrepareNextTextLineInternal(&state, true);

        if (state.charsCount == 0)
        {
            // TODO invalid code path?
            break;
        }

        drawPosition.x += fAbs(maxWidth - state.width) * params.horzAlignment;
        drawPosition.y -= state.ascent;

        for (u32 i = 0; i < state.charsCount; i++)
        {
            LineCacheEntry e = state.lineCache[i];
            FontGlyphInfo* g = e.glyph;
            f32 scale = e.scale;
            u32 color = e.color;
            Vector2 min = v2Add(drawPosition, v2Scale(g->min, scale));
            Vector2 max = v2Add(drawPosition, v2Scale(g->max, scale));
            gfxEmitQuadGeometry(buffer, min, max, g->uv0, g->uv1, color);
            drawPosition.x += g->advance * scale;
        }

        drawPosition.x = rect.min.x;
        drawPosition.y += state.descent + state.lineGap;
    }
}

RenderCommandEntry* rcmdPushCommand(RenderCommandBuffer* commandBuffer)
{
    return commandBuffer->commands + commandBuffer->renderCommandsCount++;
}

void rcmdPushGeometryBatch(RenderCommandBuffer* commandBuffer, GeometryBuffer* buffer, Matrix4x4* transform)
{
    RenderCommandEntry* command = rcmdPushCommand(commandBuffer);
    command->command = RenderCommand_DrawMeshImmediate;

    command->drawMeshImmediate.vertexCount = buffer->vertexCount - buffer->vertexOffset;
    command->drawMeshImmediate.indexCount = buffer->indexCount - buffer->indexOffset;
    command->drawMeshImmediate.vertices = buffer->vertexBuffer + buffer->vertexOffset;
    command->drawMeshImmediate.indices = buffer->indexBuffer + buffer->indexOffset;
    // TODO: Store it
    command->drawMeshImmediate.transform = transform;
}

void rcmdSetQuadMaterial(RenderCommandBuffer* commandBuffer, TextureDescriptor textureId, SamplerDescriptor sampler, Vector4 color)
{
    RenderCommandEntry* command = rcmdPushCommand(commandBuffer);
    command->command = RenderCommand_SetMaterial;
    command->setMaterial.type = RenderMaterialType_Texture;
    command->setMaterial.textureId = textureId;
    command->setMaterial.sampler = sampler;
    command->setMaterial.color = color;
}

void rcmdClear(RenderCommandBuffer* commandBuffer, RenderClearFlags flags, Vector4 color, f32 depth)
{
    RenderCommandEntry* clearCommand = rcmdPushCommand(commandBuffer);
    clearCommand->command = RenderCommand_Clear;
    clearCommand->clear.color = color;
    clearCommand->clear.depth = depth;
    clearCommand->clear.flags = flags;
}

Rectangle2D gfxCalcTextBoundingBox(Rectangle2D rect, TextDrawBatch* batches, u32 count, TextDrawParams params, u32* outLinesCount)
{
    Assert(count > 0);
    Assert(batches != NULL);

    if (batches[0].dataCount == 0)
    {
        Rectangle2D result = {0};
    }

    // TODO: Handle case when there is no text
    Vector2 drawPosition = MakeVector2(rect.min.x, rect.max.y);
    f32 maxWidth = rect.max.x - rect.min.x;

    f32 xMin = rect.max.x;
    f32 xMax = rect.min.x;
    f32 yMin = rect.max.y;

    u32 linesCount = 0;

    DrawTextState state = {0};
    state.maxWidth = maxWidth;
    state.batches = batches;
    state.batchesCount = count;

    while (true)
    {
        gfxPrepareNextTextLineInternal(&state, false);

        if (state.charsCount == 0)
        {
            break;
        }

        if ((drawPosition.y - state.ascent + state.descent) < rect.min.y)
        {
            // Filled whole box.
            break;
        }

        drawPosition.y -= state.ascent;

        f32 x = rect.min.x + fAbs(maxWidth - state.width) * params.horzAlignment;
        xMin = fMin(xMin, x);
        xMax = fMax(xMax, x + state.width);
        yMin = drawPosition.y + state.descent;

        linesCount++;
        drawPosition.y += state.descent + state.lineGap;
    }

    Rectangle2D result =
    {
        .min = MakeVector2(xMin, yMin),
        .max = MakeVector2(xMax, rect.max.y)
    };

    // Apply vertical alignment.
    f32 textHeight = result.max.y - result.min.y;
    f32 rectHeight = rect.max.y - rect.min.y;
    f32 vertOffset = fMax(0.0f, rectHeight - textHeight) * params.vertAlignment;

    result.max.y -= vertOffset;
    result.min.y -= vertOffset;

    if (outLinesCount != NULL)
    {
        *outLinesCount = linesCount;
    }

    return result;
}

void gfxRewindTextIteratorInternal(DrawTextState* state, TextDrawBatch* batch, u32 index)
{
    state->batchIndex = (u32)(((uptr)batch - (uptr)state->batches) / sizeof(TextDrawBatch));
    Assert(state->batchIndex >=0 && state->batchIndex < state->batchesCount);
    state->charIndex = index;
}

u32 gfxGetNextCharacterAndBatchInternal(DrawTextState* state, TextDrawBatch** batch)
{
    if (state->batchIndex >= state->batchesCount)
    {
        *batch = NULL;
        return 0;
    }

    TextDrawBatch* currentBatch = state->batches + state->batchIndex;
    Assert(currentBatch->dataCount > 0);
    u32 index = state->charIndex;

    state->charIndex++;
    if (state->charIndex >= currentBatch->dataCount)
    {
        state->batchIndex++;
        state->charIndex = 0;
    }

    *batch = currentBatch;
    return index;
}

const char32 SkipCharacters[] = { '\r', 182, /*paragraph*/ 164, /*currency*/ 0};

void gfxPrepareNextTextLineInternal(DrawTextState* state, bool writeLineCache)
{
    i32 lastFitSpaceIndex = -1;
    i32 lastFitCharIndex = -1;
    f32 lastFitSpaceWidth = 0.0f;
    f32 lastFitCharWidth = 0.0f;
    u32 fitCharCount = 0;
    f32 fitWidth = 0.0f;
    TextDrawBatch* lastSpaceBatch = NULL;
    u32 lastSpaceBatchIndex = 0;
    TextDrawBatch* lastCharBatch = NULL;
    u32 lastCharBatchIndex = 0;

    f32 maxAscent = -f32_Infinity;
    f32 minDescent = f32_Infinity;
    f32 maxLineGap = 0.0f;

    while (true)
    {
        TextDrawBatch* batch = NULL;
        u32 index = gfxGetNextCharacterAndBatchInternal(state, &batch);

        if (batch == NULL)
        {
            lastFitSpaceIndex = fitCharCount;
            lastFitSpaceWidth = fitWidth;

            // Do not rewind.
            lastCharBatch = NULL;
            lastCharBatchIndex = 0;
            lastSpaceBatch = NULL;
            lastSpaceBatchIndex = 0;
            break;
        }

        char32 c = batch->data[index];

        if (utf32OneOf(c, SkipCharacters))
        {
            continue;
        }

        if (c == '\n')
        {
            lastFitSpaceIndex = fitCharCount;
            lastFitSpaceWidth = fitWidth;
            lastSpaceBatch = NULL;
            lastSpaceBatchIndex = 0;
            break;
        }

        Font* font = batch->font;
        f32 scale = batch->height * font->bakedHeightRcp;

        FontGlyphInfo* g = font->glyphs + font->glyphsTable[c];
        f32 scalableAdvance = g->boxMax.x - g->boxMin.x;
        f32 constantAdvance = g->advance * scale - scalableAdvance;
        f32 newWidth = fitWidth + scalableAdvance + constantAdvance;
        maxAscent = fMax(maxAscent, font->ascent * scale);
        maxAscent = fMax(maxAscent, g->boxMax.y * scale);
        minDescent = fMin(minDescent, g->boxMin.y * scale);
        minDescent = fMin(minDescent, font->descent * scale);
        maxLineGap = fMin(maxLineGap, font->lineGap * scale);

        if (newWidth > state->maxWidth)
        {
            break;
        }

        if (c <= ' ')
        {
            lastFitSpaceIndex = fitCharCount;
            lastFitSpaceWidth = fitWidth;
            lastSpaceBatch = batch;
            lastSpaceBatchIndex = index;
        }
        else
        {
            lastFitCharIndex = fitCharCount;
            lastFitCharWidth = fitWidth;
            lastCharBatch = batch;
            lastCharBatchIndex = index;
        }

        fitWidth = newWidth;

        if (writeLineCache)
        {
            Assert(fitCharCount < state->lineCacheSize);
            state->lineCache[fitCharCount].glyph = g;
            state->lineCache[fitCharCount].color = batch->color;
            state->lineCache[fitCharCount].scale = scale;
        }

        fitCharCount++;
    }

    state->width = lastFitSpaceWidth;
    state->ascent = maxAscent;
    state->descent = minDescent;
    state->lineGap = maxLineGap;

    TextDrawBatch* lastBatch = NULL;
    u32 lastBatchIndex = 0;
    bool brokeAtSpace = false;

    if (lastFitSpaceIndex > 0)
    {
        state->charsCount = lastFitSpaceIndex;
        lastBatch = lastSpaceBatch;
        lastBatchIndex = lastSpaceBatchIndex;
        brokeAtSpace = true;
    }
    else if (lastFitCharIndex > 0)
    {
        state->charsCount = lastFitCharIndex;
        lastBatch = lastCharBatch;
        lastBatchIndex = lastCharBatchIndex;
    }
    else
    {
        state->charsCount = 0;
    }

    if (lastBatch != NULL)
    {
        gfxRewindTextIteratorInternal(state, lastBatch, lastBatchIndex);
        if (brokeAtSpace)
        {
            // Eat trailing space.
            TextDrawBatch* unused;
            gfxGetNextCharacterAndBatchInternal(state, &unused);
        }
    }
}

// TODO: Allocate it via core allocators?
static LineCacheEntry _gfxLineCacheInternal[262144];

void gfxSetupLineCacheInternal(DrawTextState* state)
{
    state->lineCache = _gfxLineCacheInternal;
    state->lineCacheSize = ArrayCount(_gfxLineCacheInternal);
}
