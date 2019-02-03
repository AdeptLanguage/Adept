
#ifndef __INSIGHT_OVERLOADS_H
#define __INSIGHT_OVERLOADS_H

#if !defined(ADEPT_INSIGHT_BUILD) && !defined(ADEPT_INSIGHT_BUILD_IGNORE_UNDEFINED)
#error "This file can only be used in the Adept Insight API"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

extern char insight_buffer[2560];
extern char insight_tmp_buffer[1280];
extern size_t insight_buffer_index;
extern size_t insight_tmp_buffer_length;

#define printf(...) { \
    snprintf(insight_tmp_buffer, 1280, __VA_ARGS__); \
    insight_tmp_buffer_length = strlen(insight_tmp_buffer) + 1; \
    if(insight_buffer_index + insight_tmp_buffer_length < 2560){ \
        memcpy(&insight_buffer[insight_buffer_index], insight_tmp_buffer, insight_tmp_buffer_length); \
        insight_buffer_index += insight_tmp_buffer_length - 1;\
    } \
}

#define puts(a) printf("%s\n", a)

#ifdef __cplusplus
}
#endif

#endif // __INSIGHT_OVERLOADS_H