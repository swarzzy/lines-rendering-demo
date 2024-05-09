#include "Memory.h"

bool mmIsPowerOfTwo(uptr n)
{
    return (((n) & ((n) - 1)) == 0);
}

uptr mmAlignAdressUp(uptr address, uptr alignment)
{
    if (alignment == 1)
    {
        return address;
    }

    Assert(mmIsPowerOfTwo(alignment));
    uptr result = (address + (alignment - 1)) & ~(alignment - 1);
    return result;
}

uptr mmAlignAdressDown(uptr address, uptr alignment)
{
    if (alignment == 1)
    {
        return address;
    }

    Assert(mmIsPowerOfTwo(alignment));

#pragma warning(push)
#pragma warning(disable : 4146)

    uptr result = address &= -alignment;

#pragma warning(pop)

    return result;
}

MemoryStack mmCreateStack(void* memory, uptr memorySize, bool reveresed, AllocationFailStrategy failStrategy, const char* debugTag)
{
    MemoryStack stack = {0};

    if (reveresed)
    {
        stack.memory = (void*)((uptr)memory + memorySize - 1);
    }
    else
    {
        stack.memory = memory;
    }

    stack.memorySize = memorySize;
    stack.free = memorySize;
    stack.reversed = reveresed;
    stack.failStrategy = failStrategy;
    stack.debugTag = debugTag;
    return stack;
}

void* mmHandleAllocationFailed(MemoryStack* stack, uptr size)
{
    if (stack->failStrategy == AllocationFailedStrategy_Crash)
    {
        Assert(false);
    }

    return NULL;
}

void* mmStackPushAlignedFwd(MemoryStack* stack, uptr size, uptr alignment)
{
    Assert(alignment > 0);

    uptr top = (uptr)stack->memory + stack->top;
    uptr aligned = mmAlignAdressUp(top, alignment);
    uptr newTop = aligned + size;
    uptr pushSize = newTop - top;

    if (pushSize > stack->free)
    {
        return mmHandleAllocationFailed(stack, size);
    }

    stack->free -= pushSize;
    stack->top = newTop - (uptr)stack->memory;

    Assert(stack->free >= 0);

    // printf("Allocated %llu bytes at %llu free %llu\n", size, aligned, stack->free);

    return (void*)aligned;
}

void* mmStackPushAlignedRev(MemoryStack* stack, uptr size, uptr alignment)
{
    Assert(alignment > 0);

    uptr top = (uptr)stack->memory - stack->top;
    uptr aligned = mmAlignAdressDown(top, alignment);
    uptr newTop = aligned - size;
    uptr pushSize = top - newTop;

    if (pushSize > stack->free)
    {
        return mmHandleAllocationFailed(stack, size);
    }

    stack->free -= pushSize;
    stack->top = (uptr)stack->memory - newTop;

    Assert(stack->free >= 0);

    return (void*)newTop;
}

void* mmStackPushAligned(MemoryStack* stack, uptr size, uptr alignment)
{
    if (!stack->reversed)
    {
        return mmStackPushAlignedFwd(stack, size, alignment);
    }
    else
    {
        return mmStackPushAlignedRev(stack, size, alignment);
    }
}

void* mmStackPush(MemoryStack* stack, uptr size)
{
    return mmStackPushAligned(stack, size, 16);
}

MemoryStackMark* mmStackSetMark(MemoryStack* stack)
{
    //Log_Trace(stack->debugTag, "Set stack mark at %llu\n", stack->top);
    MemoryStackMark* mark = mmStackPushAligned(stack, sizeof(MemoryStackMark), 1);
    if (mark == NULL)
    {
        return NULL;
    }

    mark->prev = stack->lastMark;
    stack->lastMark = mark;


    return mark;
}

bool mmStackRewindToFwd(MemoryStack* stack, MemoryStackMark* mark)
{
    if (mark == NULL)
    {
        if (stack->top != 0)
        {
            mark = (MemoryStackMark*)stack->memory;
        }
        else
        {
            return false;
        }
    }

    Assert((uptr)mark >= (uptr)stack->memory && (uptr)mark < ((uptr)stack->memory + stack->memorySize));

    uptr newTop = (uptr)mark - (uptr)stack->memory;

    stack->free = stack->memorySize - newTop;
    stack->top = newTop;
    stack->lastMark = (mark == stack->memory ? NULL : mark->prev);

    //Log_Trace(stack->debugTag, "Rewind stack to %llu\n", stack->top);

    return true;
}

bool mmStackRewindToRev(MemoryStack* stack, MemoryStackMark* mark)
{
    if (mark == NULL)
    {
        if (stack->top != 0)
        {
            mark = (MemoryStackMark*)stack->memory;
        }
        else
        {
            return false;
        }
    }

    Assert((uptr)mark <= (uptr)stack->memory && (uptr)mark > ((uptr)stack->memory - stack->memorySize));

    uptr newTop = (uptr)stack->memory - (uptr)mark;

    stack->free = stack->memorySize - newTop;
    stack->top = newTop;
    stack->lastMark = (mark == stack->memory ? NULL : mark->prev);

    //Log_Trace(stack->debugTag, "Rewind stack to %llu\n", stack->top);

    return true;
}

bool mmStackRewindTo(MemoryStack* stack, MemoryStackMark* mark)
{
    if (!stack->reversed)
    {
        return mmStackRewindToFwd(stack, mark);
    }
    else
    {
        return mmStackRewindToRev(stack, mark);
    }
}

bool mmStackRewind(MemoryStack* stack)
{
    return mmStackRewindTo(stack, stack->lastMark);
}

void mmStackEnsureAt(MemoryStack* stack, MemoryStackMark* mark)
{
    if (!stack->reversed)
    {
        Assert(((uptr)stack->memory + stack->top) == (mark == NULL ? (uptr)stack->memory : (uptr)mark));
    }
    else
    {
        Assert(((uptr)stack->memory - stack->top) == (mark == NULL ? (uptr)stack->memory : (uptr)mark));
    }
}
