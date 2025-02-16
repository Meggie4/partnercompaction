// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_VERSION_EDIT_H_
#define STORAGE_LEVELDB_DB_VERSION_EDIT_H_

#include <set>
#include <utility>
#include <vector>
#include "db/dbformat.h"

namespace leveldb {

class VersionSet;

///////////meggie
class MemTable;
struct Partner {
    uint64_t partner_number;
	uint64_t partner_size;
    InternalKey partner_smallest;
    InternalKey partner_largest;
    MemTable* nvmtable;
};
///////////meggie

struct FileMetaData {
  int refs;
  int allowed_seeks;          // Seeks allowed until compaction
  uint64_t number;
  uint64_t file_size;         // File size in bytes
  InternalKey smallest;       // Smallest internal key served by table
  InternalKey largest;        // Largest internal key served by table

  //////////////meggie 
  InternalKey origin_smallest;
  InternalKey origin_largest;
  std::vector<Partner> partners;
  bool nvm_partners;
  //////////////meggie
  FileMetaData() : refs(0), allowed_seeks(1 << 30), file_size(0), 
    /////////////meggie
    nvm_partners(false)
    /////////////meggie
    { }
  
};

class VersionEdit {
 public:
  VersionEdit() { Clear(); }
  ~VersionEdit() { }

  void Clear();

  void SetComparatorName(const Slice& name) {
    has_comparator_ = true;
    comparator_ = name.ToString();
  }
  void SetLogNumber(uint64_t num) {
    has_log_number_ = true;
    log_number_ = num;
  }
  void SetPrevLogNumber(uint64_t num) {
    has_prev_log_number_ = true;
    prev_log_number_ = num;
  }
  void SetNextFile(uint64_t num) {
    has_next_file_number_ = true;
    next_file_number_ = num;
  }
  void SetLastSequence(SequenceNumber seq) {
    has_last_sequence_ = true;
    last_sequence_ = seq;
  }
  void SetCompactPointer(int level, const InternalKey& key) {
    compact_pointers_.push_back(std::make_pair(level, key));
  }

  // Add the specified file at the specified number.
  // REQUIRES: This version has not been saved (see VersionSet::SaveTo)
  // REQUIRES: "smallest" and "largest" are smallest and largest keys in file
  void AddFile(int level, uint64_t file,
               uint64_t file_size,
               const InternalKey& smallest,
               const InternalKey& largest) {
    FileMetaData f;
    f.number = file;
    f.file_size = file_size;
    f.smallest = smallest;
    f.largest = largest;
	/////////////meggie
	f.origin_smallest = smallest;
    f.origin_largest = largest;
	/////////////meggie
    new_files_.push_back(std::make_pair(level, f));
  }
  
  ///////////////meggie
  void AddFile(int level, uint64_t file,
               uint64_t file_size,
               const InternalKey& smallest,
               const InternalKey& largest,
               const InternalKey& origin_smallest,
               const InternalKey& origin_largest, 
               std::vector<Partner>& partners
               ) {
    FileMetaData f;
    f.number = file;
    f.file_size = file_size;
    f.smallest = smallest;
    f.largest = largest;
	f.origin_smallest = origin_smallest;
    f.origin_largest = origin_largest;
    f.partners.assign(partners.begin(), partners.end());
    new_files_.push_back(std::make_pair(level, f));
  }
  ///////////////meggie

  // Delete the specified "file" from the specified "level".
  void DeleteFile(int level, uint64_t file) {
    deleted_files_.insert(std::make_pair(level, file));
  }

  ///////////meggie
  void UpdateFile(int level, uint64_t file, std::vector<Partner>& partners) {
     updated_files_.push_back(std::make_pair(level, std::make_pair(file, partners))); 
  }
  ///////////meggie

  void EncodeTo(std::string* dst) const;
  Status DecodeFrom(const Slice& src);

  std::string DebugString() const;

 private:
  friend class VersionSet;

  typedef std::set< std::pair<int, uint64_t> > DeletedFileSet;
  ///////////////meggie
  typedef std::vector< std::pair<int, std::pair< uint64_t, std::vector<Partner>>> > UpdatedFileSet;
  ///////////////meggie
  
  std::string comparator_;
  uint64_t log_number_;
  uint64_t prev_log_number_;
  uint64_t next_file_number_;
  SequenceNumber last_sequence_;
  bool has_comparator_;
  bool has_log_number_;
  bool has_prev_log_number_;
  bool has_next_file_number_;
  bool has_last_sequence_;

  std::vector< std::pair<int, InternalKey> > compact_pointers_;
  DeletedFileSet deleted_files_;
  std::vector< std::pair<int, FileMetaData> > new_files_;
  /////////meggie
  UpdatedFileSet updated_files_;
  /////////meggie
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_VERSION_EDIT_H_
