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

/* Like fasm-validation-parse, but implemented in C to demonstrate the
   C API wrapper in c-fasm-parse.h */

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "c-fasm-parse.h"

int64_t getTimeInMicros() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (int64_t)t.tv_sec * 1000000 + t.tv_usec;
}

struct ParseStatistics {
  uint64_t accumulate;
  uint32_t last_line;
};

bool StatsAccumulator(void *user_data, uint32_t line, StringPiece feature,
                     int start_bit, int width, uint64_t bits) {
  struct ParseStatistics *stats = (struct ParseStatistics *)user_data;
  stats->accumulate ^= bits;
  stats->last_line = line;
  return true;
}

/* Parse file and print number of lines and performance report. Returns 1
 * if error occured */
int ParseFile(const char *fasm_file) {
  const int fd = open(fasm_file, O_RDONLY);
  if (fd < 0) {
    perror("Can't open file");
    return ParseResultError;
  }

  struct stat s;
  fstat(fd, &s);
  const size_t file_size = s.st_size;

  fprintf(stdout, "Parsing %s with % " PRId64 " Bytes.\n", fasm_file,
          (uint64_t)file_size);
  if (file_size == 0) {
    fprintf(stdout, "Empty file.\n");
    close(fd);
    return ParseResultSuccess;
  }

  /* Memory map everything into a convenient contiguous buffer */
  void *const buffer = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (buffer == MAP_FAILED) {
    perror("Couldn't map file");
    return ParseResultError;
  }

  StringPiece content;
  content.data = (const char *)buffer;
  content.size = file_size;

  struct ParseStatistics stats = {0};
  const int64_t start_us = getTimeInMicros();
  enum FasmParseResult result =
      FasmParse(content, stderr, &StatsAccumulator, &stats, NULL, NULL);
  const int64_t duration_us = getTimeInMicros() - start_us;
  fprintf(stdout, "%d lines. XOR of all values: %" PRIX64 "\n", stats.last_line,
          stats.accumulate);
  const float MiBFactor = 1e6 / (1 << 20);
  const float bytes_per_microsecond = 1.0f * file_size / duration_us;
  fprintf(stdout, "%.3fs wall time. %.1f MiB/s; %.1f MLines/s\n",
          duration_us / 1e6, bytes_per_microsecond * MiBFactor,
          1.0 * stats.last_line / duration_us);
  munmap(buffer, file_size);

  return result > ParseResultNonCritical;
}

int main(int argc, char *argv[]) {
  int error_sum = 0;
  int i;

  if (argc < 2) {
    printf("usage: %s <fasm-file> [<fasm-file>...]\n", argv[0]);
    return 1;
  }

  for (i = 1; i < argc; ++i) {
    if (i != 1) fprintf(stdout, "\n");
    error_sum += ParseFile(argv[i]);
  }

  return error_sum;
}
