// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DB_IMPL_H_
#define STORAGE_LEVELDB_DB_DB_IMPL_H_

#include <deque>
#include <set>
#include "db/dbformat.h"
#include "db/log_writer.h"
#include "db/snapshot.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "port/port.h"
#include "port/thread_annotations.h"

///////////////meggie
#include "util/timer.h"
#include <map>
///////////////meggie

namespace leveldb {

class MemTable;
class TableCache;
class Version;
class VersionEdit;
class VersionSet;
//////////meggie
class ThreadPool;
class SplitCompaction;
class TSplitCompaction;
class Compaction;
class FileMetaData;
//////////meggie

class DBImpl : public DB {
 public:
  DBImpl(const Options& options, const std::string& dbname, 
        /////////////meggie
        const std::string& dbname_nvm = "/mnt/pmemdir/dbbench/"
        /////////////meggie
        );
  virtual ~DBImpl();

  // Implementations of the DB interface
  virtual Status Put(const WriteOptions&, const Slice& key, const Slice& value);
  virtual Status Delete(const WriteOptions&, const Slice& key);
  virtual Status Write(const WriteOptions& options, WriteBatch* updates);
  virtual Status Get(const ReadOptions& options,
                     const Slice& key,
                     std::string* value);
  virtual Iterator* NewIterator(const ReadOptions&);
  virtual const Snapshot* GetSnapshot();
  virtual void ReleaseSnapshot(const Snapshot* snapshot);
  virtual bool GetProperty(const Slice& property, std::string* value);
  virtual void GetApproximateSizes(const Range* range, int n, uint64_t* sizes);
  virtual void CompactRange(const Slice* begin, const Slice* end);

  // Extra methods (for testing) that are not in the public DB interface

  // Compact any files in the named level that overlap [*begin,*end]
  void TEST_CompactRange(int level, const Slice* begin, const Slice* end);

  // Force current memtable contents to be compacted.
  Status TEST_CompactMemTable();

  // Return an internal iterator over the current state of the database.
  // The keys of this iterator are internal keys (see format.h).
  // The returned iterator should be deleted when no longer needed.
  Iterator* TEST_NewInternalIterator();

  // Return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t TEST_MaxNextLevelOverlappingBytes();

  // Record a sample of bytes read at the specified internal key.
  // Samples are taken approximately once every config::kReadBytesPeriod
  // bytes.
  void RecordReadSample(Slice key);

 private:
  friend class DB;
  struct CompactionState;
  //////////////meggie
  struct PartnerCompactionState;
  struct TraditionalCompactionArgs;
  struct PartnerCompactionArgs;
  /////////////meggie
  struct Writer;

  Iterator* NewInternalIterator(const ReadOptions&,
                                SequenceNumber* latest_snapshot,
                                uint32_t* seed);

  Status NewDB();

  // Recover the descriptor from persistent storage.  May do a significant
  // amount of work to recover recently logged updates.  Any changes to
  // be made to the descriptor are added to *edit.
  Status Recover(VersionEdit* edit, bool* save_manifest)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void MaybeIgnoreError(Status* s) const;

  // Delete any unneeded files and stale in-memory entries.
  void DeleteObsoleteFiles() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Compact the in-memory write buffer to disk.  Switches to a new
  // log-file/memtable and writes a new descriptor iff successful.
  // Errors are recorded in bg_error_.
  void CompactMemTable() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status RecoverLogFile(uint64_t log_number, bool last_log, bool* save_manifest,
                        VersionEdit* edit, SequenceNumber* max_sequence)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status WriteLevel0Table(MemTable* mem, VersionEdit* edit, Version* base)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Status MakeRoomForWrite(bool force /* compact even if there is room? */)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  WriteBatch* BuildBatchGroup(Writer** last_writer)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void RecordBackgroundError(const Status& s);

