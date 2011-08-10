// Copyright (c) 2011 Cloudera, Inc. All rights reserved.

package com.cloudera.impala.analysis;

import com.cloudera.impala.catalog.PrimitiveType;
import com.cloudera.impala.common.AnalysisException;
import com.cloudera.impala.common.NotImplementedException;
import com.google.common.base.Preconditions;

public abstract class LiteralExpr extends Expr {

  public static LiteralExpr create(String value, PrimitiveType type) throws AnalysisException {
    Preconditions.checkArgument(type != PrimitiveType.INVALID_TYPE);
    switch (type) {
      case BOOLEAN:
        return new BoolLiteral(value);
      case TINYINT:
      case SMALLINT:
      case INT:
      case BIGINT:
        return new IntLiteral(value);
      case FLOAT:
      case DOUBLE:
        return new FloatLiteral(value);
      case STRING:
        return new StringLiteral(value);
      case DATE:
      case DATETIME:
      case TIMESTAMP:
        throw new AnalysisException("DATE/DATETIME/TIMESTAMP literals not supported: " + value);
    }
    return null;
  }

  // Swaps the sign of numeric literals.
  // Throws for non-numeric literals.
  public void swapSign() throws NotImplementedException {
    throw new NotImplementedException("swapSign() only implemented for numeric literals");
  }
}
