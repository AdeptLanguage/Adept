
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "ground.h"
#include "filename.h"

char* filename_name(const char *filename){
    length_t i;
    char *stub;
    length_t filename_length = strlen(filename);

    for(i = filename_length - 1 ; i != 0; i--){
        if(filename[i] == '/' || filename[i] == '\\'){
            length_t stub_size = filename_length - i; // Includes null character
            stub = malloc(stub_size);
            memcpy(stub, &filename[i+1], stub_size);
            return stub;
        }
    }

    stub = malloc(filename_length + 1);
    memcpy(stub, filename, filename_length + 1);
    return stub;
}

const char* filename_name_const(const char *filename){
    length_t i;
    length_t filename_length = strlen(filename);

    for(i = filename_length - 1 ; i != 0; i--){
        if(filename[i] == '/' || filename[i] == '\\'){
            return &filename[i+1];
        }
    }

    return filename;
}

char* filename_local(const char *current_filename, const char *local_filename){
    // NOTE: Returns string that must be freed by caller
    // NOTE: Appends 'local_filename' to the path of 'current_filename' and returns result

    length_t current_filename_cutoff;
    bool found = false;
    char *result;

    for(current_filename_cutoff = strlen(current_filename) - 1; current_filename_cutoff != 0; current_filename_cutoff--){
        if(current_filename[current_filename_cutoff] == '/' || current_filename[current_filename_cutoff] == '\\'){
            found = true;
            break;
        }
    }

    if(found){
        result = malloc(current_filename_cutoff + strlen(local_filename) + 2);
        memcpy(result, current_filename, current_filename_cutoff);
        result[current_filename_cutoff] = '/';
        memcpy(&result[current_filename_cutoff + 1], local_filename, strlen(local_filename) + 1);
        return result;
    } else {
        result = malloc(strlen(local_filename) + 1);
        strcpy(result, local_filename);
        return result;
    }

    return NULL; // Should never be reached
}

char* filename_adept_import(const char *filename){
    // NOTE: Returns string that must be freed by caller

    #ifdef _WIN32

    char *user = filename_user();
    length_t user_len = strlen(user);
    length_t filename_len = strlen(filename);
    char *result = malloc(filename_len + user_len + 41); // "C:\Users\........\AppData\Roaming\.adept\import\"
    memcpy(result, "C:/Users/", 9);
    memcpy(&result[9], user, user_len);
    memcpy(&result[9 + user_len], "/.adept/import/", 15);
    memcpy(&result[24 + user_len], filename, filename_len + 1);
    free(user); // SPEED: Probably shouldn't have username allocated on heap (Should probably do something like GetUserNameA does)
    return result;

    #elif
    #error "Adept import folder not specified for this platform"
    #endif

    return NULL;
}

char* filename_ext(const char *filename, const char *ext_without_dot){
    // NOTE: Returns a newly allocated string with replaced file extension

    length_t i;
    length_t filename_length = strlen(filename);
    char *without_ext;
    length_t without_ext_length;
    length_t ext_length = strlen(ext_without_dot);
    bool found = false;

    for(i = filename_length - 1 ; i != 0; i--){
        if(filename[i] == '.'){
            without_ext = malloc(i);
            memcpy(without_ext, filename, i);
            without_ext_length = i;
            found = true;

            break;
        } else if(filename[i] == '/' || filename[i] == '\\'){
            without_ext = malloc(filename_length + 1);
            memcpy(without_ext, filename, filename_length);
            without_ext[filename_length] = '\0';
            without_ext_length = filename_length;
            found = true;
            break;
        }
    }

    if(!found){
        return NULL;
    }

    char *with_ext = malloc(without_ext_length + ext_length + 2);
    memcpy(with_ext, without_ext, without_ext_length);
    memcpy(&with_ext[without_ext_length], ".", 1);
    memcpy(&with_ext[without_ext_length + 1], ext_without_dot, ext_length + 1);
    free(without_ext);
    return with_ext;
}

char* filename_user(){
    // NOTE: Returns string that must be freed by caller
    // Returns the username of the current user

    #if defined(_WIN32) || defined(_WIN64)

    char username_buffer[256];
    DWORD buffer_length = 256;
    if(GetUserNameA(username_buffer, &buffer_length) == 0){
        redprintf("INTERNAL ERROR: Username exceeds 256 characters!\n");
        exit(1); // Not a good idea but whatever
    }
    char *username = malloc(buffer_length);
    memcpy(username, username_buffer, buffer_length);
    return username;

    #else
    #error "Getting the username of the current user not implemented for this platform"
    #endif

    return NULL; // Should never be reached
}

char *filename_absolute(const char *filename){
    #if defined(_WIN32) || defined(_WIN64)

    char *buffer = malloc(512);
    if(GetFullPathName(filename, 512, buffer, NULL) == 0){
        free(buffer);
        return NULL;
    }
    return buffer;

    #else
    #error "Getting absolute paths not implemented for this platform"
    #endif

    return NULL;
}

void filename_auto_ext(char **out_filename, unsigned int mode){
    if(mode == FILENAME_AUTO_PACKAGE){
        filename_append_if_missing(out_filename, ".dep");
        return;
    }

    if(mode == FILENAME_AUTO_EXECUTABLE){
        #if defined(__WIN32__)
            // Windows file extensions
            filename_append_if_missing(out_filename, ".exe");
            return;
        #elif defined(__APPLE__)
            // Macintosh file extensions
            return;
        #else
            // Linux / General Unix file extensions
            return;
        #endif
    }
}

void filename_append_if_missing(char **out_filename, const char *addition){
    length_t length = strlen(*out_filename);
    length_t addition_length = strlen(addition);
    if(addition_length == 0) return;

    if(length < addition_length || strncmp( &((*out_filename)[length - addition_length]), addition, addition_length ) != 0){
        char *new_filename = malloc(length + addition_length + 1);
        memcpy(new_filename, *out_filename, length);
        memcpy(&new_filename[length], addition, addition_length + 1);
        free(*out_filename);
        *out_filename = new_filename;
    }
}
