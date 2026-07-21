#include "service_screen.h"
#include "claiming/claiming.h"
#include "hardware/port_config.h"

#define LOG_LEVEL LOG_LEVEL_INFO
#include "debug_log.h"
#define LOG_TAG "SERVICE_SCREEN"

// ------------------------ ВСПОМОГАТЕЛЬНЫЕ ------------------------
// шапка: "MODE U#" слева, "TIME" справа, линия-разделитель
static void headerLine(const char *modeName, uint8_t unit_id, const char *timeStr) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_unifont_t_cyrillic);
  u8g2.setFontMode(0);
  u8g2.setFontPosTop();

  // Левая часть: "MODE U#"
  char left[24];
  snprintf(left, sizeof(left), "%s U%u", modeName, (unsigned)(unit_id + 1));

  // Правая часть: TIME (или "--:--")
  const char *right = (timeStr && *timeStr) ? timeStr : "--:--";

  // Лево
  u8g2.drawUTF8(0, 0, left);

  // Право по правому краю
  uint16_t w = u8g2.getUTF8Width(right);
  int16_t x = (int16_t)u8g2.getDisplayWidth() - (int16_t)w;
  if (x < 0) x = 0;
  u8g2.drawUTF8(x, 0, right);

  // Разделитель под шапкой
  u8g2.drawHLine(0, 16, u8g2.getDisplayWidth());
}

// печать текстовой метки слева и значения справа в одну строку
static void rowLabelValue(uint8_t y, const char *label, const char *value) {
  u8g2.drawUTF8(0, y, label);
  uint16_t w = u8g2.getUTF8Width(value);
  int16_t x = (int16_t)u8g2.getDisplayWidth() - (int16_t)w;
  if (x < 0) x = 0;
  u8g2.drawUTF8(x, y, value);
}

// формат числа (или "--")
static void fmtNumber(float v, char *out, size_t n, uint8_t digits = 0) {
  if (!isfinite(v)) {
    snprintf(out, n, "--");
    return;
  }
  if (digits == 0)
    snprintf(out, n, "%d", (int)lroundf(v));
  else
    snprintf(out, n, "%.*f", digits, v);
}

// три строки: AIR / HUM / HEATER
static void threeRows_TH(const DryerInputs &in) {
  // Координаты под шапкой
  const uint8_t baseY = 22; // первая строка
  const uint8_t stepY = 14; // шаг между строками
  char val[16];

  u8g2.setFont(u8g2_font_unifont_t_cyrillic);

  // AIR
  fmtNumber(in.airTempC, val, sizeof(val), 0); // ХХХ
  rowLabelValue(baseY + 0 * stepY, "AIR", val);

  // HUM
  fmtNumber(in.airHumRH, val, sizeof(val), 0);
  rowLabelValue(baseY + 1 * stepY, "HUM", val);

  // HEATER
  fmtNumber(in.heaterTempC, val, sizeof(val), 0);
  rowLabelValue(baseY + 2 * stepY, "HEATER", val);
}

static void twoRows_TH(const DryerInputs &in) {
  // Координаты под шапкой
  const uint8_t baseY = 22; // первая строка
  const uint8_t stepY = 14; // шаг между строками
  char val[16];

  u8g2.setFont(u8g2_font_unifont_t_cyrillic);

  // AIR
  fmtNumber(in.airTempC, val, sizeof(val), 0); // «ХХХ»
  rowLabelValue(baseY + 0 * stepY, "AIR", val);

  // HUM
  fmtNumber(in.airHumRH, val, sizeof(val), 0);
  rowLabelValue(baseY + 1 * stepY, "HUM", val);

  // HEATER
  //   fmtNumber(in.heaterTempC, val, sizeof(val), 0);
  //   rowLabelValue(baseY + 2*stepY, "HEATER", val);
}

static void oneRow(uint8_t p, const char *label, uint16_t data) {
  // Координаты под шапкой
  const uint8_t baseY = 22; // первая строка
  const uint8_t stepY = 14; // шаг между строками
  char val[16];
  if (p == 0) p = 1;

  u8g2.setFont(u8g2_font_unifont_t_cyrillic);
  fmtNumber(float(data), val, sizeof(val), 0);
  rowLabelValue(baseY + (p - 1) * stepY, label, val);
}

