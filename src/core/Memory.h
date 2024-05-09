#pragma once

typedef enum
{
    AllocationFailedStrategy_ReturnNull,
    AllocationFailedStrategy_Crash
} AllocationFailStrategy;

typedef struct _MemoryStackMark
{
    struct _MemoryStackMark* prev;
} MemoryStackMark;

typedef struct
{
    bool reversed;
    AllocationFailStrategy failStrategy;
    void* memory;
    uptr memorySize;
    uptr top;
    uptr free;
    MemoryStackMark* lastMark;
    const char* debugTag;
} MemoryStack;

bool mmIsPowerOfTwo(uptr n);
uptr mmAlignAdressUp(uptr address, uptr alignment);
uptr mmAlignAdressDown(uptr address, uptr alignment);

MemoryStack mmCreateStack(void* memory, uptr memorySize, bool reveresed, AllocationFailStrategy failStrategy, const char* debugTag);
void* mmStackPushAligned(MemoryStack* stack, uptr size, uptr alignment);
void* mmStackPush(MemoryStack* stack, uptr size);
MemoryStackMark* mmStackSetMark(MemoryStack* stack);
bool mmStackRewindTo(MemoryStack* stack, MemoryStackMark* mark);
bool mmStackRewind(MemoryStack* stack);
void mmStackEnsureAt(MemoryStack* stack, MemoryStackMark* mark);

#define mmCopy(dest, src, size) memcpy(dest, src, size)
#define mmSet(dest, value, size) memset(dest, value, size)
