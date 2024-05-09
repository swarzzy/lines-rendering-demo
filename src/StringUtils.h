#pragma once

#include "Core/Common.h"
#include "Memory.h"

// UTF-32
u32 utf32StringLength(const char32* string);
bool utf32OneOf(char32 c, const char32* chars);
char32* utf32Extract(const char32* string, u32 count, MemoryStack* stack);

// UTF-8
bool utf8PrettySize(char* buffer, u32 bufferSize, uptr bytes);
char* utf8NullTreminate(const char* str, u32 count, MemoryStack* stack);
char* utf8CopyString(const char* str, MemoryStack* stack);

u32 asciiStringLength(const char* string);

// Conversion
char32* utf8toUtf32Str(const char* utf8str, MemoryStack* stack);
char* utf32toUtf8Str(const char32* utf32str, MemoryStack* stack);
char* utf32toUtf8StrCounted(const char32* utf32str, u32 count, MemoryStack* stack);
char32* utf32NullTreminate(const char32* str, u32 count, MemoryStack* stack);
