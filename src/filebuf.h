// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NINJA_FILEBUF_H_
#define NINJA_FILEBUF_H_

#include <stdio.h>

#include <streambuf>

class ofilebuf : public std::streambuf {
 public:
  ofilebuf(size_t sz = 0) : f_(NULL), buffer_(sz) { }

  ofilebuf(FILE* f, size_t sz = BUFSIZ) : f_(f), buffer_(sz) {
    setvbuf(f_, NULL, _IONBF, 0);
    char* buf = &buffer_.front();
    setp(buf, buf + buffer_.size() - 1);
  }
  ~ofilebuf() { fclose(f_); }

 private:
  int_type overflow(int_type c) {
    if (c != traits_type::eof()) {
      *pptr() = c;
      pbump(1);
    }

    if (sync() < 0) {
      return traits_type::eof();
    }

    return 0;
  }

  int sync() {
    size_t n = pptr() - pbase();
    size_t ret = fwrite(pbase(), 1, n, f_);

    pbump(-ret);

    if (ret < n) {
      return -1;
    }

    return 0;
  }

 private:
  FILE* f_;
  std::vector<char> buffer_;
};

#endif // NINJA_FILEBUF_H_
