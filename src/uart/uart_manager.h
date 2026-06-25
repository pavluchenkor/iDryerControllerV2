#pragma once

#include "../configuration.h"
#include "../error/error_bus.h"
#include "../menu/menu_state.h"
#include "claiming/claiming.h"
#include <WorkTimeCounter.h>
#include <uart/uart_bridge.h>

// Глобальный объект UartBridge
extern idryer::UartBridge uartBridge;

// Флаг готовности LINK (ESP) — устанавливается при HelloAck/Heartbeat
bool uartLinkReady();
bool uartLinkInfoAvailable();
uint32_t uartLinkIp();
const char *uartLinkSsid();

// =============================================================================
// ОЧЕРЕДЬ ОТЛОЖЕННЫХ КОМАНД ДЛЯ MAIN LOOP
// =============================================================================

#define UART_CMD_QUEUE_SIZE 4

// Структура для отложенных команд из UART
struct PendingUartCommand {
  idryer::UartCmdCode code;
  uint8_t unitId;
  uint8_t reserved[2]; // выравнивание
  uint32_t arg0;
  uint32_t arg1;

  PendingUartCommand() : code((idryer::UartCmdCode)0), unitId(0xFF), arg0(0), arg1(0) { reserved[0] = reserved[1] = 0; }

  bool isEmpty() const { return unitId == 0xFF; }
  void clear() {
    unitId = 0xFF;
    code = (idryer::UartCmdCode)0;
  }
};

/**
 * @brief Поставить команду в очередь для обработки в main loop
 * @return true если команда добавлена, false если очередь заполнена
 */
bool enqueueUartCommand(idryer::UartCmdCode code, uint8_t unitId, uint32_t arg0 = 0, uint32_t arg1 = 0);

/**
 * @brief Извлечь команду из очереди
 * @param out Указатель на структуру для записи команды
 * @return true если команда извлечена, false если очередь пуста
 */
bool dequeueUartCommand(PendingUartCommand *out);

// =============================================================================
// ИНИЦИАЛИЗАЦИЯ
// =============================================================================

/**
 * @brief Инициализирует UART мост и регистрирует обработчики
 * @param serial Указатель на ISerial объект (ArduinoSerial)
 * @param baudRate Скорость передачи данных
 */
void initUartBridge(idryer::hal::ISerial *serial, uint32_t baudRate);

// =============================================================================
// ФОРМИРОВАНИЕ И ОТПРАВКА HELLO
// =============================================================================

/**
 * @brief Формирует HelloPayload на основе текущей конфигурации устройства
 * @param menu Ссылка на MenuState с конфигурацией
 * @param wtc Ссылка на WorkTimeCounter для счётчика наработки
 * @return Заполненный HelloPayload
 */
idryer::UartHelloPayload buildHelloPayload(const MenuState &menu, const WorkTimeCounter &wtc);

/**
 * @brief Отправляет Hello сообщение в ESP32
 * @param menu Ссылка на MenuState
 * @param wtc Ссылка на WorkTimeCounter
 * @param ackRequired Требуется ли подтверждение
 * @return true если отправка успешна
 */
bool sendHello(const MenuState &menu, const WorkTimeCounter &wtc, bool ackRequired = false);

// =============================================================================
// ОТПРАВКА ДАННЫХ
// =============================================================================

/**
 * @brief Отправляет телеметрию (температура, влажность, мощность)
 */
void sendUartTelemetry();

/**
 * @brief Отправляет статус (режимы работы, таймеры)
 */
void sendUartStatus();

/**
 * @brief Отправляет heartbeat
 */
void sendUartHeartbeat();

/**
 * @brief Отправляет данные весов (weight data)
 */
void sendUartWeights();

/**
 * @brief Отправляет ClaimStart (инициация claiming процесса)
 */
bool sendUartClaimStart();

/**
 * @brief Отправляет RFID событие
 * @param event Тип события (TagDetected/TagRemoved)
 * @param readerId ID ридера (0-3)
 * @param tag HEX ID метки
 * @param unitId ID юнита (0-3)
 */
bool sendRfidEvent(idryer::UartRfidEvent event, uint8_t readerId, const char *tag, uint8_t unitId);

// =============================================================================
// ОБРАБОТЧИКИ ВХОДЯЩИХ СООБЩЕНИЙ
// =============================================================================

/**
 * @brief Обработчик команд от ESP32 (start/stop)
 */
void handleUartCommand(const idryer::UartCmdPayload &payload, const idryer::UartFrameHeader &header);

/**
 * @brief Обработчик Profile команд от ESP32
 */
void handleUartProfileCommand(const idryer::UartProfilePayload &payload, const idryer::UartFrameHeader &header);

/**
 * @brief Обработчик Hello/HelloAck сообщений
 */
void handleUartHello(const idryer::UartHelloPayload &payload, const idryer::UartFrameHeader &header);

/**
 * @brief Обработчик HelloAck — handshake завершён, отправляем WsEnable
 */
void handleUartHelloAck(const idryer::UartHelloAckPayload &payload, const idryer::UartFrameHeader &header);

/**
 * @brief Обработчик heartbeat от ESP32
 */
void handleUartHeartbeat(const idryer::UartHeartbeatPayload &payload, const idryer::UartFrameHeader &header);

/**
 * @brief Обработчик статуса claim процесса
 */
void handleUartClaimStatus(const idryer::UartClaimStatusPayload &payload, const idryer::UartFrameHeader &header);

/**
 * @brief Обработчик завершения claiming
 */
void handleUartClaimComplete(const idryer::UartClaimCompletePayload &payload, const idryer::UartFrameHeader &header);

/**
 * @brief Обработчик ошибок UART протокола
 */
void handleUartError(const idryer::UartErrorPayload &payload, bool remote);

/**
 * @brief Обработчик ConfigPush - получение команд set/invoke (фрагментированный)
 */
void handleUartConfigPush(const idryer::UartConfigChunkPayload &payload, uint8_t dataLen, const idryer::UartFrameHeader &header);

// =============================================================================
// WEBSOCKET LOCAL ACCESS
// =============================================================================

/**
 * @brief Отправляет WsEnable (включить/выключить WS сервер на ESP32)
 * @param enable true=включить, false=выключить
 */
bool sendWsEnable(bool enable);

/**
 * @brief Запросить текущий статус WS сервера
 */
bool sendWsStatusRequest();

/**
 * @brief Обработчик WsStatus от ESP32
 */
void handleUartWsStatus(const idryer::UartWsStatusPayload &payload, const idryer::UartFrameHeader &header);

/**
 * @brief Получить последний статус WS сервера
 */
const idryer::UartWsStatusPayload& getWsStatus();

/**
 * @brief Проверить, включён ли WS сервер
 */
bool wsEnabled();

// =============================================================================
// REMOTE CONFIG
// =============================================================================

/**
 * @brief Отправляет полный конфиг меню (все значения)
 * @param menu Ссылка на MenuState
 * @return true если отправка успешна
 */
bool sendFullConfig(const MenuState &menu);

/**
 * @brief Отправляет дельту конфига (одно изменённое значение)
 * @param itemId ID элемента меню (MENU_*)
 * @param unit Индекс юнита (0-3) для per-unit настроек, 0 для global
 */
void sendConfigDelta(uint16_t itemId, uint8_t unit);

// =============================================================================
// ERROR EVENTS
// =============================================================================

/**
 * @brief Отправляет ErrorEvent как Log сообщение в ESP32
 * @param ev Указатель на ErrorEvent
 * @return true если отправка успешна
 */
bool sendUartErrorEvent(const ErrorEvent *ev);