// Формат "MM:SS" (до 99:59). Возвращает указатель на buf.
static const char *fmtRemain(uint32_t now, uint32_t startMs, uint32_t totalMs, char *buf, size_t n) {
  if (!totalMs) {
    snprintf(buf, n, "--:--");
    return buf;
  }

  // Если таймер ещё не стартовал показываем полную длительность.
  if (startMs == 0u) {
    uint32_t total_min = totalMs / 60000u;
    uint32_t hh = total_min / 60u;
    uint32_t mm = total_min % 60u;
    if (hh > 99u) hh = 99u;
    snprintf(buf, n, "%02u:%02u", (unsigned)hh, (unsigned)mm);
    return buf;
  }

  uint32_t elapsed = now - startMs; // unsigned разность корректно работает при переполнении millis()
  uint32_t remMs = (elapsed < totalMs) ? (totalMs - elapsed) : 0u;

  uint32_t total_min = remMs / 60000u;
  uint32_t hh = total_min / 60u;
  uint32_t mm = total_min % 60u;
  if (hh > 99u) hh = 99u; // защита от артефактов
  snprintf(buf, n, "%02u:%02u", (unsigned)hh, (unsigned)mm);
  return buf;
}

#if 0 // CLAIM-ЭКРАН ВРЕМЕННО ОТКЛЮЧЁН
static const char *claimStatusToStr(idryer::UartClaimStatus st) {
  switch (st) {
  case idryer::UartClaimStatus::Provisioning:
    return "PROVISION";
  case idryer::UartClaimStatus::WaitingClaim:
    return "WAIT PIN";
  case idryer::UartClaimStatus::Claimed:
    return "CLAIMED";
  case idryer::UartClaimStatus::Error:
    return "ERROR";
  default:
    return "IDLE";
  }
}

static void fmtSeconds(uint32_t sec, char *buf, size_t n) {
  uint32_t mm = sec / 60;
  uint32_t ss = sec % 60;
  if (mm > 99) mm = 99;
  snprintf(buf, n, "%02u:%02u", (unsigned)mm, (unsigned)ss);
}

static void formatDeviceTail(const char *deviceId, char *out, size_t n) {
  if (!deviceId || !deviceId[0]) {
    out[0] = '\0';
    return;
  }
  size_t len = strlen(deviceId);
  if (len <= 6) {
    strncpy(out, deviceId, n - 1);
    out[n - 1] = '\0';
    return;
  }
  snprintf(out, n, "...%s", deviceId + (len - 6));
}

static void drawClaimScreen(uint32_t now) {
  ClaimUiSnapshot snap = claim_get_snapshot(now);
  if (!snap.visible) return;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_unifont_t_cyrillic);
  u8g2.setFontMode(0);
  u8g2.setFontPosTop();

  u8g2.drawUTF8(0, 0, "PIN MODE");
  u8g2.drawHLine(0, 14, u8g2.getDisplayWidth());

  const uint8_t baseY = 18;
  const uint8_t stepY = 12;

  const char *pin = snap.pin[0] ? snap.pin : "--------";
  char remainBuf[8];
  fmtSeconds(snap.remainingSeconds, remainBuf, sizeof(remainBuf));

  rowLabelValue(baseY + 0 * stepY, "STATUS", claimStatusToStr(snap.status));
  rowLabelValue(baseY + 1 * stepY, "PIN", pin);
  rowLabelValue(baseY + 2 * stepY, "LEFT", remainBuf);

  if (snap.deviceId[0]) {
    char devTail[16];
    formatDeviceTail(snap.deviceId, devTail, sizeof(devTail));
    rowLabelValue(baseY + 3 * stepY, "DEV", devTail);
  }

  u8g2.sendBuffer();
}
#endif // CLAIM-ЭКРАН ВРЕМЕННО ОТКЛЮЧЁН

static inline bool unitHasData(const DryerInputs &in) {
  // юнит пустой, если нет обоих датчиков (оба NaN)
  return !(isnan(in.airTempC) && isnan(in.heaterTempC));
}

void rotateToNextUnit(uint32_t now) {
  for (uint8_t k = 0; k < menu.units_count; ++k) {
    uint8_t cand = (uint8_t)((gscr_active_unit + 1 + k) % menu.units_count);

    DryerInputs inCand = controllers[cand]->getInputs(); // снимок из inputs_, без side-effect
    //! if (!unitHasData(inCand)) continue; //! пропуск пустых
    gscr_active_unit = cand;
    menu.number_controller = cand;  // Синхронизируем с меню
    break;
  }
  gscr_next_unit_rotate_ms = now + UNIT_ROTATE_MS;
}

