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

// See if a file can be parsed successfully with fasm-parse and simple benchmark

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <string_view>
#include <thread>

#include "fasm-parse.h"

int64_t getTimeInMicros() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (int64_t)t.tv_sec * 1000000 + t.tv_usec;
}

struct ParseStatistics {
  uint64_t accumulate = 0;
  uint32_t last_line = 0;
  fasm::ParseResult result = fasm::ParseResult::kSuccess;
};

ParseStatistics ParseContent(std::string_view content) {
  ParseStatistics stats;
  stats.result = fasm::parse(
      content, stderr,
      [&stats](uint32_t line, std::string_view, int, int, uint64_t bits) {
        stats.accumulate ^= bits;
        stats.last_line = line;
        return true;
      });
  return stats;
}

// Useful upper bound.
static const int kMaxThreads = 2 * std::thread::hardware_concurrency();
int GetThreadNumberToUse() {
  const char *const parallel_env = getenv("PARALLEL_FASM");
  return std::clamp(parallel_env ? atoi(parallel_env) : 1, 1, kMaxThreads);
}

// Parse file and print number of lines and performance report.
fasm::ParseResult ParseFile(const char *fasm_file, int thread_count) {
  const int fd = open(fasm_file, O_RDONLY);
  if (fd < 0) {
    perror("Can't open file");
    return fasm::ParseResult::kError;
  }

  struct stat s;
  fstat(fd, &s);
  const size_t file_size = s.st_size;

  fprintf(stdout, "Parsing %s with %zu Bytes.\n", fasm_file, file_size);
  if (file_size == 0) {
    fprintf(stdout, "Empty file.\n");
    close(fd);
    return fasm::ParseResult::kSuccess;
  }

  // Memory map everything into a convenient contiguous buffer
  void *const buffer = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (buffer == MAP_FAILED) {
    perror("Couldn't map file");
    return fasm::ParseResult::kError;
  }

  std::string_view content((const char *)buffer, file_size);
  if (content[content.size() - 1] != '\n') {
    fprintf(stdout, "File does not end in a newline\n");
    return fasm::ParseResult::kError;
  }

  // Split this into chunks at newline boundaries to be processed in parallel.
  std::vector<std::string_view> chunks(thread_count);
  for (std::string_view &chunk : chunks) {
    size_t pos = std::min(content.size(), file_size / thread_count) - 1;
    while (content[pos] != '\n') { // find next fasm line boundary
      ++pos;
    }
    chunk = content.substr(0, pos + 1);
    content = content.substr(pos + 1);
  }
  assert(content.size() == 0);  // Everything divided into chunks now.

  std::vector<std::thread *> threads(thread_count);
  std::vector<ParseStatistics> results(thread_count);

  const int64_t start_us = getTimeInMicros();
  for (int i = 0; i < thread_count; ++i) {
    threads[i] = new std::thread([&results, &chunks, i]() { //
      results[i] = ParseContent(chunks[i]);
    });
  }
  for (std::thread *thread : threads) {
    thread->join();
    delete thread;
  }
  const int64_t duration_us = getTimeInMicros() - start_us;

  ParseStatistics combined;
  for (const ParseStatistics &thread_result : results) {
    combined.accumulate ^= thread_result.accumulate;
    combined.last_line += thread_result.last_line;
    combined.result = std::max(combined.result, thread_result.result);
  }
  fprintf(stdout, "%d lines. XOR of all values: %" PRIX64 "\n",
          combined.last_line, combined.accumulate);
  constexpr float MiBFactor = 1e6 / (1 << 20);
  const float bytes_per_microsecond = 1.0f * file_size / duration_us;
  fprintf(stdout, "%d thread%s. %.3fs wall time. %.1f MiB/s; %.1f MLines/s\n",
          thread_count, thread_count > 1 ? "s" : "", duration_us / 1e6,
          bytes_per_microsecond * MiBFactor, 1.0*combined.last_line / duration_us);
  munmap(buffer, file_size);

  return combined.result;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("usage: %s <fasm-file> [<fasm-file>...]\n\tReads PARALLEL_FASM "
           "environment variable for #threads to use [1..%d].\n",
           argv[0], kMaxThreads);
    return 1;
  }

  const int thread_count = GetThreadNumberToUse();

  fasm::ParseResult combined_result = fasm::ParseResult::kSuccess;
  for (int i = 1; i < argc; ++i) {
    if (i != 1) fprintf(stdout, "\n");
    auto result = ParseFile(argv[i], thread_count);
    combined_result = std::max(combined_result, result);
  }

  return combined_result <= fasm::ParseResult::kNonCritical ? 0 : 1;
}
