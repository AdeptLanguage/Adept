
#include "UTIL/util.h"
#include "UTIL/string_builder.h"

void string_builder_init(string_builder_t *builder){
    builder->buffer = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

strong_cstr_t string_builder_finalize(string_builder_t *builder){
    return builder->buffer;
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
