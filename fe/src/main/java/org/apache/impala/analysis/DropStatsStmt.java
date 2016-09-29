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

package org.apache.impala.analysis;

import org.apache.impala.authorization.Privilege;
import org.apache.impala.catalog.Table;
import org.apache.impala.common.AnalysisException;
import org.apache.impala.thrift.TDropStatsParams;
import org.apache.impala.thrift.TTableName;
import com.google.common.base.Preconditions;

/**
 * Represents both a DROP STATS statement, and the DROP INCREMENTAL STATS <tbl> PARTITION
 * <part_spec> variant.
 */
public class DropStatsStmt extends StatementBase {
  protected final TableName tableName_;

  // If non-null, only drop the statistics for a given partition
  PartitionSpec partitionSpec_ = null;

  // Set during analysis
  protected String dbName_;

  /**
   * Constructor for building the DROP TABLE/VIEW statement
   */
  public DropStatsStmt(TableName tableName) {
    this.tableName_ = tableName;
  }

  public DropStatsStmt(TableName tableName, PartitionSpec partSpec) {
    this.tableName_ = tableName;
    this.partitionSpec_ = partSpec;
  }

  @Override
  public String toSql() {
    StringBuilder sb = new StringBuilder("DROP ");
    if (partitionSpec_ == null) {
      sb.append(" STATS ");
      if (tableName_.getDb() != null) sb.append(tableName_.getDb() + ".");
      sb.append(tableName_.toSql());
    } else {
      sb.append(" INCREMENTAL STATS ");
      if (tableName_.getDb() != null) sb.append(tableName_.getDb() + ".");
      sb.append(tableName_.toSql());
      sb.append(partitionSpec_.toSql());
    }
    return sb.toString();
  }

  public TDropStatsParams toThrift() {
    TDropStatsParams params = new TDropStatsParams();
    params.setTable_name(new TTableName(getDb(), getTbl()));

    if (partitionSpec_ != null) {
      params.setPartition_spec(partitionSpec_.toThrift());
    }
    return params;
  }

  /**
   * Checks that the given table exists and the user has privileges
   * to drop stats on this table.
   */
  @Override
  public void analyze(Analyzer analyzer) throws AnalysisException {
    dbName_ = analyzer.getTargetDbName(tableName_);
    Table table = analyzer.getTable(tableName_, Privilege.ALTER);
    Preconditions.checkNotNull(table);
    if (partitionSpec_ != null) {
      partitionSpec_.setTableName(tableName_);
      partitionSpec_.setPrivilegeRequirement(Privilege.ALTER);
      partitionSpec_.setPartitionShouldExist();
      partitionSpec_.analyze(analyzer);
    }
  }

  /**
   * Can only be called after analysis. Returns the name of the database that
   * the target drop table resides in.
   */
  public String getDb() {
    Preconditions.checkNotNull(dbName_);
    return dbName_;
  }

  public String getTbl() { return tableName_.getTbl(); }
}
