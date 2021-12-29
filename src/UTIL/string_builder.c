
#include <stdlib.h>
#include <string.h>

#include "UTIL/ground.h"
#include "UTIL/string_builder.h"
#include "UTIL/util.h"

void string_builder_init(string_builder_t *builder){
    builder->buffer = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

strong_cstr_t string_builder_finalize(string_builder_t *builder){
    return builder->buffer;
}

strong_lenstr_t string_builder_finalize_with_length(string_builder_t *builder){
    return (strong_lenstr_t){
        .cstr = builder->buffer,
        .length = builder->length,
    };
}

void string_builder_abandon(string_builder_t *builder){
    free(builder->buffer);
}

void string_builder_append(string_builder_t *builder, const char *portion){
    string_builder_append_view(builder, portion, strlen(portion));
}

void string_builder_append_view(string_builder_t *builder, const char *portion, length_t portion_length){
    expand((void**) &builder->buffer, sizeof(char), builder->length, &builder->capacity, portion_length + 1, 32);

    memcpy(&builder->buffer[builder->length], portion, portion_length);
    builder->length += portion_length;
    builder->buffer[builder->length] = 0x00;
}
