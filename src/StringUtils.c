#include "StringUtils.h"

#include <utf8/utf8.h>

u32 utf32StringLength(const char32* string)
{
    u32 count = 0;
    while (string[count] != 0) count++;
    return count;
}

u32 asciiStringLength(const char* string)
{
    u32 count = 0;
    while (string[count] != 0) count++;
    return count;
}

char32* utf32Extract(const char32* string, u32 count, MemoryStack* stack)
{
    char32* newStr = mmStackPush(stack, sizeof(char32) * (count + 1));
    mmCopy(newStr, string, count);
    newStr[count] = 0;
    return newStr;
}

bool utf32OneOf(char32 c, const char32* chars)
{
    while (*chars != 0)
    {
        if (*chars == c)
        {
            return true;
        }

        chars++;
    }

    return false;
}

char32* utf8toUtf32Str(const char* utf8str, MemoryStack* stack)
{
    uptr numCodepoints = utf8len(utf8str) + 1;
    char32* utf32str = mmStackPush(stack, sizeof(char32) * numCodepoints);
    char32* utf32ptr = utf32str;
    while (true)
    {
        utf8str = utf8codepoint(utf8str, utf32ptr);
        if (*utf32ptr == 0)
        {
            break;
        }

        utf32ptr++;
    }

    return utf32str;
}

char* utf32toUtf8Str(const char32* utf32str, MemoryStack* stack)
{
    uptr bufferSize = 1;
    const char32* at = utf32str;
    while (*at != 0)
    {
        bufferSize += utf8codepointsize(*at);
        at++;
    }

    char* utf8Str = mmStackPush(stack, bufferSize);
    char* ptr = utf8Str;

    at = utf32str;
    while (true)
    {
        ptr = utf8catcodepoint(ptr, *at, (uptr)bufferSize - ((uptr)ptr - (uptr)utf8Str));
        Assert(ptr != NULL);

        if (*at == 0)
        {
            break;
        }

        at++;
    }

    return utf8Str;
}

#if false // bugy
char* utf32toUtf8StrCounted(const char32* utf32str, u32 count, MemoryStack* stack)
{
    uptr bufferSize = 0;
    for (u32 i = 0; i < count; i++)
    {
        bufferSize += utf8codepointsize(utf32str[i]);
    }

    char* utf8Str = mmStackPush(stack, bufferSize);
    char* ptr = utf8Str;

    for (u32 i = 0; i < count; i++)
    {
        ptr = utf8catcodepoint(ptr, utf32str[i], (uptr)bufferSize - ((uptr)ptr - (uptr)utf8Str));
        Assert(ptr != NULL);
    }

    return utf8Str;
}
#endif

bool utf8PrettySize(char* buffer, u32 bufferSize, uptr bytes)
{
    bool result = false;
    f64 size = (f64)bytes;
    static const char* suffixes[4] = {"bytes", "kb", "mb", "gb"};
    i32 s = 0;
    while (size >= 1024 && s < 4)
    {
        s++;
        size /= 1024;
    }
    if (size - dFloor(size) == 0.0)
    {
        u32 requiredSize = (u32)snprintf(0, 0, "%ld%s", (long)size, suffixes[s]) + 1;
        if (requiredSize <= bufferSize)
        {
            sprintf(buffer, "%ld%s", (long)size, suffixes[s]);
            result = true;
        }
    }
    else
    {
        u32 requiredSize = (u32)snprintf(0, 0, "%.1f%s", size, suffixes[s]) + 1;
        if (requiredSize <= bufferSize)
        {
            sprintf(buffer, "%.1f%s", size, suffixes[s]);
            result = true;
        }
    }
    return result;
}

char* utf8NullTreminate(const char* str, u32 count, MemoryStack* stack)
{
    char* result = mmStackPush(stack, count + 1);
    result[count] = 0;
    mmCopy(result, str, count);
    return result;
}

char32* utf32NullTreminate(const char32* str, u32 count, MemoryStack* stack)
{
    char32* result = mmStackPush(stack, (count + 1) * sizeof(char32));
    result[count] = 0;
    mmCopy(result, str, count * sizeof(char32));
    return result;
}

char* utf8CopyString(const char* str, MemoryStack* stack)
{
    size_t size = strlen(str) + 1;
    char* mem = mmStackPush(stack, size);
    mmCopy(mem, str, size);
    return mem;
}
