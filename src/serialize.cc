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

#define __STDC_LIMIT_MACROS

#include <stdint.h>
#include <stdio.h>

#include "serialize.h"

//static const uint8_t POS_FIXINT = 0x00; // 0x00 - 0x7f
//static const uint8_t FIXMAP     = 0x80; // 0x80 - 0x8f
static const uint8_t FIXARRAY     = 0x90; // 0x90 - 0x9f
static const uint8_t FIXSTR       = 0xa0; // 0xa0 - 0xbf
//static const uint8_t NIL        = 0xc0;
static const uint8_t FALSE      = 0xc2;
static const uint8_t TRUE       = 0xc3;
//static const uint8_t BIN8       = 0xc4;
//static const uint8_t BIN16      = 0xc5;
//static const uint8_t BIN32      = 0xc6;
//static const uint8_t EXT8       = 0xc7;
//static const uint8_t EXT16      = 0xc8;
//static const uint8_t EXT32      = 0xc9;
//static const uint8_t FLOAT32    = 0xca;
//static const uint8_t FLOAT64    = 0xcb;
static const uint8_t UINT8        = 0xcc;
static const uint8_t UINT16       = 0xcd;
static const uint8_t UINT32       = 0xce;
static const uint8_t UINT64       = 0xcf;
static const uint8_t INT8         = 0xd0;
static const uint8_t INT16        = 0xd1;
static const uint8_t INT32        = 0xd2;
static const uint8_t INT64        = 0xd3;
//static const uint8_t FIXEXT1    = 0xd4;
//static const uint8_t FIXEXT2    = 0xd5;
//static const uint8_t FIXEXT4    = 0xd6;
//static const uint8_t FIXEXT8    = 0xd7;
//static const uint8_t FIXEXT16   = 0xd8;
static const uint8_t STR8         = 0xd9;
static const uint8_t STR16        = 0xda;
static const uint8_t STR32        = 0xdb;
static const uint8_t ARRAY16      = 0xdc;
static const uint8_t ARRAY32      = 0xdd;
//static const uint8_t MAP16      = 0xde;
//static const uint8_t MAP32      = 0xdf;
static const uint8_t NEG_FIXINT = 0xe0; // 0xe0 - 0xff

static const int64_t POS_FIXINT_MAX = 0x7f;
static const int64_t NEG_FIXINT_MIN = -0x20;
//static const size_t  FIXMAP_MAX     = 0xf;
static const size_t  FIXARRAY_MAX   = 0xf;
static const size_t  FIXSTR_MAX     = 0x1f;

template<typename T>
static inline char outBE(T v, int n) {
  return static_cast<char>((v >> (8 * n)) & 0xff);
}

template<typename T>
void outBE8(uint8_t type, ostream* out, T v) {
  char bytes[9] = {
    static_cast<char>(type),
    outBE(v, 7),
    outBE(v, 6),
    outBE(v, 5),
    outBE(v, 4),
    outBE(v, 3),
    outBE(v, 2),
    outBE(v, 1),
    outBE(v, 0),
  };
  out->write(bytes, sizeof(bytes));
}

template<typename T>
void outBE4(uint8_t type, ostream* out, T v) {
  char bytes[5] = {
    static_cast<char>(type),
    outBE(v, 3),
    outBE(v, 2),
    outBE(v, 1),
    outBE(v, 0),
  };
  out->write(bytes, sizeof(bytes));
}

template<typename T>
void outBE2(uint8_t type, ostream* out, T v) {
  char bytes[3] = {
    static_cast<char>(type),
    outBE(v, 1),
    outBE(v, 0),
  };
  out->write(bytes, sizeof(bytes));
}

template<typename T>
void outBE1(uint8_t type, ostream* out, T v) {
  char bytes[2] = {
    static_cast<char>(type),
    outBE(v, 0),
  };
  out->write(bytes, sizeof(bytes));
}

void Serializer::Bool(bool v) {
  out_->put(static_cast<char>(v ? TRUE : FALSE));
}

void Serializer::Int(int64_t v) {
  if (v >= NEG_FIXINT_MIN && v <= POS_FIXINT_MAX) {
    out_->put(static_cast<char>(v & 0xff));
  } else if (v >= INT8_MIN && v <= INT8_MAX) {
    outBE1(INT8, out_, v);
  } else if (v >= INT16_MIN && v <= INT16_MAX) {
    outBE2(INT16, out_, v);
  } else if (v >= INT32_MIN && v <= INT32_MAX) {
    outBE4(INT32, out_, v);
  } else {
    outBE8(INT64, out_, v);
  }
}

void Serializer::Uint(uint64_t v) {
  if (v <= static_cast<uint64_t>(POS_FIXINT_MAX)) {
    out_->put(static_cast<char>(v & 0xff));
  } else if (v <= UINT8_MAX) {
    outBE1(UINT8, out_, v);
  } else if (v <= UINT16_MAX) {
    outBE2(UINT16, out_, v);
  } else if (v <= UINT32_MAX) {
    outBE4(UINT32, out_, v);
  } else {
    outBE8(UINT64, out_, v);
  }
}

void Serializer::String(string s) {
  String(s.length());
  out_->write(s.data(), static_cast<streamsize>(s.size()));
}

void Serializer::String(size_t s) {
  if (s <= FIXSTR_MAX) {
    out_->put(FIXSTR + static_cast<uint8_t>(s));
  } else if (s <= UINT8_MAX) {
    outBE1(STR8, out_, s);
  } else if (s <= UINT16_MAX) {
    outBE2(STR16, out_, s);
  } else {
    outBE4(STR32, out_, s);
  }
}

