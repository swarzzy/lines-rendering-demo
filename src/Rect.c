#include "Rect.h"

void ResolveNode_Depth(InLayoutRectangleData* node, InLayoutRectangleData* parent, u32* index, OutLayoutRectangleData* buffer)
{
    u32 i = *index;

    f32 xParentMin = parent != NULL ? parent->resolvedMin.x : 0.0f;
    f32 yParentMin = parent != NULL ? parent->resolvedMin.y : 0.0f;
    f32 xParentMax = parent != NULL ? parent->resolvedMax.x : 0.0f;
    f32 yParentMax = parent != NULL ? parent->resolvedMax.y : 0.0f;

    f32 xLocalOffset = fLerp(node->position.x, node->positionWeight.x, fLerp(xParentMin, node->position.x, xParentMax));
    f32 yLocalOffset = fLerp(node->position.y, node->positionWeight.y, fLerp(yParentMin, node->position.y, yParentMax));

    f32 xMin = xParentMin + xLocalOffset - node->size.x * node->pivot.x;
    f32 yMin = yParentMin + yLocalOffset - node->size.y * node->pivot.y;

    f32 xMax = xParentMin + xLocalOffset + node->size.x - node->size.x * node->pivot.x;
    f32 yMax = yParentMin + yLocalOffset + node->size.y - node->size.y * node->pivot.y;

    node->resolvedMin = MakeVector2(xMin, yMin);
    node->resolvedMax = MakeVector2(xMax, yMax);

    buffer[i].rect.min = node->resolvedMin;
    buffer[i].rect.max = node->resolvedMax;

    buffer[i].debugName = node->debugName;
    buffer[i].color = node->color;

    buffer[i].calculatedDepth = i;
    (*index)++;

    if (node->firstChild != NULL)
    {
        ResolveNode_Depth(node->firstChild, node, index, buffer);
    }

    if (node->nextSibling != NULL)
    {
        ResolveNode_Depth(node->nextSibling, parent, index, buffer);
    }
}

void rltResolveRectHierarchy(InLayoutRectangleData* root, OutLayoutRectangleData* buffer, u32 bufferCount)
{
    u32 index = 0;
    ResolveNode_Depth(root, NULL, &index, buffer);
}
