#include "Game.h"
#include "Logging.h"
#include "Math.h"
#include "renderer/RendererAPI.h"
#include <locale.h>

#include "Rect.h"
#include "Font.h"
#include "Drawing.h"
#include "core/Keys.h"
#include "Assets.h"
#include "StringUtils.h"

char* LoremIpsum = "Давно выяснено, что при оценке дизайна и композиции читаемый текст мешает сосредоточиться. Lorem Ipsum используют потому, что тот обеспечивает более или менее стандартное заполнение шаблона, а также реальное распределение букв и пробелов в абзацах, которое не получается при простой дубликации Здесь ваш текст.. Здесь ваш текст.. Здесь ваш текст.. Многие программы электронной вёрстки и редакторы HTML используют Lorem Ipsum в качестве текста по умолчанию, так что поиск по ключевым словам lorem ipsum сразу показывает, как много веб-страниц всё ещё дожидаются своего настоящего рождения. За прошедшие годы текст Lorem Ipsum получил много версий. Некоторые версии появились по ошибке, некоторые - намеренно (например, юмористические варианты).";

typedef struct {
    u32 a;
} RandomSeries;

f32 RandomUnilateral(RandomSeries* state)
{
    // xorshift32
    u32 x = state->a;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state->a = x;
    return (f32)(state->a / (f64)u32_Max);
}

typedef struct
{
    GeometryBuffer geometryBuffer;

    Texture2D texture;
    RenderCommandBuffer commandBuffer;
    Matrix4x4 projectionTransform;

    MemoryStack tempStack;
    MemoryStack tempStack1;
    CoreState* core;

    Texture2D whiteTexture;
    Texture2D imageTexture;

    SamplerDescriptor linearSampler;

    void* fontFileData;
    const char* fontName;
    MemoryStack fontStacks[2];
    f32 fontSize;
    Texture2D fontAtlasTexture;
    Font font;

    char inputText[16384];
    f32 textScale;
} GameState;

static GameState _GameState;

GameState* GetGameState()
{
    return &_GameState;
}

void _WriteLog(CoreLogLevel logLevel, const char* tags, u32 tagsCount, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    GameState* gameState = GetGameState();
    gameState->core->coreAPI.WriteLog(logLevel, tags, tagsCount, format, args);
    va_end(args);
}

#define Log_Error(tag, format, ...) _WriteLog(CoreLogLevel_Error, tag, 1, format, ##__VA_ARGS__)
#define Log_Info(tag, format, ...) _WriteLog(CoreLogLevel_Info, tag, 1, format, ##__VA_ARGS__)
#define Log_Trace(tag, format, ...) _WriteLog(CoreLogLevel_Trace, tag, 1, format, ##__VA_ARGS__)
#define Log_Warn(tag, format, ...) _WriteLog(CoreLogLevel_Warning, tag, 1, format, ##__VA_ARGS__)

void* LoadFile(CoreAPI* core, const char* filename, MemoryStack* stack)
{
    FileHandle handle = core->OpenFile(filename, OpenFileMode_Read);
    if (handle == 0)
    {
        Log_Error("LoadFile", "Failed to open file %s", filename);
        return NULL;
    }

    i64 fileSize = core->GetFileSize(handle);
    char* buffer = mmStackPush(stack, fileSize);
    if (buffer == NULL)
    {
        Log_Error("LoadFile", "Failed to open file %s. File is bigger than read temp buffer.", filename);
        return NULL;
    }

    i64 bytesRead = core->ReadFile(handle, buffer, fileSize);

    core->CloseFile(handle);
    return buffer;
}

