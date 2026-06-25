#pragma once
#include <stdint.h>
#include "menu/menu_types.h"
#include "menu/menu_bindings.h"

static inline const MenuBinding* find_binding_by_ptr(void* p) {
  for (uint16_t i = 0; i < g_bindings_count; ++i)
    if (g_bindings[i].ptr == p) return &g_bindings[i];
  return nullptr;
}

static inline bool ui_apply_from_menuitem(const MenuItem& it, float v) {
  if (it.type != MN_VALUE && it.type != MN_TOGGLE) return false;
  const MenuBinding* b = find_binding_by_ptr(it.u.value.ptr);
  if (!b) return false;
  return menu_apply_by_bind(b->bind, v);
}

static inline bool ui_read_from_menuitem(const MenuItem& it, void* out_val) {
  const MenuBinding* b = find_binding_by_ptr(it.u.value.ptr);
  if (!b) return false;
  return menu_read_by_bind(b->bind, out_val);
}