#include "claiming.h"
#include <string.h>

namespace {
struct ClaimState {
  bool visible = false;
  idryer::UartClaimStatus status = idryer::UartClaimStatus::Idle;
  char pin[9] = {};
  uint32_t expiresAt = 0;
  uint32_t remainingSeconds = 0;
  uint32_t lastUpdateMs = 0;
  uint32_t claimedAtMs = 0;
  bool success = false;
  char deviceId[37] = {};
};

ClaimState g_claim;

// Сколько показывать "CLAIMED" перед авто-возвратом в меню.
constexpr uint32_t kClaimedHideMs = 3000;

uint32_t computeRemaining(const ClaimState &st, uint32_t nowMs) {
  if (st.remainingSeconds == 0 || st.lastUpdateMs == 0) return 0;
  uint32_t elapsedSec = (nowMs - st.lastUpdateMs) / 1000;
  if (elapsedSec >= st.remainingSeconds) return 0;
  return st.remainingSeconds - elapsedSec;
}
} // namespace

void claim_reset() { g_claim = ClaimState{}; }

void claim_begin(uint32_t nowMs) {
  g_claim = ClaimState{};
  g_claim.visible = true;
  g_claim.status = idryer::UartClaimStatus::Provisioning;
  g_claim.lastUpdateMs = nowMs;
}

void claim_on_status(const idryer::UartClaimStatusPayload &payload, uint32_t nowMs) {
  const bool becameClaimed = payload.status == idryer::UartClaimStatus::Claimed &&
                             g_claim.status != idryer::UartClaimStatus::Claimed;
  g_claim.visible = (payload.status != idryer::UartClaimStatus::Idle);
  g_claim.status = payload.status;
  if (becameClaimed) g_claim.claimedAtMs = nowMs;
  strncpy(g_claim.pin, payload.pin, sizeof(g_claim.pin) - 1);
  g_claim.pin[sizeof(g_claim.pin) - 1] = '\0';
  g_claim.expiresAt = payload.expiresAt;
  g_claim.remainingSeconds = payload.remainingSeconds;
  g_claim.lastUpdateMs = nowMs;
}

void claim_on_complete(const idryer::UartClaimCompletePayload &payload, uint32_t nowMs) {
  g_claim.visible = true;
  g_claim.status = payload.success ? idryer::UartClaimStatus::Claimed : idryer::UartClaimStatus::Error;
  g_claim.success = payload.success != 0;
  strncpy(g_claim.deviceId, payload.deviceId, sizeof(g_claim.deviceId) - 1);
  g_claim.deviceId[sizeof(g_claim.deviceId) - 1] = '\0';
  g_claim.remainingSeconds = 0;
  g_claim.lastUpdateMs = nowMs;
  if (payload.success) g_claim.claimedAtMs = nowMs;
}

bool claim_is_visible() { return g_claim.visible; }

ClaimUiSnapshot claim_get_snapshot(uint32_t nowMs) {
  ClaimUiSnapshot snap{};
  snap.visible = g_claim.visible;
  snap.status = g_claim.status;
  strncpy(snap.pin, g_claim.pin, sizeof(snap.pin) - 1);
  snap.pin[sizeof(snap.pin) - 1] = '\0';
  snap.expiresAt = g_claim.expiresAt;
  snap.remainingSeconds = computeRemaining(g_claim, nowMs);
  snap.success = g_claim.success;
  strncpy(snap.deviceId, g_claim.deviceId, sizeof(snap.deviceId) - 1);
  snap.deviceId[sizeof(snap.deviceId) - 1] = '\0';
  return snap;
}

void claim_hide() { g_claim.visible = false; }

void claim_tick(uint32_t nowMs) {
  // После успешной привязки показываем "CLAIMED" kClaimedHideMs, затем прячем
  // overlay и возвращаемся в меню. Иначе claim-экран висит вечно — claim_hide()
  // больше никто не вызывает.
  if (g_claim.visible &&
      g_claim.status == idryer::UartClaimStatus::Claimed &&
      g_claim.claimedAtMs != 0 &&
      (nowMs - g_claim.claimedAtMs) >= kClaimedHideMs) {
    g_claim.visible = false;
  }
}