  void MaybeScheduleCompaction() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  static void BGWork(void* db);
  void BackgroundCall();
  void BackgroundCompaction() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void CleanupCompaction(CompactionState* compact)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Status DoCompactionWork(CompactionState* compact)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ///////////////meggie
  Status DoSplitCompactionWork(Compaction* c);
  static void DoTraditionCompactionWork(void* args);
  static void DoPartnerCompactionWork(void* args);
  void DealWithTraditionCompaction(CompactionState* compact, 
                        TSplitCompaction* t_sptcompaction);
  void DealWithPartnerCompaction(PartnerCompactionState* compact, 
                            SplitCompaction* p_sptcompaction);
  void AddFileWithTraditionalCompaction(VersionEdit* edit, 
        std::vector<CompactionState*>& t_compactionstate_list);
  bool ValidAndInRange(Iterator* iter, InternalKey end, 
                bool containsend, ParsedInternalKey* ikey_victim);
  void UpdateFileWithPartnerCompaction(VersionEdit* edit,
            Compaction* c,
            std::vector<SplitCompaction*>& pcompaction_files, 
            std::vector<PartnerCompactionState*>& p_compactionstate_list);
  Status DealWithSingleCompaction(CompactionState* compact);
  Status OpenCompactionNVMTable(PartnerCompactionState* compact);
  void CleanupPartnerCompaction(PartnerCompactionState* compact);
  Status FinishCompactionNVMTable(PartnerCompactionState* compact,
                                        Iterator* input);
  std::map<uint64_t, MemTable*> partner_mapping_;
  ///////////////meggie

  Status OpenCompactionOutputFile(CompactionState* compact);
  Status FinishCompactionOutputFile(CompactionState* compact, Iterator* input);
  Status InstallCompactionResults(CompactionState* compact)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Constant after construction
  Env* const env_;
  const InternalKeyComparator internal_comparator_;
  const InternalFilterPolicy internal_filter_policy_;
  const Options options_;  // options_.comparator == &internal_comparator_
  const bool owns_info_log_;
  const bool owns_cache_;
  const std::string dbname_;
  //////////////meggie
  const std::string dbname_nvm_;
  //////////////meggie

  // table_cache_ provides its own synchronization
  TableCache* const table_cache_;

  // Lock over the persistent DB state.  Non-null iff successfully acquired.
  FileLock* db_lock_;

  // State below is protected by mutex_
  port::Mutex mutex_;
  port::AtomicPointer shutting_down_;
  port::CondVar background_work_finished_signal_ GUARDED_BY(mutex_);
  MemTable* mem_;
  MemTable* imm_ GUARDED_BY(mutex_);  // Memtable being compacted
  port::AtomicPointer has_imm_;       // So bg thread can detect non-null imm_
  WritableFile* logfile_;
  uint64_t logfile_number_ GUARDED_BY(mutex_);
  log::Writer* log_;
  uint32_t seed_ GUARDED_BY(mutex_);  // For sampling.

  // Queue of writers.
  std::deque<Writer*> writers_ GUARDED_BY(mutex_);
  WriteBatch* tmp_batch_ GUARDED_BY(mutex_);

  SnapshotList snapshots_ GUARDED_BY(mutex_);

  // Set of table files to protect from deletion because they are
  // part of ongoing compactions.
  std::set<uint64_t> pending_outputs_ GUARDED_BY(mutex_);

  // Has a background compaction been scheduled or is running?
  bool background_compaction_scheduled_ GUARDED_BY(mutex_);

  // Information for a manual compaction
  struct ManualCompaction {
    int level;
    bool done;
    const InternalKey* begin;   // null means beginning of key range
    const InternalKey* end;     // null means end of key range
    InternalKey tmp_storage;    // Used to keep track of compaction progress
  };
  ManualCompaction* manual_compaction_ GUARDED_BY(mutex_);

  VersionSet* const versions_;

  // Have we encountered a background error in paranoid mode?
  Status bg_error_ GUARDED_BY(mutex_);

  // Per level compaction stats.  stats_[level] stores the stats for
  // compactions that produced data for the specified "level".
  struct CompactionStats {
    int64_t micros;
    int64_t bytes_read;
    int64_t bytes_written;

    CompactionStats() : micros(0), bytes_read(0), bytes_written(0) { }

    void Add(const CompactionStats& c) {
      this->micros += c.micros;
      this->bytes_read += c.bytes_read;
      this->bytes_written += c.bytes_written;
    }
  };
  CompactionStats stats_[config::kNumLevels] GUARDED_BY(mutex_);

  /////////////meggie
  Timer* timer;
  ThreadPool* thpool_;
  virtual void PrintTimerAudit();
  /////////////meggie
  // No copying allowed
  DBImpl(const DBImpl&);
  void operator=(const DBImpl&);

  const Comparator* user_comparator() const {
    return internal_comparator_.user_comparator();
  }
};

// Sanitize db options.  The caller should delete result.info_log if
// it is not equal to src.info_log.
Options SanitizeOptions(const std::string& db,
                        const InternalKeyComparator* icmp,
                        const InternalFilterPolicy* ipolicy,
                        const Options& src,
                        /////////////meggie
                        const std::string& dbname_nvm = "/mnt/pmemdir/dbbench"
                        /////////////meggie
                        );

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_DB_IMPL_H_
