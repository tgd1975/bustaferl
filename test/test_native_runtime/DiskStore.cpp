#include "DiskStore.h"

#include "../../src/render/rle.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace bustaferl::native_runtime {

namespace {

constexpr std::uint32_t MAGIC = 0xB05AFE71;
constexpr std::uint32_t SCHED_MAGIC = 0x5CEDB052;
// Same RLE hard-cap the Esp32PersistentStore uses; matches RTC-slow-memory
// budget so a host snapshot fits the device round-trip.
constexpr size_t RLE_CAP = 4096;

struct DiskPayload {
  std::uint32_t magic = 0;
  PersistedMeta meta{};
  std::uint32_t rle_len = 0;
  std::uint8_t rle[RLE_CAP] = {0};
  std::uint32_t sched_magic = 0;
  ScheduleSnapshot sched{};
};

bool readPayload(const std::string &path, DiskPayload &out) {
  std::FILE *f = std::fopen(path.c_str(), "rb");
  if (f == nullptr)
    return false;
  const size_t got = std::fread(&out, 1, sizeof(out), f);
  std::fclose(f);
  return got == sizeof(out);
}

bool writePayload(const std::string &path, const DiskPayload &payload) {
  std::FILE *f = std::fopen(path.c_str(), "wb");
  if (f == nullptr)
    return false;
  const size_t put = std::fwrite(&payload, 1, sizeof(payload), f);
  std::fclose(f);
  return put == sizeof(payload);
}

DiskPayload loadOrZero(const std::string &path) {
  DiskPayload p{};
  readPayload(path, p);
  return p;
}

} // namespace

DiskStore::DiskStore(std::string path) : path_(std::move(path)) {}

PersistedMeta DiskStore::loadMeta() {
  DiskPayload p = loadOrZero(path_);
  if (p.magic != MAGIC)
    return PersistedMeta{};
  return p.meta;
}

void DiskStore::saveMeta(const PersistedMeta &m) {
  DiskPayload p = loadOrZero(path_);
  p.magic = MAGIC;
  p.meta = m;
  writePayload(path_, p);
}

size_t DiskStore::loadFramebuffer(uint8_t *out, size_t cap) {
  DiskPayload p = loadOrZero(path_);
  if (p.magic != MAGIC || !p.meta.framebuffer_valid)
    return 0;
  if (p.rle_len == 0 || p.rle_len > RLE_CAP)
    return 0;
  return rleDecode(p.rle, p.rle_len, out, cap);
}

bool DiskStore::saveFramebuffer(const uint8_t *fb, size_t len) {
  DiskPayload p = loadOrZero(path_);
  const size_t n = rleEncode(fb, len, p.rle, RLE_CAP);
  if (n == 0) {
    p.rle_len = 0;
    p.meta.framebuffer_valid = false;
    p.magic = MAGIC;
    writePayload(path_, p);
    return false;
  }
  p.rle_len = static_cast<std::uint32_t>(n);
  p.meta.framebuffer_valid = true;
  p.magic = MAGIC;
  return writePayload(path_, p);
}

ScheduleSnapshot DiskStore::loadSchedule() {
  DiskPayload p = loadOrZero(path_);
  if (p.sched_magic != SCHED_MAGIC)
    return ScheduleSnapshot{};
  return p.sched;
}

void DiskStore::saveSchedule(const ScheduleSnapshot &s) {
  DiskPayload p = loadOrZero(path_);
  p.sched_magic = SCHED_MAGIC;
  p.sched = s;
  writePayload(path_, p);
}

} // namespace bustaferl::native_runtime
