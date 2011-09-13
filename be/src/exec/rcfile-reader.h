// Copyright (c) 2011 Cloudera, Inc. All rights reserved.

#ifndef IMPALA_EXEC_RCFILE_READER_H_
#define IMPALA_EXEC_RCFILE_READER_H_

#include <vector>
#include <memory>
#include <string>
#include <cstring>
#include <iostream>

#include <stdint.h>
#include <hdfs.h>

#include "common/status.h"
#include "exec/serde-utils.h"

namespace impala {

// org.apache.hadoop.hive.ql.io.RCFile is the original RCFile implementation
// and should be viewed as the canonical definition of this format. If
// anything is unclear in this file you should consult the code in
// org.apache.hadoop.hive.ql.io.RCFile.
//
// The following is a pseudo-BNF grammar for RCFile. Comments are prefixed
// with dashes:
//
// rcfile ::=
//   <file-header>
//   <rcfile-rowgroup>+
// 
// file-header ::=
//   <file-version-header>
//   <file-key-class-name>
//   <file-value-class-name>
//   <file-is-compressed>
//   <file-is-block-compressed>
//   [<file-compression-codec-class>]
//   <file-header-metadata>
//   <file-sync-field>
//
// -- The normative RCFile implementation included with Hive is actually
// -- based on a modified version of Hadoop's SequenceFile code. Some
// -- things which should have been modified were not, including the code
// -- that writes out the file version header. Consequently, RCFile and
// -- SequenceFile actually share the same version header.
//
// file-version-header ::= Byte[4] {'S', 'E', 'Q', 6}
//
// -- The name of the Java class responsible for reading the key buffer
// -- component of the rowgroup.
//
// file-key-class-name ::=
//   Text {"org.apache.hadoop.hive.ql.io.RCFile$KeyBuffer"}
//
// -- The name of the Java class responsible for reading the value buffer
// -- component of the rowgroup.
//
// file-value-class-name ::=
//   Text {"org.apache.hadoop.hive.ql.io.RCFile$ValueBuffer"}
//
// -- Boolean variable indicating whether or not the file uses compression
// -- for the key and column buffer sections.
//
// file-is-compressed ::= Byte[1]
//
// -- A boolean field indicating whether or not the file is block compressed.
// -- This field is *always* false. According to comments in the original
// -- RCFile implementation this field was retained for backwards
// -- compatability with the SequenceFile format.
//
// file-is-block-compressed ::= Byte[1] {false}
//
// -- The Java class name of the compression codec iff <file-is-compressed>
// -- is true. The named class must implement
// -- org.apache.hadoop.io.compress.CompressionCodec.
// -- The expected value is org.apache.hadoop.io.compress.GzipCodec.
//
// file-compression-codec-class ::= Text
//
// -- A collection of key-value pairs defining metadata values for the
// -- file. The Map is serialized using standard JDK serialization, i.e.
// -- an Int corresponding to the number of key-value pairs, followed by
// -- Text key and value pairs. The following metadata properties are
// -- mandatory for all RCFiles:
// --
// -- hive.io.rcfile.column.number: the number of columns in the RCFile
//
// file-header-metadata ::= Map<Text, Text>
//
// -- A 16 byte marker that is generated by the writer. This marker appears
// -- at regular intervals at the beginning of rowgroup-headers, and is
// -- intended to enable readers to skip over corrupted rowgroups.
//
// file-sync-hash ::= Byte[16]
//
// -- Each row group is split into three sections: a header, a set of
// -- key buffers, and a set of column buffers. The header section includes
// -- an optional sync hash, information about the size of the row group, and
// -- the total number of rows in the row group. Each key buffer
// -- consists of run-length encoding data which is used to decode
// -- the length and offsets of individual fields in the corresponding column
// -- buffer.
//
// rcfile-rowgroup ::=
//   <rowgroup-header>
//   <rowgroup-key-buffers>
//   <rowgroup-column-buffers>
// 
// rowgroup-header ::=
//   [<rowgroup-sync-marker>, <rowgroup-sync-hash>]
//   <rowgroup-record-length>
//   <rowgroup-key-length>
//   <rowgroup-compressed-key-length>
//   <rowgroup-num-rows>
//
// -- An integer (always -1) signaling the beginning of a sync-hash
// -- field.
//
// rowgroup-sync-marker ::= Int
//
// -- A 16 byte sync field. This must match the <file-sync-hash> value read
// -- in the file header.
//
// rowgroup-sync-hash ::= Byte[16]
//
// -- The record-length is the sum of the number of bytes used to store
// -- the key and column parts, i.e. it is the total length of the current
// -- rowgroup.
//
// rowgroup-record-length ::= Int
//
// -- Total length in bytes of the rowgroup's key sections.
//
// rowgroup-key-length ::= Int
//
// -- Total uncompressed length in bytes of the rowgroup's key sections.
//
// rowgroup-compressed-key-length ::= Int
//
// -- Number of rows in the current rowgroup.
//
// rowgroup-num-rows ::= VInt
//
// -- One or more column key buffers corresponding to each column
// -- in the RCFile.
//
// rowgroup-key-buffers ::= <rowgroup-key-buffer>+
//
// -- Data in each column buffer is stored using a run-length
// -- encoding scheme that is intended to reduce the cost of
// -- repeated column field values. This mechanism is described
// -- in more detail in the following entries.
//
// rowgroup-key-buffer ::=
//   <column-buffer-length>
//   <column-buffer-uncompressed-length>
//   <column-key-buffer-length>
//   <column-key-buffer>
//
// -- The serialized length on disk of the corresponding column buffer.
//
// column-buffer-length ::= VInt
//
// -- The uncompressed length of the corresponding column buffer. This
// -- is equivalent to column-buffer-length if the RCFile is not compressed.
//
// column-buffer-uncompressed-length ::= VInt
//
// -- The length in bytes of the current column key buffer
//
// column-key-buffer-length ::= VInt
//
// -- The column-key-buffer contains a sequence of serialized VInt values
// -- corresponding to the byte lengths of the serialized column fields
// -- in the corresponding rowgroup-column-buffer. For example, consider
// -- an integer column that contains the consecutive values 1, 2, 3, 44.
// -- The RCFile format stores these values as strings in the column buffer,
// -- e.g. "12344". The length of each column field is recorded in
// -- the column-key-buffer as a sequence of VInts: 1,1,1,2. However,
// -- if the same length occurs repeatedly, then we replace repeated
// -- run lengths with the complement (i.e. negative) of the number of
// -- repetitions, so 1,1,1,2 becomes 1,~2,2.
//
// column-key-buffer ::= Byte[column-key-buffer-length]
// 
// rowgroup-column-buffers ::= <rowgroup-value-buffer>+
//
// -- RCFile stores all column data as strings regardless of the
// -- underlying column type. The strings are neither length-prefixed or
// -- null-terminated, and decoding them into individual fields requires
// -- the use of the run-length information contained in the corresponding
// -- column-key-buffer.
//
// rowgroup-column-buffer ::= Byte[column-buffer-length]
// 
// Byte ::= An eight-bit byte
// 
// VInt ::= Variable length integer. The high-order bit of each byte
// indicates whether more bytes remain to be read. The low-order seven
// bits are appended as increasingly more significant bits in the
// resulting integer value.
// 
// Int ::= A four-byte integer in big-endian format.
// 
// Text ::= VInt, Chars (Length prefixed UTF-8 characters)


// Container class for column data in an RCFile RowGroup
// Provides methods for deserializing the RowGroup from
// an HDFS file, along with accessor methods.
//
// TODO: Move the methods that read data from HDFS into
// the RCFileReader class, and make RCFileRowGroup a simple
// container class.
class RCFileRowGroup {
public:
  // C'tor for RCFileRowGroup object. Column i is read iff
  // column_read_mask[i-1] == true.
  RCFileRowGroup(const std::vector<bool>& column_read_mask);

