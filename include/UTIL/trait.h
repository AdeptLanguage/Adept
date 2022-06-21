
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

#define TRAIT_NONE 0x0000u
#define TRAIT_1    0x0001u
#define TRAIT_2    0x0002u
#define TRAIT_3    0x0004u
#define TRAIT_4    0x0008u
#define TRAIT_5    0x0010u
#define TRAIT_6    0x0020u
#define TRAIT_7    0x0040u
#define TRAIT_8    0x0080u
#define TRAIT_9    0x0100u
#define TRAIT_A    0x0200u
#define TRAIT_B    0x0400u
#define TRAIT_C    0x0800u
#define TRAIT_D    0x1000u
#define TRAIT_E    0x2000u
#define TRAIT_F    0x4000u
#define TRAIT_G    0x8000u

#define TRAIT_2_MASK 0xFFFF0000u
#define TRAIT_2_1    0x00010000u
#define TRAIT_2_2    0x00020000u
#define TRAIT_2_3    0x00040000u
#define TRAIT_2_4    0x00080000u
#define TRAIT_2_5    0x00100000u
#define TRAIT_2_6    0x00200000u
#define TRAIT_2_7    0x00400000u
#define TRAIT_2_8    0x00800000u
#define TRAIT_2_9    0x01000000u
#define TRAIT_2_A    0x02000000u
#define TRAIT_2_B    0x04000000u
#define TRAIT_2_C    0x08000000u
#define TRAIT_2_D    0x10000000u
#define TRAIT_2_E    0x20000000u
#define TRAIT_2_F    0x40000000u
#define TRAIT_2_G    0x80000000u

#define TRAIT_ALL  0xFFFFFFFFu

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_TRAIT_H
