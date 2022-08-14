
#include <stdio.h>
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
    return builder->buffer ? builder->buffer : calloc(1, sizeof(char));
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

void string_builder_append_char(string_builder_t *builder, char character){
    string_builder_append_view(builder, &character, 1);
}

void string_builder_append_int(string_builder_t *builder, int integer){
    char buffer[32];
    sprintf(buffer, "%d", integer);
    string_builder_append(builder, buffer);
}

void string_builder_append_quoted(string_builder_t *builder, const char *portion){
    string_builder_append_char(builder, '"');
    string_builder_append(builder, portion);
    string_builder_append_char(builder, '"');
}

void string_builder_append2_quoted(string_builder_t *builder, const char *part_a, const char *part_b){
    string_builder_append_char(builder, '"');
    string_builder_append(builder, part_a);
    string_builder_append(builder, part_b);
    string_builder_append_char(builder, '"');
}

void string_builder_append3_quoted(string_builder_t *builder, const char *part_a, const char *part_b, const char *part_c){
    string_builder_append_char(builder, '"');
    string_builder_append(builder, part_a);
    string_builder_append(builder, part_b);
    string_builder_append(builder, part_c);
    string_builder_append_char(builder, '"');
}