  // The sync hash field to verify rowgroup sync fields against.
  void SetSyncHash(const std::vector<char>* sync_hash);

  // Read the next rowgroup from file
  Status ReadNext(hdfsFS fs, hdfsFile file);

  // Reset the underlying key and value buffers in this rowgroup object
  void Reset();

  // Number of rows left to read using NextRow() in this rowgroup object
  int NumRowsRemaining();

  // Increment the internal cursor to point to the next row in this rowgroup
  // object. Fields in this row can subsequently be accessed using
  // GetFieldPtr() and GetFieldLength()
  bool NextRow();

  // Get the string length of the current column field
  int GetFieldLength(int col_id);

  // Get a pointer to the current column field
  const char* GetFieldPtr(int col_id);

  int num_rows() const { return num_rows_; }
  int num_cols() const { return num_cols_; }
  int row_idx() const { return row_pos_; }

private:
  // Column i is read iff column_read_mask_[i-1] == true.
  const std::vector<bool>& column_read_mask_;

  // Pointer to the sync hash read out of the file header by RCFileReader
  const std::vector<char>* sync_hash_;

  // number of columns in this rowgroup object
  int num_cols_;

  // whether or not this rowgroup is compressed
  bool is_compressed_;

  // number of rows in this rowgroup object
  int num_rows_;

  // Current row position in this rowgroup.
  // This value is incremented each time NextRow() is called.
  int row_pos_;

  // Combined size of the row group's key buffers and column buffers.
  // Read from the row group header.
  int record_length_;

  // Size of the row group's key buffers.
  // Read from the row group header.
  int key_length_;

  // Compressed size of the row group's key buffers.
  // Read from the row group header.
  int compressed_key_length_;

  // Row Group Key Buffer data, indexed by column number
  std::vector<int> col_buf_len_;
  std::vector<int> col_buf_uncompressed_len_;
  std::vector<std::vector<char> > col_key_bufs_;

