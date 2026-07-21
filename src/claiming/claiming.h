#pragma once

#include <Arduino.h>
#include <uart/uart_protocol.h>

struct ClaimUiSnapshot {
  bool visible = false;
  idryer::UartClaimStatus status = idryer::UartClaimStatus::Idle;
  char pin[9] = {};
  uint32_t expiresAt = 0;        // Unix time from ESP
  uint32_t remainingSeconds = 0; // Computed at snapshot time
  bool success = false;
  char deviceId[37] = {};
};

// Сброс состояния и скрытие экрана
void claim_reset();

// Запуск локального ожидания claiming (после отправки ClaimStart)
void claim_begin(uint32_t nowMs);

// Обновление состояния по UART событиям
void claim_on_status(const idryer::UartClaimStatusPayload &payload, uint32_t nowMs);
void claim_on_complete(const idryer::UartClaimCompletePayload &payload, uint32_t nowMs);

// UI helpers
bool claim_is_visible();
ClaimUiSnapshot claim_get_snapshot(uint32_t nowMs);
void claim_hide();

// Автоскрытие overlay после успешной привязки (Claimed) по таймауту.
// Вызывать каждый кадр из рендера — иначе claim-экран висит вечно.
void claim_tick(uint32_t nowMs);
