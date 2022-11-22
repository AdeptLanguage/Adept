
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

static const char *user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.77 Safari/537.36";

static size_t download_write_data_to_file(void *ptr, size_t size, size_t items, FILE *f);
static size_t download_write_data_to_memory(void *ptr, size_t size, size_t items, void *buffer_ptr);

successful_t download(weak_cstr_t url, weak_cstr_t destination, weak_cstr_t cainfo_file){
    CURL *curl = curl_easy_init();

    if (curl) {
        FILE *f = fopen(destination, "wb");

        if(f == NULL){
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_write_data_to_file);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);

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

        *out_memory = (download_buffer_t){
            .bytes = calloc(1, sizeof(char)),
            .length = 0,
        };

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_write_data_to_memory);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_memory);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);

        if(cainfo_file){
            curl_easy_setopt(curl, CURLOPT_CAINFO, cainfo_file);
        }

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        successful_t successful = res == CURLE_OK;

        if(!successful){
            printf("%s\n", curl_easy_strerror(res));
            free(out_memory->bytes);
        }

        return successful;
    }

    return false;
}

static size_t download_write_data_to_file(void *ptr, size_t size, size_t items, FILE *f){
    return fwrite(ptr, size, items, f);
}

static size_t download_write_data_to_memory(void *ptr, size_t size, size_t items, void *buffer_ptr){
    download_buffer_t *buffer = (download_buffer_t*) buffer_ptr;
    length_t num_bytes = size * items;

    char *reallocated = realloc(buffer->bytes, buffer->length + num_bytes + 1);

    if(reallocated == NULL){
        redprintf("external-error: ");
        printf("download_write_data_to_memory() ran out of memory!\n");
        free(buffer->bytes);
        return 0;
    }

    // Append received data
    memcpy(&reallocated[buffer->length], ptr, num_bytes);
    reallocated[buffer->length + num_bytes] = '\0';

    buffer->bytes = reallocated;
    buffer->length += num_bytes;

    return num_bytes;
}

#endif
