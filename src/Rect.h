#pragma once

#include "Math.h"

typedef struct
{
    Vector2 min;
    Vector2 max;
} Rectangle2D;

typedef struct _InLayoutRectangleData
{
    Vector2 size;
    Vector2 position;
    Vector2 pivot;
    Vector2 positionWeight; // 0 - absolutePosition, 1 - normalized

    // TODO: Remove outta here!
    Vector4 color;


    struct _InLayoutRectangleData* firstChild;
    struct _InLayoutRectangleData* nextSibling;

    Vector2 resolvedMin;
    Vector2 resolvedMax;

    u32 calculatedDepth;

    const char* debugName;
} InLayoutRectangleData;

typedef struct _OutLayoutRectangleData
{
    Rectangle2D rect;
    // TODO: Remove outta here!
    Vector4 color;

    u32 calculatedDepth;
    const char* debugName;
} OutLayoutRectangleData;

void rltResolveRectHierarchy(InLayoutRectangleData* root, OutLayoutRectangleData* buffer, u32 bufferCount);
