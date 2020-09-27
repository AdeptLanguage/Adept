
#include "UTIL/util.h"
#include "UTIL/tmpbuf.h"

void tmpbuf_init(tmpbuf_t *tmpbuf){
    tmpbuf->buffer = NULL;
    tmpbuf->capacity = 0;
}

void tmpbuf_free(tmpbuf_t *tmpbuf){
    free(tmpbuf->buffer);
}

weak_cstr_t tmpbuf_quick_concat3(tmpbuf_t *tmp, const char *a, const char *b, const char *c){
    length_t a_len = strlen(a);
    length_t b_len = strlen(b);
    length_t c_len = strlen(c);
    length_t combined_length = a_len + b_len + c_len;

    expand((void**) &tmp->buffer, 1, 0, &tmp->capacity, combined_length + 1, combined_length + 1);

    char *str = tmp->buffer;
    memcpy(str, a, a_len);
    memcpy(&str[a_len], b, b_len);
    memcpy(&str[a_len + b_len], c, c_len + 1);
    return str;
}

