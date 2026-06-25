/*****************************************************************
************ HIGH IMPACT CONFIGURATION AREA **********************
********* Changes here may break core functionality. *************
********* Modify only with full understanding of effects. ********
*****************************************************************/

/**********************
0 - русский
1 - english
 ********************/
// #define MAX_NUM_UNITS 3

#define PID_UPDATE_INTERVAL_MS 250 // PID update interval in milliseconds
#define SAMPLE_RATE_HZ 100         // Sensor read interval in Hz
#define PID_UPDATE_INTERVAL_S (PID_UPDATE_INTERVAL_MS / 1000.0f)

// ---- Verify Heater (klipper-like) ----
#define VH_HYST_C 5.0f             // зона цели 5C (hysteresis)
#define VH_TICK_PERIOD_MS 1000UL   // интеграция ошибки раз в 1s
#define VH_DISARM_PWM_OFFSET 0.05f // автоматический отступ от vh_arm_pwm_min для DISARM (5%)
#define VH_SNAPSHOT_RISE_C 1.0f    // setpoint вырос на >1°C - ждем нагрев - новый snapshot
#define VH_SNAPSHOT_FALL_C 1.0f    // setpoint упал на >1°C disarm уже не греем

/* ---- Hardware Configuration ----
 * Порты конфигурируются в рантайме через меню (menu.port*_mode).
 * См. port_config.h: hasScreen(), hasLink(), getLinkPort(), getScalesPort()
 */
#define MAX_NUM_UNITS 3
#define NUM_UNITS MAX_NUM_UNITS

// UART_ENABLED=1 всегда: модуль компилируется, но инициализируется только если есть LNK-порт
#define UART_ENABLED 1

// Serial  = USB (не требует дополнительных пинов)
// Serial1/Serial2 - пины определены в hardware.h (UART1_TX_PIN, UART1_RX_PIN, etc.)

#define UART_BAUD_RATE 115200 // Скорость передачи (baud)

// Интервалы отправки данных (миллисекунды)
#define UART_TELEMETRY_INTERVAL_MS 5000  // Телеметрия (температура, влажность) каждые 5 сек
#define UART_STATUS_INTERVAL_MS 10000    // Статус (режим, таймеры) каждые 10 сек
#define UART_WEIGHTS_INTERVAL_MS 10000   // Веса филамента каждые 10 сек
#define UART_HEARTBEAT_INTERVAL_MS 30000 // Heartbeat (uptime) каждые 30 сек

/**************************************************
****************** USER SETTINGS ******************
***************************************************/

#define BOARD_REV_1_0 0

//! >>> Specify the hardware version: <<<
#define BOARD_REVISION BOARD_REV_1_0 // BOARD_REV_1_1 or BOARD_REV_1_0

#define HEATER_MAX 140.0f // Max
#define AIR_MAX 130.0f    // Max

#define HEATER_MIN_TEMP_DELTA 0.1f                  // Minimum required temperature increase
#define HEATER_RESPONSE_TIMEOUT_MS (1000 * 60 * 30) // Time to wait for heater response
#define HEATER_MIN_PWM 80.0f                        // Minimum PWM value for response check

#define AIR_MIN_TEMP_DELTA 1.0f                  // Minimum required air temperature increase
#define AIR_RESPONSE_TIMEOUT_MS (1000 * 60 * 30) // Time to wait for air response

#define FAN_ON_TEMP 55.0f
#define FAN_OFF_TEMP 35.0f
#define FAN_ON_ACTIVE_MODE_TEMP 40.0f

#define SERVO_MIN_PULSE 500
#define SERVO_MAX_PULSE 2000
#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180

// Thermistor type selection macros
#define THERMISTOR_TYPE_0 0 // ATC Semitec 104GT-2
#define THERMISTOR_TYPE_1 1 // ATC Semitec 104NT-4-R025H42G
#define THERMISTOR_TYPE_2 2 // EPCOS 100K B57560G104F
#define THERMISTOR_TYPE_3 3 // Generic 3950
#define THERMISTOR_TYPE_4 4 // SliceEngineering 450
#define THERMISTOR_TYPE_5 5 // TDK NTCG104LH104JT1

//! Select thermistor type here
#define HEATER_UNIT0_THERMISTOR_MODEL THERMISTOR_TYPE_3
#define HEATER_UNIT1_THERMISTOR_MODEL THERMISTOR_TYPE_3
#define HEATER_UNIT2_THERMISTOR_MODEL THERMISTOR_TYPE_3

// Actual resistance of the pull-up resistors
#define PULLUP_UNIT0 4700.0f
#define PULLUP_UNIT1 4700.0f
#define PULLUP_UNIT2 4700.0f

// Resistor wire resistance
#define INLINE_RESISTOR_UNIT0 0.0f
#define INLINE_RESISTOR_UNIT1 0.0f
#define INLINE_RESISTOR_UNIT2 0.0f

// #define ALPHA 0.05f
#define ALPHA 0.05f // 0.1f
