
#define REPL_FILENAME ".adeptrepl.tmp"
#define REPL_OUTPUT_FILENAME ".adeptreplx.tmp"

#include "DRVR/repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "UTIL/trait.h"
#include "UTIL/util.h"

int main(int argc, char **argv);

void repl_init(repl_t *repl, compiler_t *compiler){
    // Setup REPL
    repl->compiler = compiler;

    repl->global_code = malloc(10240); // 10 kb
    repl->global_code_length = 0;
    repl->global_code_capacity = 10240;

    repl->main_code = malloc(5120); // 5 kb
    repl->main_code_length = 0;
    repl->main_code_capacity = 10240;

    repl->wait_code = malloc(2048); // 2 kb
    repl->wait_code_length = 0;
    repl->wait_code_capacity = 2048;

    repl->history = malloc(sizeof(repl_history_entry_t) * 256);
    repl->history_length = 0;
    repl->history_capacity = 256;

    repl->waiting_for_end = false;

    repl_append_global(repl, "import basics\n");
    repl_add_history(repl);
}

void repl_free(repl_t *repl){
    free(repl->global_code);
    free(repl->main_code);

    // Remove temporary files
    if(file_exists(REPL_FILENAME)) remove(REPL_FILENAME);
    if(file_exists(REPL_OUTPUT_FILENAME)) remove(REPL_OUTPUT_FILENAME);
}

bool repl_execute(repl_t *repl, weak_cstr_t code){
    // Exit on '#halt'
    if(
        strcmp(code, "#halt\n") == 0 ||
        strcmp(code, "halt\n") == 0 ||
        strcmp(code, "#exit\n") == 0 ||
        strcmp(code, "exit\n") == 0 ||
        strcmp(code, "#quit\n") == 0 ||
        strcmp(code, "quit\n") == 0 ||
        strcmp(code, "q\n") == 0) return true;

    // Reset on '#reset'
    if(strcmp(code, "#reset\n") == 0){
        compiler_t *compiler = repl->compiler;

        printf("[reset]\n");
        repl_free(repl);
        repl_init(repl, compiler);
        return false;
    }

    // Undo change on '#undo'
    if(strcmp(code, "#undo\n") == 0){
        printf("[undo]\n");
        repl_undo(repl);
        return false;
    }

    if(strcmp(code, "#clear\n") == 0){
        #ifdef _WIN32
        printf("#clear isn't supported yet on Windows\n");
        #else
        write(STDOUT_FILENO, "\x1b[2J", 4);
        #endif
        return false;
    }

    if(repl_should_main(repl, code)){
        repl_append_main(repl, code);

        successful_t ran = repl_run(repl);
        if(ran){
            printf("[run complete]\n");

            // Add to history if successfully run
            if(strcmp(code, "\n") != 0) repl_add_history(repl);
        } else {
            // Undo last change if failed to run
            repl_undo(repl);
        }
    } else if(strcmp(code, "\n") != 0){
        repl_append_global(repl, code);
        repl_add_history(repl);
    }

    return false;
}

successful_t repl_run(repl_t *repl){
    FILE *f = fopen(REPL_FILENAME, "w");
    if(f == NULL) return false;

    fwrite(repl->global_code, sizeof(char), repl->global_code_length, f);
    fprintf(f, "func main(argc int, argv **ubyte) {\n");
    fwrite(repl->main_code, sizeof(char), repl->main_code_length, f);
    fprintf(f, "}");
    fclose(f);

    char *arguments[] = {
        repl->compiler->location, REPL_FILENAME, "-e", "--ignore-unused", "-o", REPL_OUTPUT_FILENAME
    };

    // Lazy REPL
    return main((int)(sizeof(arguments) / sizeof(char*)), arguments) == SUCCESS;
}

void repl_add_history(repl_t *repl){
    expand((void**) &repl->history, sizeof(repl_history_entry_t), repl->history_length, &repl->history_capacity, 1, 256);
    
    repl_history_entry_t *entry = &repl->history[repl->history_length++];
    entry->global_marker = repl->global_code_length;
    entry->main_marker = repl->main_code_length;
}

