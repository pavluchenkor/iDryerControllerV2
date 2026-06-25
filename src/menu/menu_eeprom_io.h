
// Auto-generated. Do not edit.
#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

// Чтение произвольного типа T из EEPROM
template<typename T>
inline bool ee_read(uint16_t addr, T& out){
  EEPROM.get(addr, out);
  return true;
}

// Запись произвольного типа T в EEPROM (всегда пишет)
template<typename T>
inline bool ee_write(uint16_t addr, const T& val){
  EEPROM.put(addr, val);
  #if defined(EEPROM_CLASS_VERSION) || defined(ARDUINO_ARCH_RP2040)
    EEPROM.commit();
  #endif
  return true;
}

// Записать ТОЛЬКО если значение изменилось; вернуть, было ли изменение.
template<typename T>
inline bool ee_store_field(uint16_t addr, const T& val){
  T cur{};
  EEPROM.get(addr, cur);
  if (memcmp(&cur, &val, sizeof(T))==0) return false;
  EEPROM.put(addr, val);
  #if defined(EEPROM_CLASS_VERSION) || defined(ARDUINO_ARCH_RP2040)
    EEPROM.commit();
  #endif
  return true;
}