  // Current position in the key buffer, by column
  std::vector<int> key_buf_pos_;
  
  // RLE: Length of the current field, by column
  std::vector<int> cur_field_length_;

  // RLE: Repetition count of the current field, by column
  std::vector<int> cur_field_length_rep_;

  // Column data buffers, by column
  std::vector<std::vector<char> > col_bufs_;

  // Column buffer byte offset, by column
  std::vector<int> col_buf_pos_;

  // Read the rowgroup header
  Status ReadHeader(hdfsFS fs, hdfsFile file);

  // Read and validate the rowgroup sync field
  Status ReadSync(hdfsFS fs, hdfsFile file);

  // Read the rowgroup key buffers
  Status ReadKeyBuffers(hdfsFS fs, hdfsFile file);

  // Read the current key buffer
  Status ReadCurrentKeyBuffer(hdfsFS fs, hdfsFile file, int col_idx, bool skip_col_data);

  // Read the rowgroup column buffers
  Status ReadColumnBuffers(hdfsFS fs, hdfsFile file);

  // Read the current rowgroup buffer
  Status ReadCurrentColumnBuffer(hdfsFS fs, hdfsFile file, int col_idx, bool skip_col_data);

  // Look at the next field in the specified column buffer
  void NextField(int col_idx);
};

// RCFileReader reads RCFileRowGroup objects from a
// set of RCFile files located in HDFS.
class RCFileReader {
public:
  // C'tor for RCFileReader. Column i is read iff
  // column_read_mask[i-1] == true.
  RCFileReader(hdfsFS hdfs_fs, std::vector<std::string> files,
               const std::vector<bool>& column_read_mask);

  ~RCFileReader();

  // Creates and initializes an empty RCFileRowGroup object
  // for use with this RCFileReader instance.
  RCFileRowGroup* NewRCFileRowGroup();
  
  // Read the next RowGroup out of the current file and copy
  // the column values into the supplied RowGroup object.
  // If no more row groups are available for reading, then the
  // resulting RowGroup object will have zero rows.
  Status ReadNextRowGroup(RCFileRowGroup* row_group);

  // Get the index of the file that is currently being read.
  // Returns -1 if the first file on the input list has not yet
  // been opened.
  int file_idx() const { return cur_file_idx_; }

  // Get the index of the current row group in the current file.
  int row_group_idx() const { return row_group_idx_; }

  // Get the number of columns in the current file.
  int num_cols() const { return num_cols_; }

  friend class RCFileRowGroup;
  
private:
  // Sync indicator
  const static int SYNC_MARKER = -1;

  // Size of the sync hash field
  const static int SYNC_HASH_SIZE = 16;

  // The key class name located in the RCFile Header.
  // This is always "org.apache.hadoop.hive.ql.io.RCFile$KeyBuffer"
  static const char* const RCFILE_KEY_CLASS_NAME;

  // The value class name located in the RCFile Header.
  // This is always "org.apache.hadoop.hive.ql.io.RCFile$ValueBuffer"
  static const char* const RCFILE_VALUE_CLASS_NAME;

  // RCFile metadata key for determining the number of columns
  // present in the RCFile: "hive.io.rcfile.column.number"
  static const char* const RCFILE_METADATA_KEY_NUM_COLS;

  // The four byte RCFile version header present at the beginning of every
  // RCFile file: {'S', 'E', 'Q', 6}
  // NOTE: This is a defect in the original RCFile implementation since
  // this is actually the same version header used by SequenceFile.
  static const uint8_t RCFILE_VERSION_HEADER[4];

  // Connection to hdfs
  hdfsFS fs_;

  // List of HDFS paths to read.
  const std::vector<std::string> files_;

  // The sync hash read in from the file header
  std::vector<char> sync_;
  
  // The column read mask. Column i is read iff column_read_mask_[i] == true.
  const std::vector<bool>& column_read_mask_;

  // Compression codec specified in the RCFile Header.
  std::vector<char> compression_codec_;
  
  // current file index in files_
  int cur_file_idx_;

  // index of the current row group in the current file
  int row_group_idx_;

  // current file
  hdfsFile file_;

  // length of the current file
  int file_len_;

  // true if the current RCFile is compressed
  bool is_compressed_;

  // number of columns in the RCFile
  int num_cols_;

  // the current row group object
  RCFileRowGroup* row_group_;

  // read the current RCFile header
  Status ReadFileHeader();

  // read the RCFile Header Metadata section in the current file
  Status ReadFileHeaderMetadata();

  // read and validate a RowGroup sync field
  Status ReadSync();

  // get the position in the current file
  long GetPosition();

  // open and read the header of the next RCFile on the input list
  Status OpenNextFile();

  // get the file length of the current RCFile
  Status GetFileLength(int* length);
};

} // namespace impala

#endif // IMPALA_EXEC_RCFILE_READER_H_
