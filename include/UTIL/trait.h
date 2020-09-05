
#ifndef _ISAAC_TRAIT_H
#define _ISAAC_TRAIT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    ================================= trait.h =================================
    Module for storing traits as bitwise flags
    ---------------------------------------------------------------------------
*/

// ---------------- trait_t ----------------
// A type for storing traits as bitwise flags
typedef unsigned int trait_t;

#define TRAIT_NONE 0x0000
#define TRAIT_1    0x0001
#define TRAIT_2    0x0002
#define TRAIT_3    0x0004
#define TRAIT_4    0x0008
#define TRAIT_5    0x0010
#define TRAIT_6    0x0020
#define TRAIT_7    0x0040
#define TRAIT_8    0x0080
#define TRAIT_9    0x0100
#define TRAIT_A    0x0200
#define TRAIT_B    0x0400
#define TRAIT_C    0x0800
#define TRAIT_D    0x1000
#define TRAIT_E    0x2000
#define TRAIT_F    0x4000
#define TRAIT_G    0x8000

#define TRAIT_2_MASK 0xFFFF0000
#define TRAIT_2_1    0x00010000
#define TRAIT_2_2    0x00020000
#define TRAIT_2_3    0x00040000
#define TRAIT_2_4    0x00080000
#define TRAIT_2_5    0x00100000
#define TRAIT_2_6    0x00200000
#define TRAIT_2_7    0x00400000
#define TRAIT_2_8    0x00800000
#define TRAIT_2_9    0x01000000
#define TRAIT_2_A    0x02000000
#define TRAIT_2_B    0x04000000
#define TRAIT_2_C    0x08000000
#define TRAIT_2_D    0x10000000
#define TRAIT_2_E    0x20000000
#define TRAIT_2_F    0x40000000
#define TRAIT_2_G    0x80000000

#define TRAIT_ALL  0xFFFFFFFF

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_TRAIT_H