void ReloadFont(GameState* gameState)
{
    mmStackRewind(gameState->fontStacks + 1);
    mmStackSetMark(gameState->fontStacks + 0);

    gameState->core->rendererAPI->UnloadTexture2D(gameState->fontAtlasTexture.id);

    // TODO: cheack whar happens when accessing \0
    CodepointRange ranges[2];
    ranges[0].begin = (u32)' ';
    ranges[0].end = 127;
    ranges[1].begin = 1024;
    ranges[1].end = 1024 + 256;

    Font font = LoadFont(gameState->fontStacks + 0, gameState->fontFileData, gameState->fontSize, ranges, 2, gameState->fontName);
    Assert(font.bitmap);

    void* newGlyphs = mmStackPush(gameState->fontStacks + 1, sizeof(FontGlyphInfo) * font.glyphCount);
    mmCopy(newGlyphs, font.glyphs, sizeof(FontGlyphInfo) * font.glyphCount);
    font.glyphs = newGlyphs;

    void* newGlyphsTable = mmStackPushAligned(gameState->fontStacks + 1, sizeof(u16) * FONT_GLYPHS_TABLE_SIZE, 1);
    mmCopy(newGlyphsTable, font.glyphsTable, sizeof(u16) * FONT_GLYPHS_TABLE_SIZE);
    font.glyphsTable = newGlyphsTable;

    gameState->font = font;

    TextureSamplerSettings sampler;
    sampler.filtering = TextureFiltering_Bilinear;
    gameState->fontAtlasTexture = gameState->core->rendererAPI->LoadTexture2D(font.bitmapDim, font.bitmapDim, TextureFormat_R8, font.bitmap, 0);
    Assert(gameState->fontAtlasTexture.id.data0);

    mmStackRewind(gameState->fontStacks + 0);
}

Texture2D LoadTextureFromPng(const char* file, MemoryStack* stack, CoreState* core)
{
    mmStackSetMark(stack);
    ImageData img = resLoadImageFromFile(&core->coreAPI, file, 4, stack);
    Assert(img.data != NULL);

    u32 x = img.width / 4;
    u32 y = img.height / 4;

    u32 w = x * 4;
    u32 h = y * 4;
    ImageData resized = resResizeImage(img, w, h, stack);

    ImageData comp = resCompressImageDXT1(resized, stack);
    Assert(comp.data != NULL);

    TextureSamplerSettings sampler;
    sampler.filtering = TextureFiltering_Point;
    Texture2D texture = core->rendererAPI->LoadTexture2D(comp.width, comp.height, TextureFormat_sRGB_DXT1, comp.data, comp.dataSize);
    Assert(texture.id.data0);
    mmStackRewind(stack);
    return texture;
}

void GameInit(CoreState* core)
{
    GameState* gameState = GetGameState();
    gameState->core = core;

    gameState->fontSize = 40.0f;
    gameState->fontName = "Roboto-Medium.ttf";

    PagesAllocationResult fontPages = core->coreAPI.AllocatePages(Megabytes(16));
    gameState->fontStacks[0] = mmCreateStack(fontPages.memory, fontPages.actualSize, false, AllocationFailedStrategy_Crash, "Font Stack 0");
    gameState->fontStacks[1] = mmCreateStack(fontPages.memory, fontPages.actualSize, true, AllocationFailedStrategy_Crash, "Font Stack 1");

    PagesAllocationResult allocResult = core->coreAPI.AllocatePages(Megabytes(128));
    gameState->tempStack = mmCreateStack(allocResult.memory, allocResult.actualSize, false, AllocationFailedStrategy_Crash, "Temp Stack 0");
    gameState->tempStack1 = mmCreateStack(allocResult.memory, allocResult.actualSize, true, AllocationFailedStrategy_Crash, "Temp Stack 1");

    gameState->fontFileData = LoadFile(&gameState->core->coreAPI, "../../assets/fonts/Roboto-Medium.ttf", gameState->fontStacks + 0);
    Assert(gameState->fontFileData);

    u32 commandBufferCapacity = 102400;
    gameState->commandBuffer.commands = core->coreAPI.AllocatePages(Megabytes(1024)).memory;
    gameState->commandBuffer.renderCommandsCount = 0;

    gameState->geometryBuffer.vertexCount = 0;
    gameState->geometryBuffer.indexCount = 0;
    gameState->geometryBuffer.vertexOffset = 0;
    gameState->geometryBuffer.indexOffset = 0;
    gameState->geometryBuffer.vertexBuffer = core->coreAPI.AllocatePages(Megabytes(1024)).memory;
    gameState->geometryBuffer.indexBuffer = core->coreAPI.AllocatePages(Megabytes(1024)).memory;

    gameState->textScale = 0.7f;

    TextureSamplerSettings sampler = {0};
    sampler.filtering = TextureFiltering_Bilinear;
    gameState->linearSampler = core->rendererAPI->CreateSampler(sampler);
    Assert(gameState->linearSampler.data0);

    sampler.filtering = TextureFiltering_Point;
    byte whitePixel[] = { 255, 255, 255, 255 };
    gameState->whiteTexture = core->rendererAPI->LoadTexture2D(1, 1, TextureFormat_SRGB24_A8, whitePixel, 0);
    Assert(gameState->whiteTexture.id.data0);

    gameState->textScale = 25.0f;

    mmCopy(gameState->inputText, LoremIpsum, asciiStringLength(LoremIpsum) + 1);

    ReloadFont(gameState);

    gameState->imageTexture = LoadTextureFromPng("../../assets/sinji.png", &gameState->tempStack, core);
}

