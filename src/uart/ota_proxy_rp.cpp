// ota_proxy_rp implementation — см. ota_proxy_rp.h для контракта и flow.

#include "ota_proxy_rp.h"

#define LOG_LEVEL LOG_LEVEL_UART
#define LOG_TAG "OTA_RP"
#include "debug_log.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <PicoOTA.h>
// BearSSL SHA256 встроен в Earle Philhower core (как часть WiFi/bearssl).
// На RP2040 mbedtls недоступен, поэтому используем bearssl напрямую.
#include <bearssl/bearssl_hash.h>

#include <uart/uart_protocol.h>
#include "uart_manager.h"  // uartBridge extern для sendOtaChunkAck
#include "version.h"       // VERSION_MAJOR — ключ совместимости меню (solo vs paired)

namespace ota_rp {

namespace {

constexpr const char* FW_PATH = "/firmware.bin";
constexpr size_t      HEADER_SIZE = sizeof(idryer::UartOtaChunkForMcuPayload); // 12
constexpr size_t      MAX_CHUNK_DATA = 4096;
constexpr size_t      FRAG_BUF_SIZE = HEADER_SIZE + MAX_CHUNK_DATA;             // 4108

// Session state
bool     s_active   = false;   // между announce и финальным chunk
bool     s_verified = false;   // финальный SHA-verify прошёл, файл готов к PicoOTA.commit (Этап 5)
bool     s_shaInited = false;
bool     s_fsMounted = false;
uint32_t s_commandId = 0;
uint16_t s_totalChunks = 0;
uint16_t s_chunkSize = 0;
uint32_t s_totalSize = 0;
uint8_t  s_expectedSha[32] = {0};
uint8_t  s_targetMajor = 0;

uint16_t s_chunksReceived = 0;
uint32_t s_bytesReceived  = 0;

// Self-healing state (Этап 4).
bool     s_swapPendingOnIdle = false;
bool     s_checkRequestSent  = false;
uint32_t s_checkRequestAt    = 0;
constexpr uint32_t CHECK_REQUEST_TTL_MS = 60000;

// Commit-gate state (Этап 5).
bool     s_partnerReady       = false;
uint8_t  s_partnerTargetMajor = 0;
bool     s_commitInProgress   = false;
constexpr uint32_t COMMIT_REBOOT_DELAY_MS = 200;  // совпадает с ESP delay перед restart

// Frag-accumulator: собираем UART-фрагменты одного логического chunk'а.
uint8_t* s_fragBuf  = nullptr;
size_t   s_fragLen  = 0;
bool     s_fragFirst = false; // true пока ждём первый FRAGMENT текущего chunk'а

File     s_fwFile;
br_sha256_context s_shaCtx;

void shaStart() {
    // BearSSL не имеет init/free — это POD struct; br_sha256_init готовит его.
    br_sha256_init(&s_shaCtx);
    s_shaInited = true;
}

void shaFree() {
    // BearSSL: ресурсов вне struct нет.
    s_shaInited = false;
}

void sendAck(uint16_t chunkIdx, uint8_t status) {
    idryer::UartOtaChunkAckPayload ack{};
    ack.commandId = s_commandId;
    ack.chunkIdx  = chunkIdx;
    ack.status    = status;
    ack._pad      = 0;
    uartBridge.sendOtaChunkAck(ack);
}

void resetSession(bool removeFile) {
    if (s_fwFile) s_fwFile.close();
    if (removeFile && s_fsMounted && LittleFS.exists(FW_PATH)) {
        LittleFS.remove(FW_PATH);
    }
    shaFree();
    s_active   = false;
    s_verified = false;
    s_commandId = 0;
    s_totalChunks = 0;
    s_chunkSize = 0;
    s_totalSize = 0;
    memset(s_expectedSha, 0, sizeof(s_expectedSha));
    s_targetMajor = 0;
    s_chunksReceived = 0;
    s_bytesReceived = 0;
    s_fragLen   = 0;
    s_fragFirst = true;
}

bool ensureFsMounted() {
    if (s_fsMounted) return true;
    if (!LittleFS.begin()) {
        DEBUG_E("LittleFS.begin() failed");
        return false;
    }
    s_fsMounted = true;
    FSInfo info{};
    if (LittleFS.info(info)) {
        DEBUG_W("LittleFS mounted: totalBytes=%u usedBytes=%u blockSize=%u",
                (unsigned)info.totalBytes, (unsigned)info.usedBytes,
                (unsigned)info.blockSize);
    } else {
        DEBUG_W("LittleFS mounted (FSInfo unavailable)");
    }
    return true;
}

} // namespace

void begin() {
    if (!s_fragBuf) {
        s_fragBuf = (uint8_t*)malloc(FRAG_BUF_SIZE);
        if (!s_fragBuf) {
            DEBUG_E("ota_rp::begin: malloc(%u) failed", (unsigned)FRAG_BUF_SIZE);
            return;
        }
    }
    ensureFsMounted();
    resetSession(/*removeFile=*/false);
    DEBUG_I("ota_rp ready, fragBuf=%uB, fs=%s",
            (unsigned)FRAG_BUF_SIZE, s_fsMounted ? "ok" : "fail");
}

bool isVerified() { return s_verified; }
uint8_t targetMajor() { return s_targetMajor; }

void onUartFrameDropped() {
    // Сбрасываем накопитель — следующий принятый OtaChunkForMcu фрагмент
    // начнёт chunk заново. Сессионный state (s_active, sha-context, файл)
    // не трогаем: ESP переселлёт тот же chunk целиком (retry-loop), RP
    // встретит его как обычный новый chunk c тем же chunkIdx.
    if (s_fragLen > 0) {
        DEBUG_W("ota_rp: UART frame dropped, reset frag buf (was %u bytes)",
                (unsigned)s_fragLen);
        s_fragLen   = 0;
        s_fragFirst = true;
    }
}

bool hasVerifiedImageForMajor(uint8_t major) {
    return s_verified && s_targetMajor == major;
}

void scheduleSwapOnIdle() { s_swapPendingOnIdle = true; }
bool isSwapPendingOnIdle() { return s_swapPendingOnIdle; }

void markCheckRequestSent(uint32_t nowMs) {
    s_checkRequestSent = true;
    s_checkRequestAt   = nowMs;
}

bool isCheckRequestPending(uint32_t nowMs) {
    if (!s_checkRequestSent) return false;
    if (nowMs - s_checkRequestAt > CHECK_REQUEST_TTL_MS) {
        s_checkRequestSent = false;  // TTL вышел — следующий Hello снова попросит.
        return false;
    }
    return true;
}

void onPartnerStatus(const idryer::UartOtaStatusPayload& p) {
    // Любой пришедший OtaStatus просто фиксируется. Семантика «свежий статус
    // только после локальной готовности» обеспечивается в tickCommitGate:
    // там s_partnerReady сбрасывается в false пока не выполнены наши
    // локальные условия (verified + idle). Первый OtaStatus, который придёт
    // ПОСЛЕ того как gate откроется, и будет тем самым свежим триггером.
    s_partnerReady       = (p.espReady != 0);
    s_partnerTargetMajor = p.espTargetMajor;
}

bool partnerReady() { return s_partnerReady; }

uint8_t partnerTargetMajor() { return s_partnerTargetMajor; }

bool isCommitInProgress() { return s_commitInProgress; }

void tickCommitGate(uint32_t nowMs, bool allUnitsIdle) {
    // Раз в 30 секунд печатаем состояние OTA + LittleFS для диагностики.
    // Логгер на RP залочен в Warning, поэтому только DEBUG_W ловится в serial.
    static uint32_t s_lastDiagAt = 0;
    if (nowMs - s_lastDiagAt > 30000 || s_lastDiagAt == 0) {
        s_lastDiagAt = nowMs;
        FSInfo info{};
        bool ok = s_fsMounted && LittleFS.info(info);
        DEBUG_W("ota_rp diag: fs=%s total=%u used=%u verified=%d swap=%d cmt=%d idle=%d",
                ok ? "ok" : "fail",
                ok ? (unsigned)info.totalBytes : 0u,
                ok ? (unsigned)info.usedBytes  : 0u,
                (int)s_verified, (int)s_swapPendingOnIdle,
                (int)s_commitInProgress, (int)allUnitsIdle);
    }

    if (s_commitInProgress) return;  // уже инициирован — ждём reboot

    // Локальные условия не выполнены (не верифицированы или не в idle) →
    // сбрасываем партнёрский статус. Идея: пока мы сами не готовы, любые
    // ранее полученные OtaStatus устарели. После того как локальные условия
    // станут истинными, первый же НОВЫЙ OtaStatus(espReady=1) от ESP
    // взведёт s_partnerReady и tickCommitGate уйдёт в commit.
    if (!s_verified || !allUnitsIdle) {
        s_partnerReady = false;
        return;
    }

    // Условия для самостоятельного swap (self-healing, ESP уже на новой major).
    bool selfHealing = s_swapPendingOnIdle;  // s_verified уже проверен выше
    // Условия для paired commit (ESP параллельно обновляется и прислала
    // свежий espReady=1 после того как мы стали готовы).
    bool pairedReady = s_partnerReady && (s_partnerTargetMajor == s_targetMajor);
    // Solo commit для minor/patch: тот же major → меню EEPROM/NVS совместимо,
    // координация с ESP не нужна. Закрывает сценарий «обновляется только RP».
    // Major-bump (s_targetMajor != VERSION_MAJOR) сюда не попадает — он требует
    // paired/self-healing пути выше.
    bool minorSolo = (s_targetMajor == VERSION_MAJOR);

    if (!selfHealing && !pairedReady && !minorSolo) return;

    // Входим в commit-режим — гейтим команды.
    s_commitInProgress = true;
    DEBUG_I("Stage 5: enter commit (selfHealing=%d pairedReady=%d minorSolo=%d major=%u)",
            (int)selfHealing, (int)pairedReady, (int)minorSolo, s_targetMajor);

    if (pairedReady) {
        // Сигнал ESP перезагрузиться синхронно.
        idryer::UartOtaCommitNowPayload c{};
        c.targetMajor = s_targetMajor;
        c._pad = 0;
        uartBridge.sendOtaCommitNow(c);
        DEBUG_I("Stage 5: OtaCommitNow sent → ESP will restart");
    }

    // Дать ESP / TLS / MQTT момент закрыться + RP-собственный PubSubClient.
    delay(COMMIT_REBOOT_DELAY_MS);

    // PicoOTA: добавить /firmware.bin как команду на следующий boot.
    // PicoOTA создаст /otacommand.bin с подписью "Pico OTA"; OTA-loader при
    // следующем boot применит и сотрёт command-файл.
    picoOTA.begin();
    picoOTA.addFile(FW_PATH);
    if (!picoOTA.commit()) {
        DEBUG_E("Stage 5: picoOTA.commit() failed → file remains, reboot anyway");
    }
    DEBUG_I("Stage 5: rp2040.reboot() now");
    delay(50);
    rp2040.reboot();
}

void onAnnounce(const idryer::UartOtaAnnounceForMcuPayload& p,
                const idryer::UartFrameHeader&) {
    if (!s_fragBuf) {
        DEBUG_E("announce: fragBuf not allocated (begin() not called?)");
        return;
    }
    if (!ensureFsMounted()) {
        DEBUG_E("announce: LittleFS not available");
        return;
    }
    if (p.totalSize > 1024u * 1024u) {
        DEBUG_E("announce: totalSize=%u > 1MB partition", (unsigned)p.totalSize);
        return;
    }

    // Если предыдущая сессия не завершена — обнуляем (replay либо новый push).
    if (s_active) {
        DEBUG_W("announce: aborting previous session cmd=%08x", (unsigned)s_commandId);
        resetSession(/*removeFile=*/true);
    }

    s_commandId   = p.commandId;
    s_totalChunks = p.totalChunks;
    s_chunkSize   = p.chunkSize;
    s_totalSize   = p.totalSize;
    memcpy(s_expectedSha, p.expectedSha, sizeof(s_expectedSha));
    s_targetMajor = p.targetMajor;
    s_chunksReceived = 0;
    s_bytesReceived  = 0;
    s_fragLen   = 0;
    s_fragFirst = true;

    // Этап 4 self-healing: backend начал push → сбрасываем pending-флаг
    // (handleUartHello больше не будет переспрашивать).
    s_checkRequestSent = false;

    // Открыть файл в режиме truncate ("w").
    s_fwFile = LittleFS.open(FW_PATH, "w");
    if (!s_fwFile) {
        DEBUG_E("announce: LittleFS.open(%s, w) failed", FW_PATH);
        return;
    }

    shaStart();
    s_active = true;
    DEBUG_I("OTA session: cmd=%08x v_major=%u size=%u chunks=%ux%uB",
            (unsigned)s_commandId, s_targetMajor, (unsigned)s_totalSize,
            (unsigned)s_totalChunks, (unsigned)s_chunkSize);
}

void onChunkFragment(const uint8_t* payload, uint8_t length, uint8_t flags,
                     const idryer::UartFrameHeader&) {
    if (!s_active) {
        DEBUG_W("chunk fragment without active session — drop");
        return;
    }
    if (!s_fragBuf) return;

    // Накапливаем фрагмент в буфере.
    if (s_fragLen + length > FRAG_BUF_SIZE) {
        DEBUG_E("chunk overflow: %u + %u > %u", (unsigned)s_fragLen, length,
                (unsigned)FRAG_BUF_SIZE);
        // Сбрасываем накопитель — следующий FRAGMENT начнёт заново.
        s_fragLen   = 0;
        s_fragFirst = true;
        return;
    }
    memcpy(s_fragBuf + s_fragLen, payload, length);
    s_fragLen += length;
    s_fragFirst = false;

    bool last = (flags & idryer::UART_FLAG_LAST_FRAGMENT) != 0;
    if (!last) return;

    // Полный chunk собран. Парсим header.
    if (s_fragLen < HEADER_SIZE) {
        DEBUG_E("chunk too short: %u < %u header", (unsigned)s_fragLen, (unsigned)HEADER_SIZE);
        s_fragLen = 0; s_fragFirst = true;
        return;
    }
    idryer::UartOtaChunkForMcuPayload hdr{};
    memcpy(&hdr, s_fragBuf, HEADER_SIZE);

    if (hdr.commandId != s_commandId) {
        DEBUG_E("chunk cmdId mismatch: got %08x exp %08x",
                (unsigned)hdr.commandId, (unsigned)s_commandId);
        sendAck(hdr.chunkIdx, /*out_of_order*/ 3);
        s_fragLen = 0; s_fragFirst = true;
        return;
    }
    if (hdr.totalChunks != s_totalChunks) {
        DEBUG_E("chunk totalChunks mismatch: got %u exp %u",
                hdr.totalChunks, s_totalChunks);
        sendAck(hdr.chunkIdx, /*out_of_order*/ 3);
        s_fragLen = 0; s_fragFirst = true;
        return;
    }
    if (hdr.chunkIdx != s_chunksReceived) {
        // Дубликат — молча игнорируем (idempotency).
        if (hdr.chunkIdx < s_chunksReceived) {
            DEBUG_D("chunk %u duplicate (have %u), ignore", hdr.chunkIdx, s_chunksReceived);
            sendAck(hdr.chunkIdx, /*ok*/ 0);  // ack чтобы ESP не зависала
            s_fragLen = 0; s_fragFirst = true;
            return;
        }
        DEBUG_E("chunk out of order: got %u exp %u", hdr.chunkIdx, s_chunksReceived);
        sendAck(hdr.chunkIdx, /*out_of_order*/ 3);
        s_fragLen = 0; s_fragFirst = true;
        return;
    }
    size_t dataLen = (size_t)hdr.dataLength;
    if (HEADER_SIZE + dataLen != s_fragLen) {
        // Неполный chunk — потерян UART-фрагмент (CRC-дроп в bridge / overrun).
        // НЕ фатально: просим ESP переслать ТОТ ЖЕ chunk (status=4 retry).
        // chunksReceived не трогаем, файл не пишем — состояние чистое для повтора.
        DEBUG_W("chunk %u incomplete: dataLength=%u but received %u → retry",
                hdr.chunkIdx, (unsigned)dataLen, (unsigned)(s_fragLen - HEADER_SIZE));
        sendAck(hdr.chunkIdx, /*retry*/ 4);
        s_fragLen = 0; s_fragFirst = true;
        return;
    }
    if (dataLen > MAX_CHUNK_DATA) {
        sendAck(hdr.chunkIdx, /*flash_failed*/ 2);
        s_fragLen = 0; s_fragFirst = true;
        return;
    }

    // Запись data в LittleFS + SHA.
    const uint8_t* data = s_fragBuf + HEADER_SIZE;
    size_t written = s_fwFile.write(data, dataLen);
    if (written != dataLen) {
        DEBUG_E("chunk %u: LittleFS.write %u/%u", hdr.chunkIdx,
                (unsigned)written, (unsigned)dataLen);
        sendAck(hdr.chunkIdx, /*flash_failed*/ 2);
        resetSession(/*removeFile=*/true);
        return;
    }
    br_sha256_update(&s_shaCtx, data, dataLen);

    s_chunksReceived += 1;
    s_bytesReceived  += dataLen;

    // Финальный chunk → SHA-verify.
    if (s_chunksReceived == s_totalChunks) {
        s_fwFile.flush();
        s_fwFile.close();

        uint8_t actualSha[32];
        br_sha256_out(&s_shaCtx, actualSha);
        shaFree();

        if (memcmp(actualSha, s_expectedSha, 32) != 0) {
            DEBUG_E("FINAL SHA MISMATCH: file is corrupt, removing");
            LittleFS.remove(FW_PATH);
            sendAck(hdr.chunkIdx, /*sha_mismatch*/ 1);
            resetSession(/*removeFile=*/false);
            return;
        }

        DEBUG_I("OTA OK: %u bytes, sha verified, /firmware.bin ready for commit (Stage 5)",
                (unsigned)s_bytesReceived);
        sendAck(hdr.chunkIdx, /*ok*/ 0);
        s_verified = true;
        s_active   = false;
        s_fragLen  = 0;
        s_fragFirst = true;
        // s_commandId/expectedSha/totalSize оставляем для Этапа 5 commit-gate.
        return;
    }

    // Обычный chunk → ack(ok), подготовка к следующему.
    sendAck(hdr.chunkIdx, /*ok*/ 0);
    s_fragLen   = 0;
    s_fragFirst = true;
}

} // namespace ota_rp
