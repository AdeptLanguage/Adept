
#ifdef ADEPT_ENABLE_PACKAGE_MANAGER

#include <curl/curl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "UTIL/color.h"
#include "NET/download.h"
#include "UTIL/ground.h"
#include "UTIL/util.h" // IWYU pragma: keep

static size_t download_write_data_to_file(void *ptr, size_t size, size_t items, FILE *f);
static size_t download_write_data_to_memory(void *ptr, size_t size, size_t items, void *buffer_voidptr);

successful_t download(weak_cstr_t url, weak_cstr_t destination, weak_cstr_t cainfo_file){
    CURL *curl = curl_easy_init();
    if (curl) {
        FILE *f = fopen(destination, "wb");
        if(f == NULL) return false;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_write_data_to_file);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);

        if(cainfo_file){
            curl_easy_setopt(curl, CURLOPT_CAINFO, cainfo_file);
        }

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        fclose(f);

        return res == CURLE_OK;
    }
    return false;
}

successful_t download_to_memory(weak_cstr_t url, download_buffer_t *out_memory, weak_cstr_t cainfo_file){
    CURL *curl = curl_easy_init();
    if (curl) {
        // Setup output buffer

        out_memory->bytes = malloc(1);
        ((char*) out_memory->bytes)[0] = 0x00;
        out_memory->length = 0;

        #ifdef TRACK_MEMORY_USAGE
        out_memory->capacity = 1;
        #endif

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_write_data_to_memory);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_memory);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.77 Safari/537.36");

        if(cainfo_file){
            curl_easy_setopt(curl, CURLOPT_CAINFO, cainfo_file);
        }

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        successful_t successful = res == CURLE_OK;
        if(!successful) printf("%s\n", curl_easy_strerror(res));

        // Free allocated memory if failed
        if(!successful) free(out_memory->bytes);
        return successful;
    }
    return false;
}

static size_t download_write_data_to_file(void *ptr, size_t size, size_t items, FILE *f){
    return fwrite(ptr, size, items, f);
}

static size_t download_write_data_to_memory(void *ptr, size_t size, size_t items, void *buffer_voidptr){
    download_buffer_t *buffer = (download_buffer_t*) buffer_voidptr;
    length_t total_append_size = size * items;

    #ifdef TRACK_MEMORY_USAGE
    // Avoid using realloc when tracking memory usage
    expand(&ptr, 1, buffer->length, &buffer->capacity, total_append_size + 1, 1024);
    #else
    char *reallocated = realloc(buffer->bytes, buffer->length + total_append_size + 1);

    if(reallocated == NULL){
        redprintf("external-error: ");
        printf("download_write_data_to_memory() ran out of memory!\n");
        free(buffer->bytes);
        return 0;
    }

    buffer->bytes = reallocated;
    #endif

    // Append received data
    memcpy(&(((char*) buffer->bytes)[buffer->length]), ptr, total_append_size);
    buffer->length += total_append_size;

    // Zero terminate the buffer for good measure
    ((char*) buffer->bytes)[buffer->length] = 0x00;
    return total_append_size;
}

#endif