void uiTick(uint32_t now, MenuUI &menuUi) {
  if (!hasScreen()) return;

  // Гашение экрана после SCREEN_OFF_MS без ввода (энкодер/кнопки). Экран ошибок
  // не гасим. Пробуждение — markUserInput() обновляет g_lastInputMs.
  static bool screenOff = false;
  const bool idleLong = (uint32_t)(now - g_lastInputMs) >= SCREEN_OFF_MS;
  if (idleLong && !g_err_ack_active) {
    if (!screenOff) { u8g2.setPowerSave(1); screenOff = true; }
    return;
  }
  if (screenOff) { u8g2.setPowerSave(0); screenOff = false; }

  if (!g_err_ack_active && (int32_t)(now - gscr_next_unit_rotate_ms) >= 0) {
    rotateToNextUnit(now);
  }

  const uint8_t u = gscr_active_unit % NUM_UNITS;
  const DryerController *ctrl = (u < NUM_UNITS) ? controllers[u] : nullptr;
  DryerMode mode = ctrl ? ctrl->mode() : DryerMode::Idle;
  // Снимок из inputs_ (main loop обновил перед uiTick) — без дублирующего
  // sensor tick и report_sensor_error (устраняет удвоение спама событий).
  DryerInputs in = ctrl ? ctrl->getInputs() : DryerInputs{};

  drawScreen(now, in, mode, u, menuUi, ctrl);
}

// вызывать каждый кадр, когда показан оверлей ошибок
void errorOverlayTick(uint32_t now) {
  if (errlog_count() == 0) {
    g_err_ack_active = false;
    g_err_view_idx = -1;
    return;
  }

  if (!g_err_ack_active) {
    error_ack_begin();
    return; // в этот тик больше ничего не делаем
  }

  int8_t delta = enc->tick();
  bool click = enc_btn->released();

  // 4) передаём события в ACK-логику
  error_ack_update(delta, click, false, now);
}

// ------------------------ РЕЖИМНЫЕ  РИСОВАЛКИ ------------------------

void drawStatusError(const DryerInputs & /*in*/, uint8_t unit_id, const char *timeStr) { errorOverlayTick(millis()); }

void drawStatusIdle(const DryerInputs &in, uint8_t unit_id, const char *timeStr) {
  headerLine("IDLE", unit_id, timeStr);
  //   threeRows_TH(in);
  twoRows_TH(in);
  if (hx711MultiPtr && menu.scales_count == 1) oneRow((3), "WEIGHT", hx711Multi.getMassMulti(0));
  u8g2.sendBuffer();
}

void drawStatusDrying(const DryerInputs &in, uint8_t unit_id, const char *timeStr) {
  headerLine("DRYING", unit_id, timeStr);
  threeRows_TH(in);
  u8g2.sendBuffer();
}

void drawStatusStorage(const DryerInputs &in, uint8_t unit_id, const char *timeStr) {
  headerLine("STORAGE", unit_id, timeStr);
  threeRows_TH(in);
  u8g2.sendBuffer();
}

void drawStatusProfile(const DryerInputs &in, uint8_t unit_id, const char * /*timeStr*/) {
  const DryerController *ctrl = controllers[unit_id];
  if (!ctrl) return;

  uint8_t currentStage = ctrl->getProfileCurrentStage();
  ProfileState::Phase phase = ctrl->getProfilePhase();
  const char *phaseStr = (phase == ProfileState::Phase::Ramp) ? "RAMP" : "HOLD";

  // Вычисляем оставшееся время текущей фазы
  uint32_t remainingMs = ctrl->getProfilePhaseRemainingMs();
  uint32_t remainingMin = remainingMs / 60000u;
  uint32_t remainingSec = (remainingMs % 60000u) / 1000u;
  char phaseTimeStr[8];
  snprintf(phaseTimeStr, sizeof(phaseTimeStr), "%02u:%02u", (unsigned)(remainingMin > 99 ? 99 : remainingMin), (unsigned)remainingSec);

  char header[24];
  snprintf(header, sizeof(header), "S%u %s", (unsigned)(currentStage + 1), phaseStr);
  headerLine(header, unit_id, phaseTimeStr);

  // Показываем TARGET, AIR, HEATER
  const uint8_t baseY = 22;
  const uint8_t stepY = 14;
  char val[16];

  u8g2.setFont(u8g2_font_unifont_t_cyrillic);

  // TARGET (целевая температура текущего стейджа)
  uint16_t targetTemp = ctrl->getProfileTargetTemp();
  fmtNumber((float)targetTemp, val, sizeof(val), 0);
  rowLabelValue(baseY + 0 * stepY, "TARGET", val);

  // AIR
  fmtNumber(in.airTempC, val, sizeof(val), 0);
  rowLabelValue(baseY + 1 * stepY, "AIR", val);

  // HEATER
  fmtNumber(in.heaterTempC, val, sizeof(val), 0);
  rowLabelValue(baseY + 2 * stepY, "HEATER", val);

  u8g2.sendBuffer();
}

