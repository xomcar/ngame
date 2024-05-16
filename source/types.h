#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>
#define local_persist static
#define global_variable static
#define internal static

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

f32 Pi32 = 3.14159265359;

#endif