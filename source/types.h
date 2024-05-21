#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define local_persist static
#define global_variable static
#define internal static

#define KILOBYTES(n) ((n)*1024)
#define MEGABYTES(n) ((KILOBYTES(n))*1024)
#define GIGABYTES(n) ((MEGABYTES(n))*1024)
#define TERABYTES(n) ((GIGABYTES(n))*1024)

#ifndef PERFORMANCE
#define assert(b) if (!(b)) {*(u8*)(0) = 0;}
#else
#define assert(b)
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef float f64;

typedef i32 bool32;

f32 Pi32 = 3.14159265359f;

#endif