void GameReload(CoreState* core)
{
}

void GameUpdate(CoreState* core)
{
}

void EmitText(GameState* gameState, GeometryBuffer* buffer, RenderCommandBuffer* commandBuffer, Rectangle2D rect, char32* text, u32 textLength, f32 height, TextDrawParams params)
{
    TextDrawBatch textBatch;
    textBatch.height = height;
    textBatch.font = &gameState->font;
    textBatch.color = DefaultColor32_Black;
    textBatch.data = text;
    textBatch.dataCount = textLength;

    if (textLength != 0)
    {
        gfxStartGeometryBatch(buffer);

        RenderCommandEntry* sdfMaterialCommand = rcmdPushCommand(commandBuffer);
        sdfMaterialCommand->command = RenderCommand_SetMaterial;
        sdfMaterialCommand->setMaterial.type = RenderMaterialType_TextSDF;
        sdfMaterialCommand->setMaterial.textureId = gameState->fontAtlasTexture.id;
        sdfMaterialCommand->setMaterial.sampler = gameState->linearSampler;
        sdfMaterialCommand->setMaterial.sdfParams = MakeVector4(gameState->font.sdfDrawParams.x, gameState->font.sdfDrawParams.y, textBatch.height / gameState->font.bakedHeight, 0.0f);
        sdfMaterialCommand->setMaterial.color = MakeVector4(1.0f, 1.0f, 1.0f, 1.0f);

        gfxEmitTextBoxGeometry(buffer, rect, &textBatch, 1, params);
        rcmdPushGeometryBatch(commandBuffer, buffer, &gameState->projectionTransform);
    }
}

static void PathBezierCubicCurveToCasteljau(Vector2* path, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tess_tol, int level, int* index)
{
    float dx = x4 - x1;
    float dy = y4 - y1;
    float d2 = (x2 - x4) * dy - (y2 - y4) * dx;
    float d3 = (x3 - x4) * dy - (y3 - y4) * dx;
    d2 = (d2 >= 0) ? d2 : -d2;
    d3 = (d3 >= 0) ? d3 : -d3;
    if ((d2 + d3) * (d2 + d3) < tess_tol * (dx * dx + dy * dy))
    {
        path[*index] = MakeVector2(x4, y4);
        (*index)++;
    }
    else if (level < 10)
    {
        float x12 = (x1 + x2) * 0.5f, y12 = (y1 + y2) * 0.5f;
        float x23 = (x2 + x3) * 0.5f, y23 = (y2 + y3) * 0.5f;
        float x34 = (x3 + x4) * 0.5f, y34 = (y3 + y4) * 0.5f;
        float x123 = (x12 + x23) * 0.5f, y123 = (y12 + y23) * 0.5f;
        float x234 = (x23 + x34) * 0.5f, y234 = (y23 + y34) * 0.5f;
        float x1234 = (x123 + x234) * 0.5f, y1234 = (y123 + y234) * 0.5f;
        PathBezierCubicCurveToCasteljau(path, x1, y1, x12, y12, x123, y123, x1234, y1234, tess_tol, level + 1, index);
        PathBezierCubicCurveToCasteljau(path, x1234, y1234, x234, y234, x34, y34, x4, y4, tess_tol, level + 1, index);
    }
}

