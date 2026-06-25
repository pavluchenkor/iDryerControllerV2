// DRYER paired OTA — RP-сторона: приём прошивки через UART-мост от ESP.
//
// Поток (см. ___OTA_MQTT_DESIGN.md):
//   1) ESP → OtaAnnounceForMcu: открываем /firmware.bin в LittleFS, init SHA256.
//   2) ESP → OtaChunkForMcu (фрагментированно FRAGMENT/LAST_FRAGMENT):
//      собираем фрагменты в буфер, при LAST_FRAGMENT разбираем 12-байт header,
//      пишем data в файл, обновляем SHA256, шлём OtaChunkAck.
//   3) На последнем chunk'е (chunksReceived == totalChunks):
//      finalize SHA256, сравниваем с expectedSha. Если ok — verified_=true,
//      шлём OtaChunkAck(ok); если нет — удаляем файл, OtaChunkAck(sha_fail).
//   4) PicoOTA.commit() + rp2040.reboot() — НЕ здесь, а на Этапе 5 (idle-gate
//      + OtaCommitNow от RP к ESP).
//
// Регистрируется из uart_manager.cpp initUartBridge() через setOtaAnnounceForMcuHandler
// + setOtaChunkForMcuHandler.

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace idryer {
struct UartOtaAnnounceForMcuPayload;
struct UartOtaStatusPayload;
struct UartFrameHeader;
}

namespace ota_rp {

// Аллокация переиспользуемого frag-буфера (4108 байт) + сброс состояния.
// Вызывать в setup() ОДИН раз.
void begin();

void onAnnounce(const idryer::UartOtaAnnounceForMcuPayload& p,
                const idryer::UartFrameHeader& hdr);

void onChunkFragment(const uint8_t* payload, uint8_t length, uint8_t flags,
                     const idryer::UartFrameHeader& hdr);

// Сбросить frag-accumulator. Зовётся из handleUartError при CrcMismatch /
// InvalidPayload: битый фрагмент мог быть частью OtaChunkForMcu, и если не
// сбросить накопитель, следующий валидный FRAGMENT допишется к битому
// буферу (header съедет, commandId/chunkIdx будут мусорные). ESP сам
// повторит chunk целиком по timeout ack (retry в pushChunkToRp).
void onUartFrameDropped();

// Финальный SHA-verify прошёл (Этап 5 commit-gate проверяет это).
bool isVerified();

// Major из последнего announce — для Этапа 5/6 (commit + post-handshake).
uint8_t targetMajor();

// Self-healing API (Этап 4):

// true когда у нас в LittleFS лежит верифицированная прошивка с заданным major.
bool hasVerifiedImageForMajor(uint8_t major);

// Этап 5: «применить swap при первой же возможности (idle)».
void scheduleSwapOnIdle();
bool isSwapPendingOnIdle();

// Pending-state для OtaCheckRequest, TTL 60c после markRequestSent.
// Сбрасывается автоматически при получении OtaAnnounceForMcu (backend взял в работу).
void markCheckRequestSent(uint32_t nowMs);
bool isCheckRequestPending(uint32_t nowMs);

// Этап 5 — periodic OtaStatus от ESP: обновляет partner-state.
void onPartnerStatus(const idryer::UartOtaStatusPayload& p);
bool partnerReady();
uint8_t partnerTargetMajor();

// Gate для handleUartCommand: пока commit инициирован — все Start/Stop
// дропаются (window между OtaCommitNow и rp2040.reboot).
bool isCommitInProgress();

// Главный tick Этапа 5 — продукт зовёт из main loop с текущим временем и
// флагом allUnitsIdle. Внутри проверяет условия (self-healing или paired)
// и инициирует commit (PicoOTA + rp2040.reboot).
// allUnitsIdle: все контроллеры в DryerMode::Idle.
void tickCommitGate(uint32_t nowMs, bool allUnitsIdle);

} // namespace ota_rp