void repl_undo(repl_t *repl){
    if(repl->history == NULL || repl->history_length <= 1) return;

    repl_history_entry_t *entry = &repl->history[--repl->history_length - 1];
    repl->global_code_length = entry->global_marker;
    repl->main_code_length = entry->main_marker;

    repl->global_code[repl->global_code_length + 1] = '\0';
    repl->main_code[repl->main_code_length + 1] = '\0';
}

void repl_reset_compiler(repl_t *repl){
    compiler_free_objects(repl->compiler);
    compiler_free_error(repl->compiler);
    compiler_free_warnings(repl->compiler);

    // Reset result flags
    repl->compiler->result_flags = TRAIT_NONE;
}

bool repl_should_main(repl_t *repl, weak_cstr_t inout_code){
    length_t length = strlen(inout_code);
    bool backslash = (length > 1 && inout_code[length - 2] == '\\');

    if(backslash){
        // Overwrite '\'
        inout_code[length - 2] = '\n';
        inout_code[length - 1] = '\0';
    }

    if(repl->waiting_for_end || backslash){
        repl_append_wait(repl, inout_code);
        
        // Get whether character before newline is '\'
        if(!backslash){
            repl->waiting_for_end = false;

            if(repl_should_main(repl, repl->wait_code)){
                repl_append_main(repl, repl->wait_code);
            } else {
                repl_append_global(repl, repl->wait_code);
            }

            // Clear waiting code
            repl->wait_code[0] = '\0';
            repl->wait_code_length = 0;

            // Clear inout code
            inout_code[0] = '\n';
            inout_code[1] = '\0';

            repl_add_history(repl);
        } else {
            // Clear inout code
            inout_code[0] = '\n';
            inout_code[1] = '\0';
            repl->waiting_for_end = true;
        }

        return false;
    }
    
    if(length >= 7 && strncmp(inout_code, "import ", 7)  == 0) return false;
    if(length >= 7 && strncmp(inout_code, "pragma ", 7)  == 0) return false;
    if(length >= 5 && strncmp(inout_code, "func ", 5)    == 0) return false;
    if(length >= 8 && strncmp(inout_code, "foreign ", 8) == 0) return false;
    if(length >= 7 && strncmp(inout_code, "struct ", 7)  == 0) return false;
    return true;
}

void repl_append_main(repl_t *repl, weak_cstr_t code){
    if(strcmp(code, "\n") == 0) return;

    length_t code_length = strlen(code);
    expand((void**) &repl->main_code, sizeof(char), repl->main_code_length, &repl->main_code_capacity, code_length + 1, 5120);

    memcpy(&repl->main_code[repl->main_code_length], code, code_length + 1);
    repl->main_code_length += code_length;
}

void repl_append_global(repl_t *repl, weak_cstr_t code){
    if(strcmp(code, "\n") == 0) return;

    length_t code_length = strlen(code);
    expand((void**) &repl->global_code, sizeof(char), repl->global_code_length, &repl->global_code_capacity, code_length + 1, 5120);

    memcpy(&repl->global_code[repl->global_code_length], code, code_length + 1);
    repl->global_code_length += code_length;
}

void repl_append_wait(repl_t *repl, weak_cstr_t code){
    length_t code_length = strlen(code);
    expand((void**) &repl->wait_code, sizeof(char), repl->wait_code_length, &repl->wait_code_capacity, code_length + 1, 5120);

    memcpy(&repl->wait_code[repl->wait_code_length], code, code_length + 1);
    repl->wait_code_length += code_length;
}

void repl_helper_sanitize(char *inout_buffer){
    // Strip off leading whitespace
    length_t buffer_start = 0;
    length_t buffer_length = strlen(inout_buffer);

    while(inout_buffer[buffer_start] == ' ' || inout_buffer[buffer_start] == '\n' || inout_buffer[buffer_start] == '\t'){
        buffer_start++;
    }

    buffer_length -= buffer_start;
    memmove(inout_buffer, &inout_buffer[buffer_start], buffer_length);

    // Strip off trailing whitespace
    while(inout_buffer[0] != '\0' && (inout_buffer[buffer_length - 1] == ' ' || inout_buffer[buffer_length - 1] == '\n' || inout_buffer[buffer_length - 1] == '\t')){
        buffer_length--;
    }

    // Append newline character
    inout_buffer[buffer_length]     = '\n';
    inout_buffer[buffer_length + 1] = '\0';
}
