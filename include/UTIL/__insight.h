
#ifndef INSIGHT_H
#define INSIGHT_H

// -------------------------------------------------------
// Insight API for Adept Compiler Insights
// -------------------------------------------------------

#if !defined(ADEPT_INSIGHT_BUILD) && !defined(ADEPT_INSIGHT_BUILD_IGNORE_UNDEFINED)
#error "ADEPT_INSIGHT_BUILD must be defined when using the Adept Insight API"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "DRVR/compiler.h"
#include "LEX/lex.h"
#include "PARSE/parse.h"
#include "UTIL/__insight_undo_overloads.h"

#ifdef __cplusplus
}
#endif

#endif