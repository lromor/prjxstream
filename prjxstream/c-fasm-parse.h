/* Copyright 2022 Henner Zeller <h.zeller@acm.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
  C-API wrapper around C++ fasm-parse.h. Then, need also link c-fasm-parse.o

  However, if you can, use the C++ implementation instead.
  C-API is only provied as it might be easier to bind to other languages.
*/

#ifndef C_FASM_PARSE_H
#define C_FASM_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pointer to a block of memory. Since the 'string' is not nul-terminated,
 * use the sized formatting with printf(), e.g.
 * printf("%.*s", (int)piece.size, piece.data);
 * Conceptually like struct iovec or C++ std::string_view.
 */
typedef struct StringPiece {
  const char *data;
  size_t size;
} StringPiece;

/* Parse callback for FASM lines. The "feature" found in line number "line"
 * is set the values given in "bits", starting from lowest "start_bit" (lsb)
 * with given "width".
 * Returns 'true' if it wants to continue get callbacks or 'false' if it
 * wants the parsing to abort.
 */
typedef bool (*FasmParseCallback)(void *user_data, uint32_t line,
				  StringPiece feature, int start_bit, int width,
				  uint64_t bits);

/* Optional callback that receives annotation name/value pairs. If there are
 * multiple annotations per feature, this is called multiple times.
 */
typedef void (*FasmAnnotationCallback)(void *user_data, uint32_t line,
                                       StringPiece feature, StringPiece name,
                                       StringPiece value);

/* Result values in increasing amount of severity. Start to worry at Skipped. */
enum FasmParseResult {
  ParseResultSuccess,     /* Successful parse */
  ParseResultInfo,        /* Got info messages, mostly FYI */
  ParseResultNonCritical, /* Found strange values, but mostly non-critical */
  ParseResultSkipped,     /* There were lines that had to be skipped. */
  ParseResultUserAbort,   /* The callback returned 'false' to abort. */
  ParseResultError        /* Errornous input */
};

/*
 * Parse FPGA assembly file, send parsed values to "parse_callback".
 * The "content" is the buffer to parse; last line needs to end with a newline.
 * Errors/Warnings are reported to "errstream". The user-data
 * is passed along to the corresponding callbacks.
 *
 * If the optional "annotation_callback" is provided, it receives annotations
 * in {...} blocks. Quotes around value is removed, escaped characters are
 * preserved.
 *
 * The "feature" StringPiece as well as "name" and "value" for the callbacks
 * are guranteed to not be ephemeral but backed by the original content,
 * so it is valid for the lifetime of "content".
 *
 * If there are warnings or errors, parsing will continue if possible.
 * The most severe issue found is returned.
 *
 * Spec: https://fasm.readthedocs.io/en/latest/specification/syntax.html
 */
enum FasmParseResult FasmParse(StringPiece content, FILE *errstream,
                               FasmParseCallback parse_cb, void *parse_userdata,
                               FasmAnnotationCallback annotation_cb,
                               void *annotation_userdata);

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* C_FASM_PARSE_H */