static u32 lineSegmentsCount;

void EmitCircle(GeometryBuffer* buffer, Vector2 position, f32 radius, u32 numSegments, u32 color, f32 thckness)
{
    u32 index = 0;
    Vector2 pathBuffer[4096];

    f32 pi2 = f32_Pi * 2.0f;

    for (u32 i = 0; i <= numSegments; i++)
    {
        f32 step = pi2 / numSegments * i;
        Vector2 p = MakeVector2(position.x + fCos(step) * radius, position.y + fSin(step) * radius);
        pathBuffer[index++] = p;
    }

    lineSegmentsCount += index - 1;
    gfxEmitPathGeometry(buffer, pathBuffer, index, thckness, color);
}


static const Vector2 Directions[] = { {-1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 1.0f} };

void EmitRandomPoly(GeometryBuffer* buffer, RandomSeries* series, u32 maxPoints, Rectangle2D rect, u32 color)
{
    u32 index = 0;
    Vector2 pathBuffer[1024];

    i32 pointsCount = iMax(8, (i32)(RandomUnilateral(series) * 30));

    Vector2 path = MakeVector2(rect.min.x + RandomUnilateral(series) * (rect.max.x - rect.min.x), rect.min.y + RandomUnilateral(series) * (rect.max.y - rect.min.y));

    for (i32 i = 0; i < pointsCount; i++)
    {
        i32 directionIndex = (i32)(RandomUnilateral(series) * 3);
        Vector2 direction = Directions[directionIndex];
        f32 length = RandomUnilateral(series) * 100.0f;

        path = v2Add(path, v2Scale(direction, length));
        path.x = fClamp(rect.min.x, path.x, rect.max.x);
        path.y = fClamp(rect.min.y, path.y, rect.max.y);

        pathBuffer[index++] = path;
    }

    lineSegmentsCount += index - 1;
    gfxEmitPathGeometry(buffer, pathBuffer, index, 1.0f, color);
}

