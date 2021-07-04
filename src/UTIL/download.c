
#ifdef ADEPT_ENABLE_PACKAGE_MANAGER

#include <ctype.h>
#include <curl/curl.h>
#include "UTIL/util.h"
#include "UTIL/color.h"
#include "UTIL/download.h"
#include "UTIL/aes.h"

static size_t download_write_data_to_file(void *ptr, size_t size, size_t items, FILE *f);
static size_t download_write_data_to_memory(void *ptr, size_t size, size_t items, download_buffer_t *buffer);

successful_t download(weak_cstr_t url, weak_cstr_t destination, weak_cstr_t testcookie_solution){
    CURL *curl = curl_easy_init();
    if (curl) {
        FILE *f = fopen(destination, "wb");
        if(f == NULL) return false;

        struct curl_slist_t *list = NULL;

        // DANGEROUS: TODO: REMOVE THIS / FIND SOLUTION
        if(testcookie_solution){
            // DANGEROUS: TODO: REMOVE THIS / FIND SOLUTION
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

            list = curl_slist_append(list, testcookie_solution);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_write_data_to_file);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if(list) curl_slist_free_all(list);
        fclose(f);
        return res == CURLE_OK;
    }
    return false;
}

successful_t download_to_memory(weak_cstr_t url, download_buffer_t *out_memory, strong_cstr_t *testcookie_solution){
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

        struct curl_slist *list = NULL;
        bool using_ifh = string_starts_with(url, "https://adept.lovestoblog.com/");

        if(using_ifh){
            // DANGEROUS: When user has chosen to use 'lovestoblog' stash, the SSL cert isn't trusted
            // but proceed anyway

            // DANGEROUS: TODO: REMOVE THIS / FIND SOLUTION
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

            if(testcookie_solution && *testcookie_solution){
                // TODO: CLEANUP: Simplify

                // DANGEROUS: TODO: REMOVE THIS / FIND SOLUTION
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

                list = curl_slist_append(list, *testcookie_solution);
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
            }
        }

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if(list) curl_slist_free_all(list);

        successful_t successful = res == CURLE_OK;
        if(!successful) printf("%s\n", curl_easy_strerror(res));

        // Free allocated memory if failed
        if(!successful) free(out_memory->bytes);

        weak_cstr_t trigger = "<html><body><script type=\"text/javascript\" src=\"/aes.js\" ></script>";
        length_t trigger_length = strlen(trigger);

        if(successful && using_ifh && (testcookie_solution && *testcookie_solution == NULL) && out_memory->length >= trigger_length && strncmp(out_memory->bytes, trigger, trigger_length) == 0){
            struct AES_ctx context;

            // Example values
            //uint8_t problem[] = {0x91, 0x83, 0x14, 0xf1, 0x78, 0xb9, 0xb2, 0x65, 0x3d, 0x36, 0x1d, 0x6e, 0xd8, 0x5e, 0xf3, 0xfd};
            //uint8_t key[] = {0xf6, 0x55, 0xba, 0x9d, 0x9, 0xa1, 0x12, 0xd4, 0x96, 0x8c, 0x63, 0x57, 0x9d, 0xb5, 0x90, 0xb4};
            //uint8_t iv[] = {0x98, 0x34, 0x4c, 0x2e, 0xee, 0x86, 0xc3, 0x99, 0x48, 0x90, 0x59, 0x25, 0x85, 0xb4, 0x9f, 0x80};

            uint8_t problem[16], key[16], iv[16];
            if(!testcookie_extract_precursors(out_memory, problem, key, iv)){
                redprintf("testcookie_extract_precursors failed\n");
            }

            AES_init_ctx_iv(&context, key, iv);
            AES_CBC_decrypt_buffer(&context, problem, 16);
            uint8_t *solution = problem;

            char *hexcode = "0123456789abcdef";
            char hex_solution[48];
            for(length_t i = 0; i < 16; i++){
                hex_solution[i * 2] = hexcode[solution[i] / 16];
                hex_solution[i * 2 + 1]  = hexcode[solution[i] % 16];
            }
            hex_solution[33] = '\0';

            *testcookie_solution = mallocandsprintf("Cookie: __test=%s", hex_solution);
            free(out_memory->bytes);

            return download_to_memory(url, out_memory, testcookie_solution);
        }

        return successful;
    }
    return false;
}

successful_t testcookie_extract_precursors(download_buffer_t *memory, uint8_t problem[], uint8_t key[], uint8_t iv[]){
    // NOTE: Assumes output to 'problem', 'key', and 'iv' can hold 16 digits each

    int occurrence = 0;
    weak_cstr_t seed = "toNumbers(\"";
    length_t seed_length = strlen(seed);
    length_t hex_digits_length = 32;
    length_t min_length = seed_length + hex_digits_length + 1;

    if(memory->length <= min_length) return false;
    char *buffer = (char*) memory->bytes;

    for(length_t i = 0; i < memory->length - min_length - 1; i++){
        if(strncmp(&buffer[i], seed, seed_length) == 0){
            // HIT!

            i += seed_length;
            ++occurrence;

            uint8_t *output = occurrence == 1 ? key : occurrence == 2 ? iv : problem;

            for(length_t j = 0; j < hex_digits_length; j += 2){
                uint8_t high = isdigit(buffer[i + j]) ? buffer[i + j] - '0' : buffer[i + j] - 'a' + 10;
                uint8_t low = isdigit(buffer[i + j + 1]) ? buffer[i + j + 1] - '0' : buffer[i + j + 1] - 'a' + 10;
                output[j / 2] = 16 * high + low;
            }

            i += hex_digits_length;

            if(buffer[i] != '\"') return false;
            if(occurrence == 3) return true;
        }
    }

    return false;
}

static size_t download_write_data_to_file(void *ptr, size_t size, size_t items, FILE *f){
    return fwrite(ptr, size, items, f);
}

static size_t download_write_data_to_memory(void *ptr, size_t size, size_t items, download_buffer_t *buffer){
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
    memcpy(&(buffer->bytes[buffer->length]), ptr, total_append_size);
    buffer->length += total_append_size;

    // Zero terminate the buffer for good measure
    ((char*) buffer->bytes)[buffer->length] = 0x00;
    return total_append_size;
}

#endif