void drawStatusAutoTune(const DryerInputs &in, uint8_t unit_id, const char * /*timeStr*/) {
  // --- сводка из контроллера ---
  PidAutoView v{};
  const uint32_t now = millis();
  const DryerController *ctrl = controllers[unit_id];
  if (ctrl) {
    const_cast<DryerController *>(ctrl)->getPidAutoView(v, now);
  }

  const bool isHeater = (v.targetSel == AutoTuneTarget::HeaterTemp);

  // --- шапка БЕЗ времени ---
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_unifont_t_cyrillic);
  u8g2.setFontMode(0);
  u8g2.setFontPosTop();

  char header[20];
  snprintf(header, sizeof(header), "PID %s", isHeater ? "HEATER" : "AIR");
  // слева: "PID HEATER U#"
  char left[24];
  snprintf(left, sizeof(left), "%s U%u", header, (unsigned)(unit_id + 1));
  u8g2.drawUTF8(0, 0, left);

  // разделитель под шапкой
  u8g2.drawHLine(0, 16, u8g2.getDisplayWidth());

  // --- тело экрана ---
  char right[24];

  if (v.active) {
    // ИДЁТ автопид
    // строка 1: "HEAT/COOL 3/12" слева, "T:80" справа
    char stat[24];
    snprintf(stat, sizeof(stat), "%s %u/%u", v.heating ? "HEAT" : "COOL", (unsigned)v.peaks, (unsigned)v.needCycles);
    snprintf(right, sizeof(right), "T:%d", (int)lroundf(v.targetC));
    rowLabelValue(22, stat, right);

    // строка 2: "TEMP" слева, текущее значение справа
    if (isfinite(v.currentTempC))
      snprintf(right, sizeof(right), "%d", (int)lroundf(v.currentTempC));
    else
      snprintf(right, sizeof(right), "--");
    rowLabelValue(36, "TEMP", right);

    // строка 3: "POWER" слева, "75% | 13" справа (БЕЗ 's')
    int duty = (int)(v.duty01 * 100.0f + 0.5f);
    snprintf(right, sizeof(right), "%d%% | %lu", duty, (unsigned)(v.sinceToggleMs / 1000UL));
    rowLabelValue(50, "POWER", right);

  } else if (v.resultOk) {
    // ГОТОВЫ ПИДЫ
    snprintf(right, sizeof(right), "%.2f", v.Kp);
    rowLabelValue(22, "Kp", right);

    snprintf(right, sizeof(right), "%.2f", v.Ki);
    rowLabelValue(36, "Ki", right);

    snprintf(right, sizeof(right), "%.2f", v.Kd);
    rowLabelValue(50, "Kd", right);

  } else {
    // нет процесса и нет результата
    rowLabelValue(22, "STATUS", "IDLE");
  }

  u8g2.sendBuffer();
}

void drawWeightScreen(const DryerInputs &in, uint8_t unit_id, const char *timeStr) {
  // Если катушек <= 3, показываем с шапкой
  // Если катушек 4, рисуем без шапки, чтобы влезло
  if (menu.scales_count <= 3) {
    headerLine("WEIGHT", unit_id, timeStr);
    u8g2.setFont(u8g2_font_unifont_t_cyrillic);
    if (hx711MultiPtr) {
      for (uint8_t i = 0; i < menu.scales_count; ++i) {
        char label[16];
        snprintf(label, sizeof(label), "SPOOL %u", (unsigned)(i + 1));
        oneRow((i + 1), label, hx711Multi.getMassMulti(i));
      }
    } else {
      oneRow(1, "SCALES", 0);
      oneRow(2, "ERROR", 0);
    }
  } else {
    // 4 катушки: без шапки, начинаем с самого верха
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_unifont_t_cyrillic);
    u8g2.setFontMode(0);
    u8g2.setFontPosTop();

    const uint8_t baseY = 2;  // начинаем почти с самого верха
    const uint8_t stepY = 14; // шаг между строками

    if (hx711MultiPtr) {
      for (uint8_t i = 0; i < menu.scales_count; ++i) {
        char label[16];
        char val[16];
        snprintf(label, sizeof(label), "SPOOL %u", (unsigned)(i + 1));
        fmtNumber(float(hx711Multi.getMassMulti(i)), val, sizeof(val), 0);
        rowLabelValue(baseY + i * stepY, label, val);
      }
    } else {
      rowLabelValue(baseY, "SCALES", "ERROR");
    }
  }
  u8g2.sendBuffer();
}