void Serializer::Array(const vector<string>& v) {
  Array(v.size());
  for (vector<string>::const_iterator s = v.begin(); s != v.end(); ++s) {
    String(*s);
  }
}

void Serializer::Array(size_t s) {
  if (s <= FIXARRAY_MAX) {
    out_->put(FIXARRAY + static_cast<char>(s));
  } else if (s <= UINT16_MAX) {
    outBE2(ARRAY16, out_, s);
  } else {
    outBE4(ARRAY32, out_, s);
  }
}

template<typename T>
static inline T inBE(char c, int n) {
  uint8_t byte = static_cast<uint8_t>(c);

  return static_cast<T>(static_cast<uint64_t>(byte) << (n * 8));
}

template<typename T>
T inBE8(istream& in) {
  char bytes[8];
  in.read(bytes, sizeof(bytes));
  uint64_t v =
      inBE<uint64_t>(bytes[0], 7) |
      inBE<uint64_t>(bytes[1], 6) |
      inBE<uint64_t>(bytes[2], 5) |
      inBE<uint64_t>(bytes[3], 4) |
      inBE<uint64_t>(bytes[4], 3) |
      inBE<uint64_t>(bytes[5], 2) |
      inBE<uint64_t>(bytes[6], 1) |
      inBE<uint64_t>(bytes[7], 0);
  return static_cast<T>(v);
}

template<typename T>
T inBE4(istream& in) {
  char bytes[4];
  in.read(bytes, sizeof(bytes));
  uint32_t v =
      inBE<uint32_t>(bytes[0], 3) |
      inBE<uint32_t>(bytes[1], 2) |
      inBE<uint32_t>(bytes[2], 1) |
      inBE<uint32_t>(bytes[3], 0);
  return static_cast<T>(v);
}

template<typename T>
T inBE2(istream& in) {
  char bytes[2];
  in.read(bytes, sizeof(bytes));
  uint16_t v =
      inBE<uint16_t>(bytes[0], 1) |
      inBE<uint16_t>(bytes[1], 0);
  return static_cast<T>(v);
}

template<typename T>
T inBE1(istream& in) {
  int byte = in.get();
  return static_cast<T>(byte);
}

void Deserializer::SetErr(const string& err) {
  if (!err_) {
    err_ = true;
    err_message_ = err;
  }
}

bool Deserializer::CheckEOF() {
  bool eof = in_.eof();
  if (eof) {
    SetErr("Unexpected EOF while reading");
  }
  return eof;
}

uint8_t Deserializer::Type() {
  int type = in_.get();
  if (in_.eof()) {
    SetErr("Unexpected EOF while reading type");
  }

  return static_cast<uint8_t>(type);
}

bool Deserializer::Bool() {
  uint8_t type = Type();
  if (type == TRUE) {
    return true;
  } else if (type == FALSE) {
    return false;
  } else {
    SetErr("unexpected type while reading bool");
    return false;
  }
}

int64_t Deserializer::Int() {
  uint8_t type = Type();
  int64_t ret;
  if (type <= POS_FIXINT_MAX) {
    ret = type;
  } else if (type >= NEG_FIXINT) {
    ret = static_cast<int8_t>(type);
  } else if (type == INT8) {
    ret = inBE1<int8_t>(in_);
  } else if (type == INT16) {
    ret = inBE2<int16_t>(in_);
  } else if (type == INT32) {
    ret = inBE4<int32_t>(in_);
  } else if (type == INT64) {
    ret = inBE8<int64_t>(in_);
  } else {
    SetErr("unexpected type while reading int");
    return 0;
  }

  CheckEOF();
  return ret;
}

uint64_t Deserializer::Uint() {
  uint8_t type = Type();
  uint64_t ret;
  if (type <= POS_FIXINT_MAX) {
    ret = type;
  } else if (type == UINT8) {
    ret = inBE1<uint8_t>(in_);
  } else if (type == UINT16) {
    ret = inBE2<uint16_t>(in_);
  } else if (type == UINT32) {
    ret = inBE4<uint32_t>(in_);
  } else if (type == UINT64) {
    ret = inBE8<uint64_t>(in_);
  } else {
    SetErr("unexpected type while reading uint");
    return 0;
  }

  CheckEOF();
  return ret;
}

void Deserializer::String(string& s) {
  uint8_t type = Type();
  size_t len;
  if (type >= FIXSTR && type <= FIXSTR + FIXSTR_MAX) {
    len = type - FIXSTR;
  } else if (type == STR8) {
    len = inBE1<size_t>(in_);
  } else if (type == STR16) {
    len = inBE2<size_t>(in_);
  } else if (type == STR32) {
    len = inBE4<size_t>(in_);
  } else {
    SetErr("unexpected type while reading string");
    return;
  }

  if (CheckEOF()) {
    s = "";
    return;
  }

  s.resize(len);
  if (len > 0) {
    in_.read(&s[0], static_cast<streamsize>(s.size()));
  }
}

size_t Deserializer::Array() {
  uint8_t type = Type();
  size_t ret;
  if (type >= FIXARRAY && type <= FIXARRAY + FIXARRAY_MAX) {
    ret = type - FIXARRAY;
  } else if (type == ARRAY16) {
    ret = inBE2<size_t>(in_);
  } else if (type == ARRAY32) {
    ret = inBE4<size_t>(in_);
  } else {
    SetErr("unexpected type while reading array");
    return 0;
  }

  CheckEOF();
  return ret;
}

bool Deserializer::Err(string& err) {
  if (err_) {
    err = err_message_;
  }

  return err_;
}
