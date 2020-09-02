
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
#define TRAIT_ALL  0xFFFF

#ifdef __cplusplus
}
#endif

#endif // _ISAAC_TRAIT_H