void GameRender(CoreState* core)
{
    GameState* gameState = GetGameState();
    gameState->core = core;

    core->rendererAPI->BeginFrame();

    gameState->commandBuffer.renderCommandsCount = 0;

    gameState->geometryBuffer.vertexCount = 0;
    gameState->geometryBuffer.indexCount = 0;
    gameState->geometryBuffer.vertexOffset = 0;
    gameState->geometryBuffer.indexOffset = 0;

    gameState->projectionTransform = OrthoGLRH(0.0f, 1600.0f, 0.0f, 1200.0f, 0.0f, 1.0f);

    rcmdClear(&gameState->commandBuffer, RenderClearFlags_Color | RenderClearFlags_Depth, MakeVector4(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);

    Rectangle2D screenRect = {0};
    screenRect.min = MakeVector2(0.0f, 0.0f);
    screenRect.max = MakeVector2(1600.0f, 1200.0f);
    RandomSeries randomSeries = {12345};

    for (u32 i = 0; i < 500; i++)
    {
        gfxStartGeometryBatch(&gameState->geometryBuffer);
        rcmdSetQuadMaterial(&gameState->commandBuffer, gameState->whiteTexture.id, gameState->linearSampler, DefaultColor_White);

        u32 color = (u32)((f64)RandomUnilateral(&randomSeries) * u32_Max);

        for (u32 i = 0; i < 50; i++)
        {
            EmitRandomPoly(&gameState->geometryBuffer, &randomSeries, 100, screenRect, color);
        }

        rcmdPushGeometryBatch(&gameState->commandBuffer, &gameState->geometryBuffer, &gameState->projectionTransform);
    }

    for (u32 i = 0; i < 10; i++)
    {
        gfxStartGeometryBatch(&gameState->geometryBuffer);
        rcmdSetQuadMaterial(&gameState->commandBuffer, gameState->whiteTexture.id, gameState->linearSampler, DefaultColor_White);

        u32 color = (u32)((f64)RandomUnilateral(&randomSeries) * u32_Max);

        for (u32 i = 0; i < 100; i++)
        {
            Vector2 position = MakeVector2(RandomUnilateral(&randomSeries) * screenRect.max.x, RandomUnilateral(&randomSeries) * screenRect.max.y);
            f32 radius = RandomUnilateral(&randomSeries) * 200.0f;
            f32 thickness = RandomUnilateral(&randomSeries) * 5.0f + 1.0f;

            EmitCircle(&gameState->geometryBuffer, position, radius, 64, DefaultColor32_White, thickness);
        }

        rcmdPushGeometryBatch(&gameState->commandBuffer, &gameState->geometryBuffer, &gameState->projectionTransform);
    }

    mmStackSetMark(&gameState->tempStack);

    char32* line = utf8toUtf32Str(gameState->inputText, &gameState->tempStack);
    u32 lineLength = utf32StringLength(line);

    TextDrawParams textParams;
    textParams.horzAlignment = 0.0f;
    textParams.vertAlignment = 0.0f;

    EmitText(gameState, &gameState->geometryBuffer, &gameState->commandBuffer, screenRect, line, lineLength, gameState->textScale, textParams);

    Rectangle2D fpsRect = {0};
    fpsRect.min = MakeVector2(50.0f, 0.0f);
    fpsRect.max = MakeVector2(1600.0f, 60.0f);

    char buffer[1024];
    sprintf(buffer, "FPS: %d BATCHES: %d VERTICES: %d LINE SEGS: %d", (int)(1.0f / gameState->core->renderDeltaTime), gameState->commandBuffer.renderCommandsCount, gameState->geometryBuffer.vertexCount, lineSegmentsCount);
    lineSegmentsCount = 0;
    line = utf8toUtf32Str(buffer, &gameState->tempStack);
    lineLength = utf32StringLength(line);

    EmitText(gameState, &gameState->geometryBuffer, &gameState->commandBuffer, fpsRect, line, lineLength, 25.0f, textParams);
    mmStackRewind(&gameState->tempStack);

    Rectangle2D imgRect = {0};
    Vector2 imgPosition = MakeVector2(200.0f, 200.0f);
    imgRect.min = imgPosition;
    imgRect.max = v2Add(imgPosition, MakeVector2(400.0f, 400.0f));
    gfxStartGeometryBatch(&gameState->geometryBuffer);
    rcmdSetQuadMaterial(&gameState->commandBuffer, gameState->imageTexture.id, gameState->linearSampler, MakeVector4(0.1f, 0.1f, 0.1f, 1.0f));
    gfxEmitQuadGeometry(&gameState->geometryBuffer, imgRect.min, imgRect.max, MakeVector2(0.0f, 0.0f), MakeVector2(1.0f, 1.0f), DefaultColor32_White);
    rcmdPushGeometryBatch(&gameState->commandBuffer, &gameState->geometryBuffer, &gameState->projectionTransform);

    core->rendererAPI->ExecuteCommandBuffer(&gameState->commandBuffer);

    core->rendererAPI->EndFrame();

    ImVec2 pos = {0.0f, 0.0f};
    gameState->core->imgui->igInputTextMultiline("Text", gameState->inputText, ArrayCount((gameState->inputText)), pos, 0, 0, 0);
    gameState->core->imgui->igSliderFloat("TextScale", &gameState->textScale, 20.0f, 100.0f, "Text Scale", 0);
}

#include "core/Memory.c"
#include "Font.c"
#include "Drawing.c"
#include "Assets.c"
#include "StringUtils.c"
#include "Rect.c"