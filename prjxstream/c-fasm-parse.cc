// Copyright 2022 Henner Zeller <h.zeller@acm.org>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Implementation of the c-API wrapping the C++ API

#include "c-fasm-parse.h"

#include "fasm-parse.h"

enum FasmParseResult FasmParse(StringPiece content, FILE *errstream,
                               FasmParseCallback parse_cb, void *parse_userdata,
                               FasmAnnotationCallback annotation_cb,
                               void *annotation_userdata) {
  if (annotation_cb) {
    return (FasmParseResult)fasm::parse(
        std::string_view(content.data, content.size), errstream,
        [parse_cb, parse_userdata](uint32_t line, std::string_view feature,
                                   int start_bit, int width, uint64_t bits) {
          return parse_cb(parse_userdata, line,
                          {feature.data(), feature.size()}, start_bit, width,
                          bits);
        },
        [annotation_cb, annotation_userdata](
            uint32_t line, std::string_view feature,  //
            std::string_view name, std::string_view value) {
          return annotation_cb(
              annotation_userdata, line, {feature.data(), feature.size()},
              {name.data(), name.size()}, {value.data(), value.size()});
        });

  } else {
    return (FasmParseResult)fasm::parse(
        std::string_view(content.data, content.size), errstream,
        [parse_cb, parse_userdata](uint32_t line, std::string_view feature,
                                   int start_bit, int width, uint64_t bits) {
          return parse_cb(parse_userdata, line,
                          {feature.data(), feature.size()}, start_bit, width,
                          bits);
        });
  }
}

// We're compiling with -fno-exceptions. To avoid linking with c++ libraries,
// implement this boilerplate here.
namespace std {
  void __throw_bad_function_call() { abort(); }
}
