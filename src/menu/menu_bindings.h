// AUTO-GENERATED. DO NOT EDIT.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "menu_types.h"
#include "menu_state.h"
#include "menu_eeprom.h"

typedef void (*MenuOnChangeFn)(void* ctx);

typedef enum { SCOPE_GLOBAL=0, SCOPE_PER_CONTROLLER=1 } MenuScope;

typedef struct {
  uint16_t      id;         // MenuItem ID для отслеживания изменений
  const char*   bind;
  ValueType     vtype;
  void*         ptr;        // ptr на глобальное поле или на [0] массива
  bool          persist;
  MenuOnChangeFn on_change;
  bool          apply_live;
  MenuScope     scope;
  uint16_t      ee_base;    // базовый оффсет (global) или начало блока (per_controller)
  uint16_t      ee_stride;  // 0 для global, size-of-type для per_controller
} MenuBinding;

extern const MenuBinding g_bindings[];
extern const uint16_t     g_bindings_count;

// должен вернуть индекс активного контроллера [0..NUM_UNITS-1]; реализация находится в проекте
#ifdef __cplusplus
extern "C" {
#endif
uint8_t menu_get_active_controller(void);
#ifdef __cplusplus
}
#endif

// Config change hook - вызывается после успешного применения любого изменения
typedef void (*ConfigChangeHookFn)(uint16_t itemId, uint8_t unit, const char* bind);
#ifdef __cplusplus
extern "C" {
#endif
extern ConfigChangeHookFn g_config_change_hook;
void menu_set_config_change_hook(ConfigChangeHookFn hook);
#ifdef __cplusplus
}
#endif

bool menu_apply_by_bind(const char* bind, float v);
bool menu_read_by_bind(const char* bind, void* out_value);
const MenuBinding* menu_find_bind(const char* bind);
