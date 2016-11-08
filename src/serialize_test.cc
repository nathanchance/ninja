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

#include <sstream>

#include <stdint.h>
#include <stdio.h>

#include "serialize.h"

#include "test.h"

TEST(Serialize, Bool) {
  ostringstream oss;
  Serializer s(&oss);

  s.Bool(true);
  s.Bool(false);

  istringstream iss(oss.str());
  Deserializer d(iss);

  EXPECT_EQ(d.Bool(), true);
  EXPECT_EQ(d.Bool(), false);

  string err;
  if (!EXPECT_FALSE(d.Err(err))) {
    printf("Error: %s\n", err.c_str());
  }
}

TEST(Serialize, Int) {
  int64_t values[] = {
    0, 1,
    INT8_MAX - 1,    INT8_MAX,    INT8_MAX + 1,
    UINT8_MAX - 1,   UINT8_MAX,   UINT8_MAX + 1,
    INT16_MAX - 1,   INT16_MAX,   INT16_MAX + 1,
    UINT16_MAX - 1,  UINT16_MAX,  UINT16_MAX + 1,
    INT32_MAX - 1,   INT32_MAX,   INT32_MAX + 1LL,
    UINT32_MAX - 1,  UINT32_MAX,  UINT32_MAX + 1LL,
    INT64_MAX - 1,   INT64_MAX,
    -1,
    -0x1f,           -0x20,       -0x21,           // NEG_FIXINT_MIN,
    INT8_MIN + 1,    INT8_MIN,    INT8_MIN - 1,
    -UINT8_MAX + 1,  -UINT8_MAX,  -UINT8_MAX - 1,
    INT16_MIN + 1,   INT16_MIN,   INT16_MIN - 1,
    -UINT16_MAX + 1, -UINT16_MAX, -UINT16_MAX - 1,
    INT32_MIN + 1,   INT32_MIN,   INT32_MIN - 1LL,
    -UINT32_MAX + 1, -UINT32_MAX, -UINT32_MAX - 1,
    INT64_MIN + 1,   INT64_MIN,
  };

  ostringstream oss;
  Serializer s(&oss);

  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    s.Int(values[i]);
  }

  istringstream iss(oss.str());
  Deserializer d(iss);

  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    EXPECT_EQ(d.Int(), values[i]);
  }

  string err;
  if (!EXPECT_FALSE(d.Err(err))) {
    printf("Error: %s\n", err.c_str());
  }
}

TEST(Serialize, Uint) {
  uint64_t values[] = {
    0, 1,
    INT8_MAX - 1,    INT8_MAX,    INT8_MAX + 1,
    UINT8_MAX - 1,   UINT8_MAX,   UINT8_MAX + 1,
    INT16_MAX - 1,   INT16_MAX,   INT16_MAX + 1,
    UINT16_MAX - 1,  UINT16_MAX,  UINT16_MAX + 1,
    INT32_MAX - 1,   INT32_MAX,   INT32_MAX + 1LL,
    UINT32_MAX - 1,  UINT32_MAX,  UINT32_MAX + 1LL,
    INT64_MAX - 1,   INT64_MAX,   INT64_MAX + 1ULL,
    UINT64_MAX - 1,  UINT64_MAX,
  };

  ostringstream oss;
  Serializer s(&oss);

  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    s.Uint(values[i]);
  }

  istringstream iss(oss.str());
  Deserializer d(iss);

  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    EXPECT_EQ(d.Uint(), values[i]);
  }

  string err;
  if (!EXPECT_FALSE(d.Err(err))) {
    printf("Error: %s\n", err.c_str());
  }
}

TEST(Serialize, String) {
  vector<string> values;

  values.push_back("");
  values.push_back("a");
  values.push_back(string(0x1f, 'a')); // FIXSTR_MAX
  values.push_back(string(0x20, 'a')); // FIXSTR_MAX + 1
  values.push_back(string(UINT8_MAX, 'a'));
  values.push_back(string(UINT8_MAX + 1, 'a'));
  values.push_back(string(UINT16_MAX, 'a'));
  values.push_back(string(UINT16_MAX + 1, 'a'));

  {
    ostringstream oss;
    Serializer s(&oss);

    for (vector<string>::iterator it = values.begin(); it != values.end(); ++it) {
      s.String(*it);
    }

    istringstream iss(oss.str());
    Deserializer d(iss);

    for (vector<string>::iterator it = values.begin(); it != values.end(); ++it) {
      string str;
      d.String(str);
      EXPECT_EQ(str, *it);
    }

    string err;
    if (!EXPECT_FALSE(d.Err(err))) {
      printf("Error: %s\n", err.c_str());
    }
  }

  {
    ostringstream oss;
    Serializer s(&oss);

    for (vector<string>::iterator it = values.begin(); it != values.end(); ++it) {
      s.String(it->size());
      oss << *it;
    }


    istringstream iss(oss.str());
    Deserializer d(iss);

    string str;

    for (vector<string>::iterator it = values.begin(); it != values.end(); ++it) {
      string str;
      d.String(str);
      EXPECT_EQ(str, *it);
    }

    string err;
    if (!EXPECT_FALSE(d.Err(err))) {
      printf("Error: %s\n", err.c_str());
    }
  }
}

TEST(Serialize, Array) {
  size_t lengths[] = {
    0, 1,            0xf,         0x10,          // FIXARRAY_MAX
    UINT8_MAX - 1,   UINT8_MAX,   UINT8_MAX + 1,
    UINT16_MAX - 1,  UINT16_MAX,  UINT16_MAX + 1,
  };

  {
    ostringstream oss;
    Serializer s(&oss);

    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
      vector<string> v;
      v.resize(lengths[i]);
      s.Array(v);
    }

    istringstream iss(oss.str());
    Deserializer d(iss);

    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
      size_t length = d.Array();
      EXPECT_EQ(length, lengths[i]);
      string str;
      for (size_t j = 0; j < length; j++) {
        d.String(str);
      }
    }

    string err;
    if (!EXPECT_FALSE(d.Err(err))) {
      printf("Error: %s\n", err.c_str());
    }
  }

  {
    ostringstream oss;
    Serializer s(&oss);

    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
      s.Array(lengths[i]);
      for (size_t j = 0; j < lengths[i]; j++) {
        s.String("");
      }
    }

    istringstream iss(oss.str());
    Deserializer d(iss);

    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
      size_t length = d.Array();
      EXPECT_EQ(length, lengths[i]);
      string str;
      for (size_t j = 0; j < length; j++) {
        d.String(str);
      }
    }

    string err;
    if (!EXPECT_FALSE(d.Err(err))) {
      printf("Error: %s\n", err.c_str());
    }
  }
}
