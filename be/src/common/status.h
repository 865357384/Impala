// Copyright (c) 2011 Cloudera, Inc. All rights reserved.

#ifndef IMPALA_COMMON_STATUS_H
#define IMPALA_COMMON_STATUS_H

#include <string>
#include <vector>

#include "common/logging.h"
#include "common/compiler-util.h"
#include "gen-cpp/Types_types.h"  // for TStatus

namespace impala {

// Status is used as a function return type to indicate success or failure
// of the function. In case of successful completion, it only occupies sizeof(void*)
// statically allocated memory. In the error case, it records a stack of error messages.
//
// example:
// Status fnB(int x) {
//   Status status = fnA(x);
//   if (!status.ok()) {
//     status.AddErrorMsg("fnA(x) went wrong");
//     return status;
//   }
// }
//
// TODO: macros:
// RETURN_IF_ERROR(status) << "msg"
// MAKE_ERROR() << "msg"

class Status {
 public:
  Status(): error_detail_(NULL) {}

  static const Status OK;

  // copy c'tor makes copy of error detail so Status can be returned by value
  Status(const Status& status)
    : error_detail_(
        status.error_detail_ != NULL
          ? new ErrorDetail(*status.error_detail_)
          : NULL) {
  }

  // c'tor for error case
  Status(const std::string& error_msg)
    : error_detail_(new ErrorDetail(error_msg)) {
    LOG(WARNING) << "Error Status: " << error_msg;
  }

  ~Status() {
    if (error_detail_ != NULL) delete error_detail_;
  }


  // same as copy c'tor
  Status& operator=(const Status& status);

  // "Copy" c'tor from TStatus.
  // TODO: adopt the status code
  Status(const TStatus& status);

  // same as previous c'tor
  Status& operator=(const TStatus& status);

  // assign from stringstream
  Status& operator=(const std::stringstream& stream);

  bool ok() const { return error_detail_ == NULL; }

  void AddErrorMsg(const std::string& msg);

  // Return all accumulated error msgs.
  void GetErrorMsgs(std::vector<std::string>* msgs) const;

  // Convert into TStatus.
  void ToThrift(TStatus* status);

  // Return all accumulated error msgs in a single string.
  void GetErrorMsg(std::string* msg) const;
  //std::string GetErrorString() const;
  std::string GetErrorMsg() const;

 private:
  struct ErrorDetail {
    std::vector<std::string> error_msgs;

    ErrorDetail(const std::string& msg): error_msgs(1, msg) {}
    ErrorDetail(const std::vector<std::string>& msgs): error_msgs(msgs) {}
  };

  ErrorDetail* error_detail_;
};

// some generally useful macros
#define RETURN_IF_ERROR(stmt) \
  do { Status status = (stmt); if (UNLIKELY(!status.ok())) return status; } while (false)

#define EXIT_IF_ERROR(stmt) \
  do { \
    Status status = (stmt); \
    if (UNLIKELY(!status.ok())) { \
      string msg; \
      status.GetErrorMsg(&msg); \
      cerr << msg; \
      exit(1); \
    } \
  } while (false)

}

#endif
