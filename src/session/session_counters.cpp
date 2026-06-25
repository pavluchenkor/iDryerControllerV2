#include "session/session_counters.h"

#include <Arduino.h>
#include <EEPROM.h>

#include <algorithm>
#include <stddef.h>
#include <string.h>

#include "menu/menu_eeprom.h"

namespace {

struct SessionCounters {
  uint16_t drying;
  uint16_t storage;
  uint16_t profile;
};

struct SessionBackupRecord {
  uint32_t magic;
  uint16_t version;
  uint16_t drying;
  uint16_t storage;
  uint16_t profile;
  uint16_t reserved;
  uint32_t checksum;
};

constexpr uint32_t SESSION_BACKUP_MAGIC = 0x434F4F4Cu; // "SCNT"
constexpr uint16_t SESSION_BACKUP_VERSION = 1u;
constexpr size_t SESSION_BACKUP_GUARD_BYTES = 16u;

size_t backupOffset() {
  const size_t len = EEPROM.length();
  if (len < sizeof(SessionBackupRecord)) return 0u;
  return len - sizeof(SessionBackupRecord);
}

bool backupStorageAvailable() {
  const size_t len = EEPROM.length();
  if (len < sizeof(SessionBackupRecord)) return false;
  const size_t off = backupOffset();
  return off >= (size_t)EE_TOTAL_SIZE + SESSION_BACKUP_GUARD_BYTES;
}

uint32_t fnv1a32(const uint8_t *data, size_t len) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < len; ++i) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

uint32_t calcChecksum(const SessionBackupRecord &rec) { return fnv1a32(reinterpret_cast<const uint8_t *>(&rec), offsetof(SessionBackupRecord, checksum)); }

SessionCounters toCounters(const MenuState &menu) { return SessionCounters{menu.drying_session_count, menu.storage_session_count, menu.profile_session_count}; }

void applyCounters(MenuState &menu, const SessionCounters &counters) {
  menu.drying_session_count = counters.drying;
  menu.storage_session_count = counters.storage;
  menu.profile_session_count = counters.profile;
}

bool equals(const SessionCounters &a, const SessionCounters &b) { return a.drying == b.drying && a.storage == b.storage && a.profile == b.profile; }

bool countersCorrupted(const SessionCounters &counters) { return counters.drying == UINT16_MAX || counters.storage == UINT16_MAX || counters.profile == UINT16_MAX; }

template <typename T> bool putIfChangedNoCommit(uint16_t addr, const T &val) {
  T cur{};
  EEPROM.get(addr, cur);
  if (memcmp(&cur, &val, sizeof(T)) == 0) return false;
  EEPROM.put(addr, val);
  return true;
}

SessionBackupRecord makeBackupRecord(const SessionCounters &counters) {
  SessionBackupRecord rec{};
  rec.magic = SESSION_BACKUP_MAGIC;
  rec.version = SESSION_BACKUP_VERSION;
  rec.drying = counters.drying;
  rec.storage = counters.storage;
  rec.profile = counters.profile;
  rec.reserved = 0u;
  rec.checksum = calcChecksum(rec);
  return rec;
}

bool readBackup(SessionCounters &out) {
  if (!backupStorageAvailable()) return false;
  SessionBackupRecord rec{};
  EEPROM.get((uint16_t)backupOffset(), rec);
  if (rec.magic != SESSION_BACKUP_MAGIC || rec.version != SESSION_BACKUP_VERSION) return false;
  if (rec.checksum != calcChecksum(rec)) return false;
  out = SessionCounters{rec.drying, rec.storage, rec.profile};
  return true;
}

bool writeBackupNoCommit(const SessionCounters &counters) {
  if (!backupStorageAvailable()) return false;
  const SessionBackupRecord rec = makeBackupRecord(counters);
  return putIfChangedNoCommit((uint16_t)backupOffset(), rec);
}

bool writePrimaryNoCommit(const SessionCounters &counters) {
  bool dirty = false;
  dirty |= putIfChangedNoCommit<uint16_t>(EE_OFF_DRYING_SESSION_COUNT, counters.drying);
  dirty |= putIfChangedNoCommit<uint16_t>(EE_OFF_STORAGE_SESSION_COUNT, counters.storage);
  dirty |= putIfChangedNoCommit<uint16_t>(EE_OFF_PROFILE_SESSION_COUNT, counters.profile);
  return dirty;
}

void commitIfDirty(bool dirty) {
  if (dirty) EEPROM.commit();
}

uint16_t incrementSaturating(uint16_t value) { return value == UINT16_MAX ? UINT16_MAX : (uint16_t)(value + 1u); }

} // namespace

bool sessionCountersRestoreFromBackup(MenuState &menu, bool layoutValid) {
  const SessionCounters menuCounters = toCounters(menu);
  SessionCounters backupCounters{};
  const bool hasBackup = readBackup(backupCounters);

  if (!hasBackup) {
    if (layoutValid && !countersCorrupted(menuCounters)) {
      const bool dirty = writeBackupNoCommit(menuCounters);
      commitIfDirty(dirty);
    }
    return false;
  }

  SessionCounters merged = menuCounters;
  if (!layoutValid || countersCorrupted(menuCounters)) {
    merged = backupCounters;
  } else {
    merged.drying = std::max(menuCounters.drying, backupCounters.drying);
    merged.storage = std::max(menuCounters.storage, backupCounters.storage);
    merged.profile = std::max(menuCounters.profile, backupCounters.profile);
  }

  const bool changedMenu = !equals(merged, menuCounters);
  if (changedMenu) {
    applyCounters(menu, merged);
  }

  if (layoutValid) {
    bool dirty = false;
    dirty |= writePrimaryNoCommit(merged);
    dirty |= writeBackupNoCommit(merged);
    commitIfDirty(dirty);
  }

  return changedMenu;
}

uint16_t sessionCountersIncrement(MenuState &menu, SessionCounterKind kind) {
  SessionCounters counters = toCounters(menu);

  switch (kind) {
  case SessionCounterKind::Drying:
    counters.drying = incrementSaturating(counters.drying);
    break;
  case SessionCounterKind::Storage:
    counters.storage = incrementSaturating(counters.storage);
    break;
  case SessionCounterKind::Profile:
    counters.profile = incrementSaturating(counters.profile);
    break;
  default:
    break;
  }

  applyCounters(menu, counters);

  bool dirty = false;
  dirty |= writePrimaryNoCommit(counters);
  dirty |= writeBackupNoCommit(counters);
  commitIfDirty(dirty);

  switch (kind) {
  case SessionCounterKind::Drying:
    return counters.drying;
  case SessionCounterKind::Storage:
    return counters.storage;
  case SessionCounterKind::Profile:
    return counters.profile;
  default:
    return 0u;
  }
}
