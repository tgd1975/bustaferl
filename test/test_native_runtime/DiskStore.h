#ifndef BUSTAFERL_NATIVE_RUNTIME_DISKSTORE_H
#define BUSTAFERL_NATIVE_RUNTIME_DISKSTORE_H

#include "../../src/hal/IPersistentStore.h"

#include <string>

namespace bustaferl::native_runtime {

// File-backed mirror of Esp32PersistentStore. Layout on disk:
//   [u32 magic][PersistedMeta][u32 rle_len][rle_bytes]
//   [u32 sched_magic][ScheduleSnapshot]
// All three sections sit in a single file (default
// .tmp/native-runtime/persist.bin) so "cold boot" is the equivalent of
// deleting the file — the smoke driver does that on startup unless asked
// to resume.
//
// Endian / packing note: this file is consumed only by the same host build
// that wrote it, so the structs go to disk by raw memcpy. No portability
// claim across architectures.
class DiskStore : public IPersistentStore {
public:
  explicit DiskStore(std::string path);

  PersistedMeta loadMeta() override;
  void saveMeta(const PersistedMeta &m) override;
  size_t loadFramebuffer(uint8_t *out, size_t cap) override;
  bool saveFramebuffer(const uint8_t *fb, size_t len) override;
  ScheduleSnapshot loadSchedule() override;
  void saveSchedule(const ScheduleSnapshot &s) override;

private:
  std::string path_;
};

} // namespace bustaferl::native_runtime

#endif
