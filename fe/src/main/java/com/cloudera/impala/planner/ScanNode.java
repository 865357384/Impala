// Copyright (c) 2011 Cloudera, Inc. All rights reserved.

package com.cloudera.impala.planner;

import java.util.List;

import com.cloudera.impala.analysis.Expr;
import com.cloudera.impala.analysis.LiteralExpr;
import com.cloudera.impala.analysis.TupleDescriptor;
import com.cloudera.impala.thrift.TPlanNode;
import com.cloudera.impala.thrift.TPlanNodeType;
import com.cloudera.impala.thrift.TScanNode;
import com.google.common.base.Joiner;
import com.google.common.base.Objects;

/**
 * Scan of a single single table. Currently limited to full-table scans.
 * TODO: pass in range restrictions.
 */
public class ScanNode extends PlanNode {
  private final TupleDescriptor desc;
  private final List<String> filePaths;  // data files to scan
  public List<LiteralExpr> keyValues; // partition key values per file

  /**
   * Constructs node to scan given data files of table 'tbl'.
   * @param tbl
   * @param filePaths
   * @param keyValues
   */
  public ScanNode(TupleDescriptor desc, List<String> filePaths, List<LiteralExpr> keyValues) {
    super();
    this.desc = desc;
    this.filePaths = filePaths;
    this.keyValues = keyValues;
  }

  @Override
  protected String debugString() {
    return Objects.toStringHelper(this)
        .add("tid", desc.getId().asInt())
        .add("tblName", desc.getTable().getFullName())
        .add("filePaths", Joiner.on(", ").join(filePaths))
        .addValue(super.debugString())
        .toString();
  }

  @Override
  protected void toThrift(TPlanNode msg) {
    msg.node_type = TPlanNodeType.TEXT_SCAN_NODE;
    msg.scan_node = new TScanNode(desc.getId().asInt(), filePaths);
    if (!keyValues.isEmpty()) {
      msg.scan_node.setKey_values(Expr.treesToThrift(keyValues));
    }
  }

  @Override
  protected String getExplainString(String prefix) {
    StringBuilder output = new StringBuilder();
    output.append(prefix + "SCAN table=" + desc.getTable().getFullName() + "\n");
    output.append(prefix + "  PREDICATES: " + getExplainString(conjuncts) + "\n");
    output.append(prefix + "  FILES: " + Joiner.on(", ").join(filePaths));
    return output.toString();
  }
}
