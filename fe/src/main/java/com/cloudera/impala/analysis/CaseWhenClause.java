// Copyright (c) 2012 Cloudera, Inc. All rights reserved.

package com.cloudera.impala.analysis;


/**
 * captures info of a single WHEN expr THEN expr clause.
 *
 */
class CaseWhenClause {
  private final Expr whenExpr;
  private final Expr thenExpr;

  public CaseWhenClause(Expr whenExpr, Expr thenExpr) {
    super();
    this.whenExpr = whenExpr;
    this.thenExpr = thenExpr;
  }

  public Expr getWhenExpr() {
    return whenExpr;
  }

  public Expr getThenExpr() {
    return thenExpr;
  }
}
