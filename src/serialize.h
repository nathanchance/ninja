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

#ifndef NINJA_SERIALIZE_H_
#define NINJA_SERIALIZE_H_

#include <stdint.h>

#include <iostream>
#include <string>
#include <vector>

#include "filebuf.h"

using namespace std;

/// MessagePack-compatibile serializer
struct Serializer {
  Serializer(ostream* out) : filebuf_(NULL) {
    out_ = out;
  }

  Serializer(FILE* file) {
    filebuf_ = new ofilebuf(file);
    out_ = new ostream(filebuf_);
  }

  Serializer(int fd) : filebuf_(NULL), out_(NULL) {
    FILE* file = fdopen(fd, "wb");
    filebuf_ = new ofilebuf(file);
    out_ = new ostream(filebuf_);
  }

  ~Serializer() {
    if (filebuf_) {
      delete out_;
      delete filebuf_;
    }
  }

  /// Serialize a boolean value
  void Bool(bool);

  /// Serialize a signed integer
  void Int(int64_t);

  /// Serialize an unsigned integer
  void Uint(uint64_t);

  /// Serialize a string
  void String(string);

  /// Serialize the length of a string.  The caller must write out the current
  /// contents of the serialization buffer followed by the bytes of the string.
  void String(size_t);

  /// Serialize an array with the given number of elements.  The caller must
  /// call one of the serialization methods for each element.
  void Array(size_t);

  /// Serialize an array of strings
  void Array(const vector<string>& v);

//  /// Returns a reference to the internal buffer of previous calls to serialize
//  /// methods.  Contents are valid until the next call to a serialize function
//  /// or Reset().
//  const string& Buf() { return buf_; }
//
//  /// Empty the internal buffer.  Does not release the memory to allow for fast
//  /// reuse.
//  void Reset() { buf_.clear(); }

  void Flush() { out_->flush(); }

private:
  ofilebuf* filebuf_;
  ostream* out_;
};

/// Deserializer that can handle messages encoded by Serializer
struct Deserializer {
  Deserializer(istream& in) : in_(in), err_message_(), err_(false) {}
  ~Deserializer() {}

  /// Deserialize a boolean value
  bool Bool();

  /// Deserialize a signed integer
  int64_t Int();

  /// Deserialize an unsigned integer
  uint64_t Uint();

  /// Deserialize a string
  void String(string&);

  /// Deserialize an array.  Returns the number of elements in the found array.
  /// The caller must call one of the deserialization methods for each element.
  size_t Array();

  /// Returns true and sets an error string if an error has occurred.
  bool Err(string&);

private:
  bool CheckEOF();
  uint8_t Type();
  void SetErr(const string& err);

  istream& in_;
  string err_message_;
  bool err_;
};

#endif // NINJA_SERIALIZE_H_