// ------------------------ ЦЕНТРАЛЬНЫЙ ДИСПЕТЧЕР ------------------------
void drawScreen(uint32_t now, const DryerInputs &in, DryerMode mode, uint8_t unit_id, MenuUI &menuUi, const DryerController *ctrl) {
  ScreenState &ss = gscr_units[unit_id];

  // если активирован overlay ошибок — показываем только его и выходим
  if (g_err_ack_active) {
    drawStatusError(in, unit_id, "--:--"); // время тут не важно
    return;
  }

  // CLAIM-ЭКРАН ВРЕМЕННО ОТКЛЮЧЁН
  // claim_tick(now); // авто-скрытие claim-экрана после успешной привязки
  // if (claim_is_visible()) {
  //   drawClaimScreen(now);
  //   return;
  // }

  // Переключение “режим/весы”
  // if (mode != DryerMode::Idle && now >= ss.nextSwapMs) {
  if ((mode != DryerMode::PidAutoTune && now >= ss.nextSwapMs && menu.scales_count > 1) ||
      ((mode == DryerMode::Drying || mode == DryerMode::Storage) && now >= ss.nextSwapMs && menu.scales_count > 0)) {
    ss.weightPhase = !ss.weightPhase;
    ss.nextSwapMs = now + MODE_SWAP_MS;
  }

  // if(menu.scales_count <= 1) ss.weightPhase = false;
  // ss.weightPhase = bool(menu.scales_count);
  // if (mode == DryerMode::Idle) ss.weightPhase = true; //TODO: test

  // Считаем строку времени
  char timeBuf[8] = "--:--";
  const char *timeStr = timeBuf;

  // Показываем обратный отсчёт только там, где он есть
  if (ctrl && mode == DryerMode::Drying) {
    timeStr = fmtRemain(now, ctrl->dryStartMs(), ctrl->dryTotalMs(), timeBuf, sizeof(timeBuf));
  }

  // В Idle приоритет у меню при недавнем вводе
  if (mode == DryerMode::Idle) {
    const bool showStatus = (now - ss.lastInputMs) > IDLE_TO_STATUS_MS;
    if (!showStatus && hasScreen()) {
      menuUi.setFont();
      menuUi.draw(); // меню при активности
      return;
    }
    // Если неактивен продолжаем к weightPhase или статусу
  }

  // Переключение весов/режима (для всех режимов включая Idle)
  if (ss.weightPhase && menu.scales_count > 0) {
    drawWeightScreen(in, unit_id, timeStr);
    return;
  }

  if (mode == DryerMode::PidAutoTune && ctrl) {
    PidAutoView v{};
    const uint32_t now_view = now;
    const_cast<DryerController *>(ctrl)->getPidAutoView(v, now_view);

    const bool finished = (!v.active && v.resultOk);
    // если юзер недавно крутил  показываем меню вместо статуса
    if (finished && (now - ss.lastInputMs) <= IDLE_TO_STATUS_MS && hasScreen()) {
      menuUi.setFont();
      menuUi.draw();
      return;
    }
  }

  switch (mode) {
  case DryerMode::Error:
    drawStatusError(in, unit_id, timeStr);
    break;
  case DryerMode::Drying:
    drawStatusDrying(in, unit_id, timeStr);
    break;
  case DryerMode::Storage:
    drawStatusStorage(in, unit_id, timeStr);
    break;
  case DryerMode::Profile:
    drawStatusProfile(in, unit_id, timeStr);
    break;
  case DryerMode::PidAutoTune:
    drawStatusAutoTune(in, unit_id, timeStr);
    break;
  default:
    drawStatusIdle(in, unit_id, timeStr);
    break;
  }
}
