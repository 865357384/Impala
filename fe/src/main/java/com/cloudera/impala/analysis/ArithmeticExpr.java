// Copyright (c) 2011 Cloudera, Inc. All rights reserved.

package com.cloudera.impala.analysis;

import com.cloudera.impala.catalog.PrimitiveType;
import com.cloudera.impala.common.AnalysisException;
import com.cloudera.impala.thrift.TExprNode;
import com.cloudera.impala.thrift.TExprNodeType;
import com.cloudera.impala.thrift.TExprOperator;
import com.google.common.base.Preconditions;

public class ArithmeticExpr extends Expr {
  enum Operator {
    MULTIPLY("*", TExprOperator.MULTIPLY),
    DIVIDE("/", TExprOperator.DIVIDE),
    MOD("%", TExprOperator.MOD),
    INT_DIVIDE("DIV", TExprOperator.INT_DIVIDE),
    PLUS("+", TExprOperator.PLUS),
    MINUS("-", TExprOperator.MINUS),
    BITAND("&", TExprOperator.BITAND),
    BITOR("|", TExprOperator.BITOR),
    BITXOR("^", TExprOperator.BITXOR),
    BITNOT("~", TExprOperator.BITNOT);

    private final String description;
    private final TExprOperator thriftOp;

    private Operator(String description, TExprOperator thriftOp) {
      this.description = description;
      this.thriftOp = thriftOp;
    }

    public boolean isBitwiseOperation() {
      return this == BITAND || this == BITOR || this == BITXOR || this == BITNOT;
    }

    @Override
    public String toString() {
      return description;
    }

    public TExprOperator toThrift() {
      return thriftOp;
    }
  }
  private final Operator op;

  public ArithmeticExpr(Operator op, Expr e1, Expr e2) {
    super();
    this.op = op;
    Preconditions.checkNotNull(e1);
    children.add(e1);
    Preconditions.checkArgument(op == Operator.BITNOT && e2 == null
        || op != Operator.BITNOT && e2 != null);
    if (e2 != null) {
      children.add(e2);
    }
  }

  @Override
  public String toSql() {
    if (children.size() == 1) {
      return op.toString() + " " + getChild(0).toSql();
    } else {
      Preconditions.checkState(children.size() == 2);
      return getChild(0).toSql() + " " + op.toString() + " " + getChild(1).toSql();
    }
  }

  @Override
  protected void toThrift(TExprNode msg) {
    msg.node_type = TExprNodeType.ARITHMETIC_EXPR;
    msg.op = op.toThrift();
  }

  @Override
  public boolean equals(Object obj) {
    if (!super.equals(obj)) {
      return false;
    }
    return ((ArithmeticExpr) obj).op == op;
  }

  @Override
  public void analyze(Analyzer analyzer) throws AnalysisException {
    super.analyze(analyzer);
    for (Expr child: children) {
      Expr operand = (Expr) child;
      if (!operand.type.isNumericType() && !operand.type.isStringType()) {
        throw new AnalysisException("Arithmetic operation requires " +
            "numeric or string operands: " + toSql());
      }
    }

    // bitnot is the only unary op, deal with it here
    if (op == Operator.BITNOT) {
      PrimitiveType childType = getChild(0).getType();
      if (!childType.isFixedPointType()) {
        throw new AnalysisException("Bitwise operations only allowed on fixed-point types: "
            + toSql());
      }
      type = childType;
      return;
    }

    PrimitiveType t1 = getChild(0).getType();
    PrimitiveType t2 = getChild(1).getType();  // only bitnot is unary

    switch (op) {
      case MULTIPLY:
      case PLUS:
      case MINUS:
        // numeric ops must be promoted to highest-resolution type
        // (otherwise we can't guarantee that a <op> b won't result in an overflow/underflow)
        type = PrimitiveType.getAssignmentCompatibleType(t1, t2).getMaxResolutionType();
        Preconditions.checkState(type.isValid());
        break;
      case MOD:
        type = PrimitiveType.getAssignmentCompatibleType(t1, t2);
        break;
      case DIVIDE:
        type = PrimitiveType.DOUBLE;
        break;
      case INT_DIVIDE:
      case BITAND:
      case BITOR:
      case BITXOR:
        if (t1.isFloatingPointType() || t2.isFloatingPointType()) {
          throw new AnalysisException(
              "Invalid floating point argument to operation " +
              op.toString() + ": " + this.toSql());
        }
        type =
          PrimitiveType.getAssignmentCompatibleType(t1, t2);
        // the result is always an integer
        Preconditions.checkState(type.isFixedPointType());
        break;
      default:
        // the programmer forgot to deal with a case
        Preconditions.checkState(false,
            "Unknown arithmetic operation " + op.toString() + " in: " + this.toSql());
        break;
    }

    // add operand casts
    Preconditions.checkState(type.isValid());
    if (t1 != type) {
      castChild(type, 0);
    }
    if (t2 != type) {
      castChild(type, 1);
    }
  }
}
