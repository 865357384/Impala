// Copyright (c) 2011 Cloudera, Inc. All rights reserved.

#include "runtime/string-value.h"
using namespace std;

namespace impala {

int StringValue::Compare(const StringValue& other) {
  if (len == 0 && other.len == 0) return 0;
  if (len == 0) return -1;
  if (other.len == 0) return 1;
  int result = memcmp(ptr, other.ptr, std::min(len, other.len));
  if (result == 0 && len != other.len) {
    return (len < other.len ? -1 : 1);
  } else {
    return result;
  }
}

string StringValue::DebugString() const {
  return string(ptr, len);
}

ostream& operator<<(ostream& os, const StringValue& string_value) {
  return os << string_value.DebugString();
}

}
