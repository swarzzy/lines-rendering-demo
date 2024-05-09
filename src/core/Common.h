#pragma once
#include <stdint.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>
#include <memory.h>
#include <uchar.h>

#define _ThreadLocal __declspec(thread)

#define __Concat(x,y) x##y
#define Concat(x,y) __Concat(x,y)

#if defined(_MSC_VER)

#define COMPILER_MSVC
#define BreakDebug() __debugbreak()

#elif defined(__clang__)

#define COMPILER_CLANG
#define BreakDebug() __builtin_debugtrap()

#else
#error Unsupported compiler
#endif

#define ArrayCount(arr) ((uint)(sizeof(arr) / sizeof(arr[0])))
#define TypeDecl(type, member) (((type*)0)->member)
#define OffsetOf(type, member) ((uptr)(&(((type*)0)->member)))
#define InvalidDefault() default: { BreakDebug(); } break
#define Unreachable() BreakDebug()

typedef uint8_t byte;
typedef unsigned char uchar;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uintptr_t uptr;

typedef u32 b32;
typedef byte b8;

typedef float f32;
typedef double f64;

typedef u32 u32x;
typedef i32 i32x;
typedef u32 uint;

typedef u32 char32;

const uptr Uptr_Max = UINTPTR_MAX;

const f32 f32_Pi = 3.14159265358979323846f;
const f32 f32_Eps = 0.000001f;
const f32 f32_Nan = NAN;
const f32 f32_Infinity = INFINITY;
const f32 f32_Max = FLT_MAX;
const f32 f32_Min = FLT_MIN;

const i32 i32_Max = INT32_MAX;
const i32 i32_Min = INT32_MIN;

const u32 u32_Max = 0xffffffff;

const u16 u16_Max = 0xffff;

const u64 u64_Max = UINT64_MAX;

#define Kilobytes(kb) ((kb) * (uptr)1024)
#define Megabytes(mb) ((mb) * (uptr)1024 * (uptr)1024)
