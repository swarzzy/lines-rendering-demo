#pragma once

#define Debug_LogInfo(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define Debug_Assert(expr) assert(expr)
#define Debug_Validate(expr, message) assert(expr)
