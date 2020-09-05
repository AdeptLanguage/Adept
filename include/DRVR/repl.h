
#ifndef _ISAAC_REPL_H
#define _ISAAC_REPL_H

#include "UTIL/ground.h"
#include "DRVR/compiler.h"

typedef struct {
    length_t global_marker;
    length_t main_marker;
} repl_history_entry_t;

typedef struct {
    compiler_t *compiler;
    strong_cstr_t global_code;
    length_t global_code_length;
    length_t global_code_capacity;

    strong_cstr_t main_code;
    length_t main_code_length;
    length_t main_code_capacity;

    strong_cstr_t wait_code;
    length_t wait_code_length;
    length_t wait_code_capacity;

    repl_history_entry_t *history;
    length_t history_length;
    length_t history_capacity;

    bool waiting_for_end;
} repl_t;

// ---------------- repl_init ----------------
// Constructs a 'repl_t'
void repl_init(repl_t *repl, compiler_t *compiler);

// ---------------- repl_free ----------------
// Frees a 'repl_t'
void repl_free(repl_t *repl);

// ---------------- repl_execute ----------------
// Runs some code in the REPL
// Returns whether the session should end
bool repl_execute(repl_t *repl, weak_cstr_t code);

// ---------------- repl_execute ----------------
// Runs a REPL session
successful_t repl_run(repl_t *repl);

// ---------------- repl_add_history ----------------
// Rembers the code state of a REPL
void repl_add_history(repl_t *repl);

// ---------------- repl_undo ----------------
// Undoes one state of REPL history
void repl_undo(repl_t *repl);

// ---------------- repl_reset_compiler ----------------
// Cleans up after running a REPL session
void repl_reset_compiler(repl_t *repl);

// ---------------- repl_should_main ----------------
// Returns whether the code should be run inside of main
bool repl_should_main(repl_t *repl, weak_cstr_t inout_code);

// ---------------- repl_append_* ----------------
// Appends code to areas
void repl_append_main(repl_t *repl, weak_cstr_t code);
void repl_append_global(repl_t *repl, weak_cstr_t code);
void repl_append_wait(repl_t *repl, weak_cstr_t code);

// ---------------- repl_sanitize ----------------
// NOTE: Requires that 'inout_buffer' has space for appended '\n'
// Prepares a buffer for REPL execution
void repl_helper_sanitize(char *inout_buffer);

#endif // _ISAAC_REPL_H
