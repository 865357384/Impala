// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "exprs/conditional-functions.h"
#include "runtime/runtime-state.h"
#include "udf/udf.h"

#include "common/names.h"

using namespace impala;
using namespace impala_udf;

#define CONDITIONAL_CODEGEN_FN(expr_class) \
  Status expr_class::GetCodegendComputeFn(RuntimeState* state, llvm::Function** fn) { \
    return GetCodegendComputeFnWrapper(state, fn); \
  }

CONDITIONAL_CODEGEN_FN(IsNullExpr);
CONDITIONAL_CODEGEN_FN(NullIfExpr);
CONDITIONAL_CODEGEN_FN(IfExpr);
CONDITIONAL_CODEGEN_FN(CoalesceExpr);

