#include <Arduino.h>        // даёт Print/Serial
#include "menu_state_dump.h"

extern const MenuBinding g_bindings[];
extern const uint16_t    g_bindings_count;

static void print_one(Print& s, const MenuBinding& b, uint8_t idx) {
  s.print(b.bind);
  if (b.scope != SCOPE_GLOBAL) { s.print('['); s.print(idx); s.print(']'); }
  s.print(": ");

  switch (b.vtype) {
    case VT_F32:  s.println((b.scope==SCOPE_GLOBAL) ? *(float*)b.ptr    : ((float*)b.ptr)[idx]); break;
    case VT_U32:  s.println((b.scope==SCOPE_GLOBAL) ? *(uint32_t*)b.ptr : ((uint32_t*)b.ptr)[idx]); break;
    case VT_I32:  s.println((b.scope==SCOPE_GLOBAL) ? *(int32_t*)b.ptr  : ((int32_t*)b.ptr)[idx]); break;
    case VT_U16:  s.println((b.scope==SCOPE_GLOBAL) ? *(uint16_t*)b.ptr : ((uint16_t*)b.ptr)[idx]); break;
    case VT_U8:   s.println((b.scope==SCOPE_GLOBAL) ? *(uint8_t*)b.ptr  : ((uint8_t*)b.ptr)[idx]); break;
    case VT_BOOL: s.println((b.scope==SCOPE_GLOBAL) ? (*(bool*)b.ptr ? "true":"false")
                                                    : (((bool*)b.ptr)[idx] ? "true":"false"));     break;
    default:      s.println("<unknown type>");
  }
}

void print_all_settings(Print& s) {
  s.println("=== Settings dump ===");
  for (uint16_t i=0; i<g_bindings_count; i++) {
    const auto& b = g_bindings[i];
    if (b.scope == SCOPE_GLOBAL) {
      print_one(s, b, 0);
    } else {
      for (uint8_t u = 0; u < NUM_UNITS; u++) print_one(s, b, u);
    }
  }
  s.println("=== /dump ===");
}