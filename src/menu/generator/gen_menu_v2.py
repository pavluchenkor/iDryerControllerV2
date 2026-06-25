#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор меню из YAML с поддержкой scope: global / per_controller.

- global         → одиночное поле (одно значение на всю систему)
- per_controller → массив [NUM_UNITS] и отдельные слоты в EEPROM (stride = sizeof(type))

Выход:
  menu_ids.h, menu_types.h, menu_eeprom.h,
  menu_state.h, menu_state.cpp,
  menu_eeprom_io.h, menu_eeprom_io.cpp,
  menu_data.cpp,
  menu_bindings.h, menu_bindings.cpp,
  menu_callbacks_weak.cpp,
  menu_presets_autogen.h
"""

import os, sys, re, argparse, textwrap, yaml

EE_SYS_CAL_SIZE = 48
EE_SCALE_TEMP_SIZE = 96

ERRLOG_HDR_SIZE = 12
ERRLOG_REC_SIZE = 16
ERRLOG_CAP      = 128

# ---------------------------
# Типы значений
# ---------------------------
VTYPES = {
    "float":  ("float",     "VT_F32", 4),
    "uint16": ("uint16_t",  "VT_U16", 2),
    "uint8":  ("uint8_t",   "VT_U8",  1),
    "int32":  ("int32_t",   "VT_I32", 4),
    "bool":   ("bool",      "VT_BOOL",1),
    "uint32": ("uint32_t",  "VT_U32", 4),
}
MENU_TYPES = {"submenu":"MN_SUBMENU","value":"MN_VALUE","toggle":"MN_TOGGLE","action":"MN_ACTION"}

# ---------------------------
# Фиксированная системная зона в начале EEPROM
# ---------------------------
EE_SYS_OFF_MAGIC    = 0x00  # 4 байта
EE_SYS_OFF_VERSION  = 0x04  # 4 байта (держим 4 для выравнивания)
EE_OFF_WT_HOURS     = 0x08  # uint32_t
EE_OFF_WT_MINUTES   = 0x0C  # uint8_t
EE_SYS_AREA_SIZE    = 0x10  # меню начинается отсюда

# Узлы, которые НЕЛЬЗЯ класть в persist-блок меню (хранить их только в системной зоне)
SKIP_PERSIST_IDS = {"wt_hours", "wt_minutes"}

# ---------------------------
# Утилиты
# ---------------------------
def to_enum_id(s: str) -> str:
    s = re.sub(r'[^a-zA-Z0-9_]+' , '_', s.strip())
    return s.upper()

def collect_langs(node, langs):
    if isinstance(node, dict):
        if "title" in node and isinstance(node["title"], dict):
            langs.update(node["title"].keys())
        if "unit" in node and isinstance(node.get("unit"), dict):
            langs.update(node["unit"].keys())
        for v in node.values():
            collect_langs(v, langs)
    elif isinstance(node, list):
        for x in node:
            collect_langs(x, langs)

def infer_lang_order(langs_set):
    ordered, base = [], ["ru","en"]
    for b in base:
        if b in langs_set:
            ordered.append(b); langs_set.remove(b)
    ordered += sorted(langs_set)
    return ordered

def c_string(s): return '"' + str(s).replace('\\','\\\\').replace('"','\\"') + '"'

def c_float(v):
    """Форматирует float для C++ литерала: 35 → 35.0f, 0.1 → 0.1f"""
    s = f"{float(v):.6g}"
    if '.' not in s and 'e' not in s.lower():
        s += ".0"
    return s + "f"

def flatten(nodes, parent_idx=-1, out=None, inherited_scope=None):
    """Сплющивает дерево и прокидывает scope вниз (по умолчанию per_controller)."""
    if out is None: out = []
    for n in nodes:
        idx = len(out)
        scope = n.get("scope", inherited_scope or "per_controller")
        entry = {
            "raw": n,
            "parent": parent_idx,
            "first_child": -1,
            "child_count": 0,
            "index": idx,
            "scope": scope
        }
        out.append(entry)
        kids = n.get("children") or []
        if kids:
            out[idx]["first_child"] = len(out)
            out[idx]["child_count"] = len(kids)
            flatten(kids, idx, out, scope)
    return out

def node_title_unit_arrays(raw, lang_order):
    # title
    if isinstance(raw.get("title"), dict):
        titles = [c_string(raw["title"].get(l, raw["title"].get("en",""))) for l in lang_order]
    else:
        t = c_string(raw.get("title",""))
        titles = [t for _ in lang_order]
    # unit
    uo = raw.get("unit")
    if isinstance(uo, dict):
        units = [c_string(uo.get(l, uo.get("en",""))) for l in lang_order]
    elif isinstance(uo, str):
        u = c_string(uo); units = [u for _ in lang_order]
    else:
        # units = ["NULL" for _ in lang_order]
        units = ["nullptr" for _ in lang_order]
    return titles, units

def _find_up(filename, start):
    cur = os.path.abspath(start)
    while True:
        cand = os.path.join(cur, filename)
        if os.path.exists(cand):
            return cand
        parent = os.path.dirname(cur)
        if parent == cur:
            return None
        cur = parent

def read_version_major(default=1):
    # ищем version.h вверх от каталога скрипта
    here = os.path.dirname(__file__)
    vpath = _find_up("version.h", here)
    if not vpath:
        return default
    text = open(vpath, "r", encoding="utf-8").read()

    m = re.search(r"#define\s+VERSION_MAJOR\s+(\d+)", text)
    if m:
        return int(m.group(1))

    m = re.search(r'"(\d+)\.(\d+)\.(\d+)"', text)  # запасной вариант "X.Y.Z"
    if m:
        return int(m.group(1))  # major - первая группа

    return default

# ---------------------------
# Разметка EEPROM
# ---------------------------
# def eeprom_layout(flat, num_units: int):
#     """
#     Возвращает:
#       offsets: dict[id] = dict(base=<addr>, size=<bytes>, stride=<bytes>, scope=<str>)
#       total:   общий размер EEPROM (включая системную зону)
#     """
#     off = EE_SYS_AREA_SIZE
        
#     offsets = {}
#     for f in flat:
#         r = f["raw"]; scope = f["scope"]
#         t = r["type"]
#         if t in ("value","toggle") and r.get("persist", False):
#             rid = r.get("id")
#             if rid in SKIP_PERSIST_IDS:
#                 continue
#             vtype = r.get("vtype") or ("bool" if t=="toggle" else None)
#             if vtype not in VTYPES:
#                 raise SystemExit(f"Unknown vtype in {r.get('id')}")
#             size = VTYPES[vtype][2]
#             if scope == "global":
#                 offsets[rid] = dict(base=off, size=size, stride=0, scope=scope)
#                 off += size
#             else:
#                 offsets[rid] = dict(base=off, size=size, stride=size, scope=scope)
#                 off += size * num_units
#     return offsets, off  # total включает системную зону

# ---------------------------
# Генерация файлов
# ---------------------------
def emit_menu_ids_h(path, flat, lang_order):
    with open(path, "w", encoding="utf-8") as f:
        f.write("// Auto-generated. Do not edit.\n#pragma once\n#include <stdint.h>\n\ntypedef enum {\n")
        for fn in flat:
            f.write(f"  MENU_{to_enum_id(fn['raw']['id'])},\n")
        f.write("  MENU__COUNT\n} MenuId;\n\n")
        f.write("typedef enum {\n")
        for i,l in enumerate(lang_order):
            f.write(f"  LANG_{to_enum_id(l)} = {i},\n")
        f.write("  LANG_COUNT\n} LangId;\n")

def emit_menu_types_h(path):
    with open(path, "w", encoding="utf-8") as f:
        f.write(textwrap.dedent("""
// Auto-generated. Do not edit.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "menu_ids.h"

typedef enum { MN_SUBMENU, MN_ACTION, MN_VALUE, MN_TOGGLE } MenuType;
typedef enum { VT_F32, VT_U16, VT_U8, VT_I32, VT_BOOL, VT_U32} ValueType;

typedef struct {
  ValueType vtype;
  void*     ptr;
  float     minv, maxv, step;
  void    (*on_change)(void*);
  bool      apply_live;
} ValueSpec;

typedef struct MenuItem {
  uint16_t id;
  const char* title[LANG_COUNT];
  const char* unit[LANG_COUNT];
  MenuType type;
  int16_t parent;
  int16_t first_child;
  uint16_t child_count;
  struct {
    struct { void (*invoke)(); } action;
    ValueSpec value;
  } u;
  int16_t ee_offset; // -1 if no persist (compat/для UI)
  uint16_t ee_size;
} MenuItem;

extern const MenuItem g_menu[MENU__COUNT];
"""))

def emit_menu_eeprom_h(path, flat, offsets, total, magic=0x4D4E5551, version=1, num_units=3):
    with open(path, "w", encoding="utf-8") as f:
        f.write("// Auto-generated. Do not edit.\n#pragma once\n#include <stdint.h>\n\n")
        f.write(f"#define EE_MAGIC   0x{magic:08X}\n")
        f.write(f"#define EE_VERSION {version}\n\n")
        f.write(f"#ifndef NUM_UNITS\n#define NUM_UNITS {num_units}\n#endif\n\n")

        # Системная зона (фиксированное начало EEPROM)
        f.write("// System area (fixed at start of EEPROM)\n")
        f.write(f"#define EE_SYS_OFF_MAGIC   {EE_SYS_OFF_MAGIC}\n")
        f.write(f"#define EE_SYS_OFF_VERSION {EE_SYS_OFF_VERSION}\n")
        f.write(f"#define EE_OFF_WT_HOURS    {EE_OFF_WT_HOURS}\n")
        f.write(f"#define EE_OFF_WT_MINUTES  {EE_OFF_WT_MINUTES}\n")
        # f.write(f"#define EE_SYS_AREA_SIZE   {EE_SYS_AREA_SIZE}\n\n")


        # Системная область с калибровкой весов
        f.write("\n// ---- Scales calibration (protected, inside system area) ----\n")
        f.write(f"#define EE_SYS_CAL_BASE            {16}\n\n")
        
        # Zero calibration
        f.write("// Ноль для каждой катушки: uint32_t[4]\n")
        f.write("#define EE_SYS_CAL_ZERO_BASE       (EE_SYS_CAL_BASE + 0)\n")
        f.write("#define EE_SYS_CAL_ZERO(i)         (EE_SYS_CAL_ZERO_BASE + (uint16_t)((i) * sizeof(uint32_t))) // i=0..3\n\n")
        
        # Offset calibration
        f.write("// Оффсет/масштаб для каждой катушки: uint32_t[4]\n")
        f.write("#define EE_SYS_CAL_OFFSET_BASE     (EE_SYS_CAL_ZERO_BASE + 4*4)\n")
        f.write("#define EE_SYS_CAL_OFFSET(i)       (EE_SYS_CAL_OFFSET_BASE + (uint16_t)((i) * sizeof(uint32_t)))\n\n")
        
        # Calibration block size
        f.write("// общий размер блока калибровки + 16 на всякий\n")
        f.write("#define EE_SYS_CAL_SIZE            (sizeof(uint32_t) * 4 * 2 + 16)\n\n")

        # Temperature compensation table
        f.write("// ---- Scales temperature offsets table (uint32_t[4][6]) ----\n")
        f.write("#define EE_SCALE_TEMP_BASE              (EE_SYS_CAL_BASE + EE_SYS_CAL_SIZE)\n")
        f.write("#define EE_SCALE_TEMP_STRIDE_SPOOL      (6 * 4)   // 6 points per spool * 4 bytes each\n")
        f.write("// Accessor: spool i=0..3, point j=0..5 (e.g., temps 60..110)\n")
        f.write("#define EE_SCALE_TEMP(i,j)             (EE_SCALE_TEMP_BASE + (uint16_t)((i) * EE_SCALE_TEMP_STRIDE_SPOOL + (j) * sizeof(uint32_t)))\n")
        f.write("// Table size: 4 spools * 6 points * 4 bytes = 96 bytes\n")
        f.write("#define EE_SCALE_TEMP_SIZE             (4*6*4)\n\n")


        f.write(f"// Итоговый размер системной области: {EE_SYS_AREA_SIZE} (MAGIC..MINUTES) + {EE_SYS_CAL_SIZE} (калибровка) = {EE_SYS_AREA_SIZE + EE_SYS_CAL_SIZE}\n")
        f.write(f"#define EE_SYS_AREA_SIZE           {EE_SYS_AREA_SIZE + EE_SYS_CAL_SIZE +  EE_SCALE_TEMP_SIZE}\n\n")


# --- Error log area ---
        f.write("// ---- Error log area ----\n")
        f.write(f"#define ERRLOG_CAP           {ERRLOG_CAP}\n")
        f.write(f"#define ERRLOG_REC_SIZE      {ERRLOG_REC_SIZE}\n")
        f.write(f"#define ERRLOG_HDR_SIZE      {ERRLOG_HDR_SIZE}\n")
        f.write("#define EE_ERR_BASE          (EE_SYS_AREA_SIZE)\n")
        f.write("#define EE_ERR_OFF_HDR       (EE_ERR_BASE)\n")
        f.write("#define EE_ERR_OFF_RECS      (EE_ERR_BASE + ERRLOG_HDR_SIZE)\n")
        f.write("#define EE_ERR_AREA_SIZE     (ERRLOG_HDR_SIZE + (ERRLOG_CAP * ERRLOG_REC_SIZE))\n\n")


        # Offsets для persist-полей меню
        for fn in flat:
            nid = fn["raw"]["id"]
            if nid in offsets:
                o = offsets[nid]
                if o["stride"] == 0:  # global
                    f.write(f"#define EE_OFF_{nid.upper()} {o['base']}\n")
                else:  # per_controller
                    f.write(f"#define EE_OFF_{nid.upper()}_BASE   {o['base']}\n")
                    f.write(f"#define EE_OFF_{nid.upper()}_STRIDE {o['stride']}\n")
                    f.write(f"#define EE_OFF_{nid.upper()}(i) (EE_OFF_{nid.upper()}_BASE + (uint16_t)((i) * EE_OFF_{nid.upper()}_STRIDE))\n")
        f.write(f"\n#define EE_TOTAL_SIZE {total}\n")

def eeprom_layout(flat, num_units: int):
    """
    Возвращает:
      offsets: dict[id] = dict(base=<addr>, size=<bytes>, stride=<bytes>, scope=<str>)
      total:   общий размер EEPROM (включая системную зону)
    """
    # off = EE_SYS_AREA_SIZE
    # off = EE_SYS_AREA_SIZE + (ERRLOG_HDR_SIZE + ERRLOG_REC_SIZE * ERRLOG_CAP)
    off = EE_SYS_AREA_SIZE + EE_SYS_CAL_SIZE +  EE_SCALE_TEMP_SIZE + (ERRLOG_HDR_SIZE + ERRLOG_REC_SIZE * ERRLOG_CAP)
    
    offsets = {}
    for f in flat:
        r = f["raw"]
        scope = f["scope"]  # ВАЖНО: берём scope из узла flatten, а не r.get("scope")
        t = r["type"]
        if t in ("value","toggle") and r.get("persist", False):
            rid = r.get("id")
            if rid in SKIP_PERSIST_IDS:
                continue
            vtype = r.get("vtype") or ("bool" if t=="toggle" else None)
            if vtype not in VTYPES:
                raise SystemExit(f"Unknown vtype in {r.get('id')}")
            size = VTYPES[vtype][2]
            if scope == "global":
                offsets[rid] = dict(base=off, size=size, stride=0, scope=scope)
                off += size
            else:
                offsets[rid] = dict(base=off, size=size, stride=size, scope=scope)
                off += size * num_units
    return offsets, off

def emit_menu_state_h(path, flat, num_units):
    # bind -> (ctype, vtype, scope, default)
    by_bind = {}
    for fn in flat:
        r = fn["raw"]
        if r.get("type") in ("value","toggle") and r.get("bind") and r.get("vtype"):
            bind  = r["bind"]
            vtype = r.get("vtype") or ("bool" if r["type"] == "toggle" else None)
            ctype = VTYPES[vtype][0]
            scope = fn["scope"]               # <<< ВАЖНО: берем scope из плоского узла!
            dv    = r.get("default", 0)

            # Если один и тот же bind встречается в нескольких местах и
            # одна из них глобальная - выигрывает global (скаляр).
            if bind not in by_bind:
                by_bind[bind] = (ctype, vtype, scope, dv)
            else:
                c0, v0, s0, d0 = by_bind[bind]
                if s0 != "global" and scope == "global":
                    by_bind[bind] = (ctype, vtype, scope, dv)

    #! временная отладка
    for bind, (_, _, scope, _) in by_bind.items():
        print("FIELD", bind, "->", scope)

    def fmt_default(vtype, ctype, dv):
        if vtype in ("uint16","uint8","int32","uint32"):
            return f"({ctype}){int(dv)}"
        if vtype == "bool":
            return "true" if bool(dv) else "false"
        return c_float(dv)

    lines = []
    lines += [
        "// Auto-generated. Do not edit.",
        "#pragma once",
        "#include <stdint.h>",
        "#include <stdbool.h>",
        "#include \"menu_ids.h\"",
        "",
        "#ifndef NUM_UNITS",
        f"#define NUM_UNITS {num_units}",
        "#endif",
        "",
        "class MenuState {",
        "public:",
    ]

    # Генерируем поля класса по bind'ам
    for bind, (ctype, vtype, scope, dv) in by_bind.items():
        init = fmt_default(vtype, ctype, dv)
        if scope == "global":
            # СКАЛЯР - то, что нужно для controller_choice/language
            lines.append(f"  {ctype} {bind} = {init};")
        else:
            # Массив для per_controller
            lines.append(f"  {ctype} {bind}[NUM_UNITS] = {{ {init} }};")

    lines += [
        "",
        "  void initDefaults();   // выставить дефолты (из YAML)",
        "  void loadFromEEPROM(); // подхватить, если MAGIC/VER совпали",
        "  void saveToEEPROM();   // записать только изменённые поля",
        "};",
        "",
        "extern MenuState menu;",
    ]

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

def emit_menu_state_cpp(path, flat, offsets):
    h = []
    h.append("// Auto-generated. Do not edit.")
    h.append("#include <string.h>")
    h.append("#include <EEPROM.h>")
    h.append("#include \"menu_state.h\"")
    h.append("#include \"menu_types.h\"")
    h.append("#include \"menu_eeprom.h\"")
    h.append("#include \"menu_eeprom_io.h\"")
    h.append("")
    h.append("MenuState menu;")
    h.append("")

    # initDefaults: global -> скаляр, per_controller -> цикл
    h.append("void MenuState::initDefaults(){")
    for fn in flat:
        r = fn["raw"]; scope = fn["scope"]
        if r["type"] in ("value", "toggle") and "default" in r:
            vtype = r.get("vtype") or ("bool" if r["type"] == "toggle" else None)
            if vtype not in VTYPES: raise SystemExit(f"Unknown vtype for id={r.get('id')}")
            ctype = VTYPES[vtype][0]; bind = r["bind"]; dv = r["default"]
            def scalar(vt, ct, dv):
                if vt in ("uint16","uint8","int32","uint32"): return f"({ct}){int(dv)}"
                if vt == "bool": return "true" if bool(dv) else "false"
                return c_float(dv)
            if scope == "global":
                h.append(f"  this->{bind} = {scalar(vtype, ctype, dv)};")
            else:
                h.append(f"  for (int i=0;i<NUM_UNITS;i++) this->{bind}[i] = {scalar(vtype, ctype, dv)};")
    h.append("}")
    h.append("")

    # loadFromEEPROM: stride==0 -> скаляр, иначе цикл по i
    h.append("void MenuState::loadFromEEPROM(){")
    h.append("  uint32_t magic=0, ver=0;")
    h.append("  ee_read(EE_SYS_OFF_MAGIC, magic);")
    h.append("  ee_read(EE_SYS_OFF_VERSION, ver);")
    h.append("  if (magic != EE_MAGIC || ver != EE_VERSION) return;")
    for fn in flat:
        r = fn["raw"]
        if r.get("type") in ("value","toggle") and r.get("persist", False):
            rid = r["id"]; 
            if rid in SKIP_PERSIST_IDS or rid not in offsets: continue
            bind = r["bind"]; o = offsets[rid]
            if o["stride"] == 0:
                h.append(f"  ee_read({o['base']}, this->{bind});")
            else:
                h.append(f"  for (int i=0;i<NUM_UNITS;i++) ee_read((uint16_t)EE_OFF_{rid.upper()}(i), this->{bind}[i]);")
    h.append("}")
    h.append("")

    # saveToEEPROM: stride==0 -> скаляр, иначе цикл по i
    h.append("void MenuState::saveToEEPROM(){")
    h.append("  ee_write(EE_SYS_OFF_MAGIC,   (uint32_t)EE_MAGIC);")
    h.append("  ee_write(EE_SYS_OFF_VERSION, (uint32_t)EE_VERSION);")
    for fn in flat:
        r = fn["raw"]
        if r.get("type") in ("value","toggle") and r.get("persist", False):
            rid = r["id"]; 
            if rid in SKIP_PERSIST_IDS or rid not in offsets: continue
            bind = r["bind"]; o = offsets[rid]
            if o["stride"] == 0:
                h.append(f"  ee_store_field({o['base']}, this->{bind});")
            else:
                h.append(f"  for (int i=0;i<NUM_UNITS;i++) ee_store_field((uint16_t)EE_OFF_{rid.upper()}(i), this->{bind}[i]);")
    h.append("}")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(h))

def emit_menu_eeprom_io(path_h, path_cpp):
    with open(path_h, "w", encoding="utf-8") as f:
        f.write(textwrap.dedent("""
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
"""))
    with open(path_cpp, "w", encoding="utf-8") as f:
        f.write("// Auto-generated helpers are templates in header; cpp kept for build systems.\n")

def emit_menu_data_cpp(path, flat, lang_order, offsets):
    """
    Генерирует menu_data.cpp.
    ВАЖНО: для per_controller-полей (scope=per_controller) ставим ee_offset=-1, ee_size=0,
    т.к. реальные адреса EEPROM рассчитываются через BASE/STRIDE в menu_bindings.*
    """
    with open(path, "w", encoding="utf-8") as f:
        f.write("// Auto-generated. Do not edit.\n")
        f.write("#include <stddef.h>\n")
        f.write("#include \"menu_ids.h\"\n")
        f.write("#include \"menu_types.h\"\n")
        f.write("#include \"menu_state.h\"\n\n")

        # Собираем уникальные имена колбэков
        on_changes = []
        on_invokes = []
        seen_chg, seen_inv = set(), set()
        for fn in flat:
            r = fn["raw"]
            if r["type"] in ("value", "toggle"):
                oc = r.get("on_change")
                if oc and oc not in seen_chg:
                    on_changes.append(oc); seen_chg.add(oc)
            elif r["type"] == "action":
                iv = r.get("on_invoke")
                if iv and iv not in seen_inv:
                    on_invokes.append(iv); seen_inv.add(iv)

        # Объявления колбэков с C-линковкой (совместимо с weak-заглушками)
        if on_changes or on_invokes:
            f.write('#ifdef __cplusplus\nextern "C" {\n#endif\n')
            for oc in on_changes: f.write(f"void {oc}(void*);\n")
            for iv in on_invokes: f.write(f"void {iv}(void);\n")
            f.write('#ifdef __cplusplus\n}\n#endif\n\n')

        f.write("extern MenuState menu;\n\n")
        f.write("const MenuItem g_menu[MENU__COUNT] = {\n")

        for i, fn in enumerate(flat):
            r = fn["raw"]
            scope = fn.get("scope", "per_controller")  # по умолчанию локальные
            eid = to_enum_id(r["id"])
            titles, units = node_title_unit_arrays(r, lang_order)
            tcode = MENU_TYPES[r["type"]]
            parent = fn["parent"]
            first_child = fn["first_child"]
            child_count = fn["child_count"]

            f.write(f"  [{i}] = {{\n")
            f.write(f"    MENU_{eid}, {{ {', '.join(titles)} }}, {{ {', '.join(units)} }},\n")
            f.write(f"    {tcode}, {parent}, {first_child}, {child_count},\n")

            if r["type"] in ("value", "toggle"):
                vtype = r.get("vtype") or ("bool" if r["type"] == "toggle" else None)
                _, vtc, _ = VTYPES[vtype]
                bind = r["bind"]
                minv = float(r.get("min", 0))
                maxv = float(r.get("max", 0))
                step = float(r.get("step", 1))
                # onch = r.get("on_change") or "NULL"
                onch = r.get("on_change") or "nullptr"
                apply = "true" if (r.get("apply") == "live") else "false"

                # Для UI всё равно нужен указатель на данные (глобальные или на [0] массива)
                f.write(
                    f"    {{ {{ NULL }}, "
                    f"{{ {vtc}, (void*)&menu.{bind}, {minv:.6g}, {maxv:.6g}, {step:.6g}, {onch}, {apply} }} }},\n"
                )

                # EEPROM-метаданные в MenuItem:
                # - global: кладём реальный оффсет/размер
                # - per_controller: в MenuItem не кладём оффсет; EEPROM делает menu_bindings по BASE/STRIDE
                if r.get("persist") and r["id"] in offsets:
                    o = offsets[r["id"]]
                    if o.get("stride", 0) == 0 or scope == "global":
                        off, size = o["base"], o["size"]
                    else:
                        off, size = -1, 0
                else:
                    off, size = -1, 0

                f.write(f"    {off}, {size}\n")
                f.write("  },\n")

            elif r["type"] == "action":
                # inv = r.get("on_invoke") or "NULL"
                inv = r.get("on_invoke") or "nullptr"
                f.write(f"    {{ {{ {inv} }}, {{ VT_F32, NULL, 0, 0, 0, NULL, false }} }},\n")
                f.write("    -1, 0\n")
                f.write("  },\n")

            else:  # submenu
                f.write("    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },\n")
                f.write("    -1, 0\n")
                f.write("  },\n")

        f.write("};\n")

def emit_menu_bindings_h(path, flat):
    binds = []
    for it in flat:
        r = it["raw"]
        if r.get("bind"):
            binds.append((r, it["scope"]))

    h = []
    h.append("// AUTO-GENERATED. DO NOT EDIT.")
    h.append("#pragma once")
    h.append("#include <stdint.h>")
    h.append("#include <stdbool.h>")
    h.append('#include "menu_types.h"')
    h.append('#include "menu_state.h"')
    h.append('#include "menu_eeprom.h"')
    h.append("")
    h.append("typedef void (*MenuOnChangeFn)(void* ctx);")
    h.append("")
    h.append("typedef enum { SCOPE_GLOBAL=0, SCOPE_PER_CONTROLLER=1 } MenuScope;")
    h.append("")
    h.append("typedef struct {")
    h.append("  uint16_t      id;         // MenuItem ID для отслеживания изменений")
    h.append("  const char*   bind;")
    h.append("  ValueType     vtype;")
    h.append("  void*         ptr;        // ptr на глобальное поле или на [0] массива")
    h.append("  bool          persist;")
    h.append("  MenuOnChangeFn on_change;")
    h.append("  bool          apply_live;")
    h.append("  MenuScope     scope;")
    h.append("  uint16_t      ee_base;    // базовый оффсет (global) или начало блока (per_controller)")
    h.append("  uint16_t      ee_stride;  // 0 для global, size-of-type для per_controller")
    h.append("} MenuBinding;")
    h.append("")
    h.append("extern const MenuBinding g_bindings[];")
    h.append("extern const uint16_t     g_bindings_count;")
    h.append("")
    h.append("// должен вернуть индекс активного контроллера [0..NUM_UNITS-1]; реализация находится в проекте")
    h.append("#ifdef __cplusplus")
    h.append('extern "C" {')
    h.append("#endif")
    h.append("uint8_t menu_get_active_controller(void);")
    h.append("#ifdef __cplusplus")
    h.append("}")
    h.append("#endif")
    h.append("")
    h.append("// Config change hook - вызывается после успешного применения любого изменения")
    h.append("typedef void (*ConfigChangeHookFn)(uint16_t itemId, uint8_t unit, const char* bind);")
    h.append("#ifdef __cplusplus")
    h.append('extern "C" {')
    h.append("#endif")
    h.append("extern ConfigChangeHookFn g_config_change_hook;")
    h.append("void menu_set_config_change_hook(ConfigChangeHookFn hook);")
    h.append("#ifdef __cplusplus")
    h.append("}")
    h.append("#endif")
    h.append("")
    h.append("bool menu_apply_by_bind(const char* bind, float v);")
    h.append("bool menu_read_by_bind(const char* bind, void* out_value);")
    h.append("const MenuBinding* menu_find_bind(const char* bind);")
    h.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write('\n'.join(h))

def emit_menu_bindings_cpp(path, flat, offsets):
    def vtype_enum(v):
        return {
            "float":"VT_F32","uint16":"VT_U16","uint8":"VT_U8",
            "int32":"VT_I32","bool":"VT_BOOL","uint32":"VT_U32"
        }[v]

    binds = []
    for it in flat:
        r = it["raw"]; scope = it["scope"]
        if r.get("bind"):
            binds.append((r, scope))

    cpp = []
    cpp.append("// AUTO-GENERATED. DO NOT EDIT.")
    cpp.append('#include <string.h>')
    cpp.append('#include "menu_bindings.h"')
    cpp.append('#include "menu_eeprom_io.h"')
    cpp.append("")
    cpp.append('#ifdef __cplusplus')
    cpp.append('extern "C" {')
    cpp.append('#endif')
    cpp.append("")

    declared_oc = set()
    for r, scope in binds:
        oc = r.get("on_change")
        if oc and oc not in declared_oc:
            cpp.append(f'// forward decl for optional on_change')
            cpp.append(f'void {oc}(void* ctx); // C++ linkage')
            declared_oc.add(oc)
    if declared_oc:
        cpp.append("#ifdef __cplusplus")
        cpp.append("}")
        cpp.append("#endif")
        cpp.append("")


    cpp.append("extern MenuState menu;")
    cpp.append("const MenuBinding g_bindings[] = {")
    for r, scope in binds:
        ve = vtype_enum(r["vtype"])
        persist = "true" if r.get("persist") else "false"
        ocp = r.get("on_change") or "nullptr"
        al  = "true" if (r.get("apply","") == "live") else "false"
        rid = r["id"]
        if r.get("persist") and rid in offsets:
            o = offsets[rid]
            ee_base   = o["base"]
            ee_stride = o["stride"]
        else:
            ee_base = 0
            ee_stride = 0
        scope_enum = "SCOPE_GLOBAL" if scope=="global" else "SCOPE_PER_CONTROLLER"
        menu_id_enum = f"MENU_{to_enum_id(rid)}"
        cpp.append(f'  {{{menu_id_enum}, "{r["bind"]}", {ve}, (void*)&menu.{r["bind"]}, {persist}, {ocp}, {al}, {scope_enum}, {ee_base}, {ee_stride}}},')
    cpp.append("};")
    cpp.append("const uint16_t g_bindings_count = sizeof(g_bindings)/sizeof(g_bindings[0]);")
    cpp.append("")
    cpp.append("// Global config change hook")
    cpp.append("ConfigChangeHookFn g_config_change_hook = nullptr;")
    cpp.append("")
    cpp.append("void menu_set_config_change_hook(ConfigChangeHookFn hook) {")
    cpp.append("  g_config_change_hook = hook;")
    cpp.append("}")
    cpp.append("")
    cpp.append("const MenuBinding* menu_find_bind(const char* bind) {")
    cpp.append("  if (!bind) return nullptr;")
    cpp.append("  for (uint16_t i=0;i<g_bindings_count;i++)")
    cpp.append("    if (strcmp(g_bindings[i].bind, bind) == 0) return &g_bindings[i];")
    cpp.append("  return nullptr;")
    cpp.append("}")
    cpp.append("")
    cpp.append("static inline void store_value(const MenuBinding& b, float v, uint8_t idx) {")
    cpp.append("  switch (b.vtype) {")
    cpp.append("    case VT_F32:  if (b.scope==SCOPE_GLOBAL) *(float*)b.ptr    = v; else ((float*)b.ptr)[idx]    = v; break;")
    cpp.append("    case VT_U16:  if (b.scope==SCOPE_GLOBAL) *(uint16_t*)b.ptr = (uint16_t)v; else ((uint16_t*)b.ptr)[idx] = (uint16_t)v; break;")
    cpp.append("    case VT_U8:   if (b.scope==SCOPE_GLOBAL) *(uint8_t*)b.ptr  = (uint8_t)v;  else ((uint8_t*)b.ptr)[idx]  = (uint8_t)v;  break;")
    cpp.append("    case VT_I32:  if (b.scope==SCOPE_GLOBAL) *(int32_t*)b.ptr  = (int32_t)v;  else ((int32_t*)b.ptr)[idx]  = (int32_t)v;  break;")
    cpp.append("    case VT_BOOL: if (b.scope==SCOPE_GLOBAL) *(bool*)b.ptr     = (bool)(v!=0.0f); else ((bool*)b.ptr)[idx]     = (bool)(v!=0.0f); break;")
    cpp.append("    case VT_U32:  if (b.scope==SCOPE_GLOBAL) *(uint32_t*)b.ptr = (uint32_t)v; else ((uint32_t*)b.ptr)[idx] = (uint32_t)v; break;")
    cpp.append("  }")
    cpp.append("}")
    cpp.append("")
    cpp.append("static inline void read_value(const MenuBinding& b, void* out_value, uint8_t idx) {")
    cpp.append("  switch (b.vtype) {")
    cpp.append("    case VT_F32:  *(float*)out_value    = (b.scope==SCOPE_GLOBAL)? *(float*)b.ptr    : ((float*)b.ptr)[idx]; break;")
    cpp.append("    case VT_U16:  *(uint16_t*)out_value = (b.scope==SCOPE_GLOBAL)? *(uint16_t*)b.ptr : ((uint16_t*)b.ptr)[idx]; break;")
    cpp.append("    case VT_U8:   *(uint8_t*)out_value  = (b.scope==SCOPE_GLOBAL)? *(uint8_t*)b.ptr  : ((uint8_t*)b.ptr)[idx]; break;")
    cpp.append("    case VT_I32:  *(int32_t*)out_value  = (b.scope==SCOPE_GLOBAL)? *(int32_t*)b.ptr  : ((int32_t*)b.ptr)[idx]; break;")
    cpp.append("    case VT_BOOL: *(bool*)out_value     = (b.scope==SCOPE_GLOBAL)? *(bool*)b.ptr     : ((bool*)b.ptr)[idx]; break;")
    cpp.append("    case VT_U32:  *(uint32_t*)out_value = (b.scope==SCOPE_GLOBAL)? *(uint32_t*)b.ptr : ((uint32_t*)b.ptr)[idx]; break;")
    cpp.append("  }")
    cpp.append("}")
    cpp.append("")
    cpp.append("static inline uint16_t calc_eeprom_offset(const MenuBinding& b, uint8_t idx){")
    cpp.append("  if (!b.persist) return 0;")
    cpp.append("  if (b.scope == SCOPE_GLOBAL) return b.ee_base;")
    cpp.append("  return (uint16_t)(b.ee_base + (uint16_t)idx * b.ee_stride);")
    cpp.append("}")
    cpp.append("")
    cpp.append("bool menu_read_by_bind(const char* bind, void* out_value) {")
    cpp.append("  const MenuBinding* b = menu_find_bind(bind);")
    cpp.append("  if (!b || !out_value) return false;")
    cpp.append("  uint8_t idx = 0;")
    cpp.append("  if (b->scope == SCOPE_PER_CONTROLLER) idx = menu_get_active_controller();")
    cpp.append("  read_value(*b, out_value, idx);")
    cpp.append("  return true;")
    cpp.append("}")
    cpp.append("")
    cpp.append("bool menu_apply_by_bind(const char* bind, float v) {")
    cpp.append("  const MenuBinding* b = menu_find_bind(bind);")
    cpp.append("  if (!b) return false;")
    cpp.append("  uint8_t idx = 0;")
    cpp.append("  if (b->scope == SCOPE_PER_CONTROLLER) idx = menu_get_active_controller();")
    cpp.append("  store_value(*b, v, idx);")
    cpp.append("  if (b->persist) {")
    cpp.append("    uint16_t ee = calc_eeprom_offset(*b, idx);")
    cpp.append("    switch (b->vtype) {")
    cpp.append("      case VT_F32:  ee_store_field<float>(   ee, (b->scope==SCOPE_GLOBAL)? *(float*)b->ptr    : ((float*)b->ptr)[idx]); break;")
    cpp.append("      case VT_U16:  ee_store_field<uint16_t>(ee, (b->scope==SCOPE_GLOBAL)? *(uint16_t*)b->ptr : ((uint16_t*)b->ptr)[idx]); break;")
    cpp.append("      case VT_U8:   ee_store_field<uint8_t>( ee, (b->scope==SCOPE_GLOBAL)? *(uint8_t*)b->ptr  : ((uint8_t*)b->ptr)[idx]); break;")
    cpp.append("      case VT_I32:  ee_store_field<int32_t>( ee, (b->scope==SCOPE_GLOBAL)? *(int32_t*)b->ptr  : ((int32_t*)b->ptr)[idx]); break;")
    cpp.append("      case VT_BOOL: ee_store_field<bool>(    ee, (b->scope==SCOPE_GLOBAL)? *(bool*)b->ptr     : ((bool*)b->ptr)[idx]); break;")
    cpp.append("      case VT_U32:  ee_store_field<uint32_t>(ee, (b->scope==SCOPE_GLOBAL)? *(uint32_t*)b->ptr : ((uint32_t*)b->ptr)[idx]); break;")
    cpp.append("    }")
    cpp.append("  }")
    cpp.append("  if (b->on_change) b->on_change((void*)b->ptr);")
    cpp.append("  if (g_config_change_hook) g_config_change_hook(b->id, idx, b->bind);")
    cpp.append("  return true;")
    cpp.append("}")
    cpp.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write('\n'.join(cpp))

def emit_menu_callbacks_weak_cpp(path, flat):
    on_changes, on_invokes = set(), set()
    for it in flat:
        r = it["raw"]
        oc = r.get("on_change")
        iv = r.get("on_invoke")
        if oc: on_changes.add(oc)
        if iv: on_invokes.add(iv)

    lines = []
    lines.append("// AUTO-GENERATED. DO NOT EDIT.")
    lines.append("// Weak stubs for menu callbacks so linking always succeeds.")
    lines.append("#include <Arduino.h>")
    lines.append("#ifdef __cplusplus")
    lines.append('extern "C" {')
    lines.append("#endif")

    for name in sorted(on_changes):
        lines.append(f"void {name}(void*) __attribute__((weak));")
        lines.append(f"void {name}(void*) {{ /* stub */ }}")

    for name in sorted(on_invokes):
        lines.append(f"void {name}(void) __attribute__((weak));")
        lines.append(f"void {name}(void) {{ /* stub */ }}")

    # слабая заглушка активного контроллера
    lines.append("uint8_t menu_get_active_controller(void) __attribute__((weak));")
    lines.append("uint8_t menu_get_active_controller(void) { return 0; }")

    lines.append("#ifdef __cplusplus")
    lines.append("} // extern \"C\"")
    lines.append("#endif")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

# ---------------------------
# ДОПОЛНИТЕЛЬНО: генерация menu_presets_autogen.h
# ---------------------------
def emit_menu_presets_h(path, flat):
    lines = []
    lines.append("// AUTO-GENERATED. DO NOT EDIT.")
    lines.append("#pragma once\n")

    seen = set()
    for fn in flat:
        raw = fn["raw"]
        if raw.get("type") != "submenu":
            continue
        pid = str(raw.get("id", ""))
        if not pid.startswith("preset_"):
            continue

        suffix = pid[len("preset_"):]
        bind_temp = None
        bind_time = None
        on_invoke = None

        for ch in raw.get("children", []):
            if not isinstance(ch, dict):
                continue
            ctype = ch.get("type")
            cid = str(ch.get("id", ""))
            if ctype == "value":
                b = ch.get("bind") or cid
                lb, lc = b.lower(), cid.lower()
                if lb.endswith("_temp") or lc.endswith("_temp"):
                    bind_temp = b
                elif lb.endswith("_time") or lc.endswith("_time"):
                    bind_time = b
            elif ctype == "action":
                on_invoke = ch.get("on_invoke")

        if not bind_temp:
            bind_temp = f"preset_{suffix}_temp"
        if not bind_time:
            bind_time = f"preset_{suffix}_time"
        if not on_invoke:
            on_invoke = f"start_drying_{suffix}"

        key = (on_invoke, bind_temp, bind_time)
        if key in seen:
            continue
        seen.add(key)

        title = suffix.upper().replace('_','-')
        lines.append(f"// {title}")
        lines.append(f"DEFINE_PRESET_START({on_invoke}, {bind_temp}, {bind_time})\n")

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

# ---------------------------
# LINK: генерация menu_meta.h (только метаданные, без указателей)
# ---------------------------
def calc_menu_serialized_max(flat, lang_order, num_units):
    """Расчёт верхней границы размера сериализованного JSON меню в байтах
    (UTF-8). Используется как MENU_SERIALIZED_MAX_SIZE для pre-allocated
    буфера в MenuPublisher (idryer-core/src/menu_publisher.h).

    Формат JSON задан в menu_buildFullJson:
        {"v":N,"menu":[{"id":I,"t":"sub","n":"…","p":P, …}, …]}

    Формула совпадает с idryer-core/menu/menu_gen.py — единый источник истины
    для размера сериализованного меню в обеих ветках генератора.
    """
    total = len('{"v":99999,"menu":[]}')  # ~21
    NUM_FLOAT_MAX = 12  # "-99999.99" с запасом

    items_total = 0
    for fn in flat:
        r = fn["raw"]
        scope = fn.get("scope", "per_controller")
        item_type = r.get("type", "submenu")

        item_size = len('{"id":99999,"t":"sub","p":-9999,')

        max_title = 0
        title_raw = r.get("title")
        if isinstance(title_raw, dict):
            for lang in lang_order:
                tval = title_raw.get(lang) or title_raw.get("en") or ""
                max_title = max(max_title, len(str(tval).encode("utf-8")))
        elif isinstance(title_raw, str):
            max_title = len(title_raw.encode("utf-8"))
        item_size += len('"n":"",') + max_title

        max_unit = 0
        unit_raw = r.get("unit")
        if isinstance(unit_raw, dict):
            for lang in lang_order:
                uval = unit_raw.get(lang) or unit_raw.get("en") or ""
                max_unit = max(max_unit, len(str(uval).encode("utf-8")))
        elif isinstance(unit_raw, str):
            max_unit = len(unit_raw.encode("utf-8"))
        if max_unit > 0:
            item_size += len('"u":"",') + max_unit

        role = r.get("role")
        if role:
            item_size += len('"r":"",') + len(str(role).encode("utf-8"))

        if item_type == "value":
            item_size += len('"min":,"max":,"step":,') + 3 * NUM_FLOAT_MAX

        if item_type in ("value", "toggle"):
            if scope == "global":
                item_size += len('"val":,') + NUM_FLOAT_MAX
            else:
                item_size += len('"val":[],') + (NUM_FLOAT_MAX + 1) * num_units

        item_size += len('},')
        items_total += item_size

    total += items_total
    return int(total * 1.15) + 256


def emit_menu_meta_h(path, flat, lang_order, num_units):
    """
    Генерирует menu_meta.h для ESP32 LINK.
    Содержит только статические метаданные меню без указателей на данные и callback'и.
    Используется для сборки JSON конфига из структуры + vals.
    """
    lines = []
    lines.append("// Auto-generated for ESP32 LINK. Do not edit.")
    lines.append("// Contains menu metadata only (no pointers to data or callbacks).")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <stdbool.h>")
    lines.append("")
    lines.append(f"#define MENU_META_COUNT {len(flat)}")
    lines.append(f"#define MENU_LANG_COUNT {len(lang_order)}")
    # Точный расчётный максимум сериализованного JSON меню (UTF-8 байт).
    # Используется как размер pre-allocated буфера в MenuPublisher
    # (idryer-core/src/menu_publisher.h).
    serialized_max = calc_menu_serialized_max(flat, lang_order, num_units)
    lines.append(f"#define MENU_SERIALIZED_MAX_SIZE {serialized_max}")
    lines.append("")

    # Enum для типов
    lines.append("typedef enum {")
    lines.append("    META_SUBMENU = 0,")
    lines.append("    META_ACTION = 1,")
    lines.append("    META_VALUE = 2,")
    lines.append("    META_TOGGLE = 3")
    lines.append("} MenuMetaType;")
    lines.append("")

    lines.append("typedef enum {")
    lines.append("    META_VT_F32 = 0,")
    lines.append("    META_VT_U16 = 1,")
    lines.append("    META_VT_U8 = 2,")
    lines.append("    META_VT_I32 = 3,")
    lines.append("    META_VT_BOOL = 4,")
    lines.append("    META_VT_U32 = 5")
    lines.append("} MenuMetaValueType;")
    lines.append("")

    lines.append("typedef enum {")
    lines.append("    META_SCOPE_GLOBAL = 0,")
    lines.append("    META_SCOPE_PER_UNIT = 1")
    lines.append("} MenuMetaScope;")
    lines.append("")

    # Структура метаданных
    lines.append("typedef struct {")
    lines.append(f"    uint16_t id;")
    lines.append(f"    const char* title[MENU_LANG_COUNT];")
    lines.append(f"    const char* unit[MENU_LANG_COUNT];")
    lines.append("    MenuMetaType type;")
    lines.append("    int16_t parent;")
    lines.append("    int16_t first_child;")
    lines.append("    uint16_t child_count;")
    lines.append("    // Value/Toggle fields (ignored for submenu/action)")
    lines.append("    MenuMetaValueType vtype;")
    lines.append("    float min_val;")
    lines.append("    float max_val;")
    lines.append("    float step;")
    lines.append("    MenuMetaScope scope;")
    lines.append("    const char* role;  // semantic role for portal (nullptr if not set)")
    lines.append("} MenuMeta;")
    lines.append("")

    # Массив данных
    lines.append("static const MenuMeta g_menu_meta[MENU_META_COUNT] = {")

    meta_types = {"submenu": "META_SUBMENU", "action": "META_ACTION", "value": "META_VALUE", "toggle": "META_TOGGLE"}
    meta_vtypes = {"float": "META_VT_F32", "uint16": "META_VT_U16", "uint8": "META_VT_U8",
                   "int32": "META_VT_I32", "bool": "META_VT_BOOL", "uint32": "META_VT_U32"}

    for i, fn in enumerate(flat):
        r = fn["raw"]
        scope = fn.get("scope", "per_controller")

        # Titles and units
        titles, units = node_title_unit_arrays(r, lang_order)

        # Type
        mtype = meta_types.get(r["type"], "META_SUBMENU")

        # Parent/children
        parent = fn["parent"]
        first_child = fn["first_child"]
        child_count = fn["child_count"]

        # Value fields
        if r["type"] in ("value", "toggle"):
            vtype = r.get("vtype") or ("bool" if r["type"] == "toggle" else "float")
            mvtype = meta_vtypes.get(vtype, "META_VT_F32")
            minv = float(r.get("min", 0))
            maxv = float(r.get("max", 0))
            step = float(r.get("step", 1))
        else:
            mvtype = "META_VT_F32"
            minv = 0.0
            maxv = 0.0
            step = 0.0

        mscope = "META_SCOPE_GLOBAL" if scope == "global" else "META_SCOPE_PER_UNIT"
        role_val = r.get("role")
        role_str = c_string(role_val) if role_val else "nullptr"

        lines.append(f"    // [{i}] {r['id']}")
        lines.append(f"    {{ {i}, {{ {', '.join(titles)} }}, {{ {', '.join(units)} }},")
        lines.append(f"      {mtype}, {parent}, {first_child}, {child_count},")
        lines.append(f"      {mvtype}, {c_float(minv)}, {c_float(maxv)}, {c_float(step)}, {mscope}, {role_str} }},")

    lines.append("};")
    lines.append("")

    # Helper функции
    lines.append("// Get menu item by id")
    lines.append("static inline const MenuMeta* menu_meta_get(uint16_t id) {")
    lines.append("    if (id < MENU_META_COUNT) return &g_menu_meta[id];")
    lines.append("    return nullptr;")
    lines.append("}")
    lines.append("")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

# ---------------------------
# LINK: генерация menu_cache.h (кэш значений для ESP32)
# ---------------------------
def emit_menu_cache_h(path, flat, num_units):
    """
    Генерирует menu_cache.h для ESP32 LINK.
    Содержит структуру для хранения текущих значений меню.
    """
    lines = []
    lines.append("// Auto-generated for ESP32 LINK. Do not edit.")
    lines.append("// Contains menu value cache (current values from MCU).")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <stdbool.h>")
    lines.append('#include "menu_meta.h"')
    lines.append("")
    lines.append(f"#define MENU_MAX_UNITS {num_units}")
    lines.append("")

    # Считаем сколько элементов с значениями (value/toggle)
    value_count = sum(1 for fn in flat if fn["raw"]["type"] in ("value", "toggle"))

    lines.append(f"// Total menu items: {len(flat)}, with values: {value_count}")
    lines.append("")

    # Структура MenuValue - хранит одно значение
    lines.append("// Значение одного элемента меню")
    lines.append("union MenuValue {")
    lines.append("    float    f32;")
    lines.append("    uint32_t u32;")
    lines.append("    int32_t  i32;")
    lines.append("    uint16_t u16;")
    lines.append("    uint8_t  u8;")
    lines.append("    bool     b;")
    lines.append("};")
    lines.append("")

    # Основная структура кэша
    lines.append("// Кэш значений меню для LINK")
    lines.append("class MenuCache {")
    lines.append("public:")
    lines.append("    uint16_t revision = 0;       // Ревизия конфига от MCU")
    lines.append("    uint8_t  active_unit = 0;    // Активный юнит (0..units_count-1)")
    lines.append("    uint8_t  units_count = 1;    // Количество юнитов")
    lines.append("    uint8_t  lang = 0;           // Текущий язык (0=ru, 1=en)")
    lines.append("")
    lines.append("    // Значения: [menu_id][unit_index]")
    lines.append("    // Для global элементов используется только [id][0]")
    lines.append("    MenuValue values[MENU_META_COUNT][MENU_MAX_UNITS] = {};")
    lines.append("")
    lines.append("    // Получить значение как float")
    lines.append("    float getFloat(uint16_t id, uint8_t unit = 255) const {")
    lines.append("        if (id >= MENU_META_COUNT) return 0.0f;")
    lines.append("        const MenuMeta* m = &g_menu_meta[id];")
    lines.append("        uint8_t u = (unit == 255) ? active_unit : unit;")
    lines.append("        if (m->scope == META_SCOPE_GLOBAL) u = 0;")
    lines.append("        if (u >= MENU_MAX_UNITS) u = 0;")
    lines.append("        const MenuValue& v = values[id][u];")
    lines.append("        switch (m->vtype) {")
    lines.append("            case META_VT_F32:  return v.f32;")
    lines.append("            case META_VT_U32:  return (float)v.u32;")
    lines.append("            case META_VT_I32:  return (float)v.i32;")
    lines.append("            case META_VT_U16:  return (float)v.u16;")
    lines.append("            case META_VT_U8:   return (float)v.u8;")
    lines.append("            case META_VT_BOOL: return v.b ? 1.0f : 0.0f;")
    lines.append("            default: return 0.0f;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    // Установить значение из float")
    lines.append("    void setFloat(uint16_t id, float val, uint8_t unit = 255) {")
    lines.append("        if (id >= MENU_META_COUNT) return;")
    lines.append("        const MenuMeta* m = &g_menu_meta[id];")
    lines.append("        uint8_t u = (unit == 255) ? active_unit : unit;")
    lines.append("        if (m->scope == META_SCOPE_GLOBAL) u = 0;")
    lines.append("        if (u >= MENU_MAX_UNITS) u = 0;")
    lines.append("        MenuValue& v = values[id][u];")
    lines.append("        switch (m->vtype) {")
    lines.append("            case META_VT_F32:  v.f32 = val; break;")
    lines.append("            case META_VT_U32:  v.u32 = (uint32_t)val; break;")
    lines.append("            case META_VT_I32:  v.i32 = (int32_t)val; break;")
    lines.append("            case META_VT_U16:  v.u16 = (uint16_t)val; break;")
    lines.append("            case META_VT_U8:   v.u8  = (uint8_t)val; break;")
    lines.append("            case META_VT_BOOL: v.b   = (val != 0.0f); break;")
    lines.append("            default: break;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    // Получить bool значение")
    lines.append("    bool getBool(uint16_t id, uint8_t unit = 255) const {")
    lines.append("        return getFloat(id, unit) != 0.0f;")
    lines.append("    }")
    lines.append("")
    lines.append("    // Получить int значение")
    lines.append("    int32_t getInt(uint16_t id, uint8_t unit = 255) const {")
    lines.append("        return (int32_t)getFloat(id, unit);")
    lines.append("    }")
    lines.append("")
    lines.append("    // Геттеры для lang и units_count")
    lines.append("    uint8_t getLang() const { return lang; }")
    lines.append("    uint8_t getUnitsCount() const { return units_count; }")
    lines.append("};")
    lines.append("")
    lines.append("// Глобальный экземпляр кэша")
    lines.append("extern MenuCache g_menu_cache;")
    lines.append("")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

def emit_menu_cache_cpp(path):
    """
    Генерирует menu_cache.cpp - определение глобального экземпляра кэша.
    """
    lines = []
    lines.append("// Auto-generated for ESP32 LINK. Do not edit.")
    lines.append('#include "menu_cache.h"')
    lines.append("")
    lines.append("// Глобальный экземпляр кэша значений меню")
    lines.append("MenuCache g_menu_cache;")
    lines.append("")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

# ---------------------------
# main
# ---------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("yaml_path", help="menu.yaml")
    ap.add_argument("--out", default=os.path.dirname(os.path.dirname(__file__)), help="output dir (menu/)")
    ap.add_argument("--magic", type=lambda x:int(x,0), default=0x4D4E5551, help="EE magic (default 0x4D4E5551)")
    ap.add_argument("--version", type=int, default=1, help="EE version int (default); major читается из version.h VERSION_MAJOR")
    ap.add_argument("--num-units", type=int, default=3, help="Количество контроллеров (NUM_UNITS)")
    args = ap.parse_args()

    with open(args.yaml_path, "r", encoding="utf-8") as fh:
        data = yaml.safe_load(fh)
    roots = data if isinstance(data, list) else [data]

    langs=set(); collect_langs(roots, langs)
    lang_order = infer_lang_order(langs)

    flat = flatten(roots, -1, [])
    offsets, total = eeprom_layout(flat, args.num_units)

    outdir = args.out
    os.makedirs(outdir, exist_ok=True)

    emit_menu_ids_h(os.path.join(outdir,"menu_ids.h"), flat, lang_order)
    emit_menu_types_h(os.path.join(outdir,"menu_types.h"))

    ee_major = read_version_major(default=args.version)
    emit_menu_eeprom_h(os.path.join(outdir,"menu_eeprom.h"), flat, offsets, total, args.magic, ee_major, args.num_units)
    
    emit_menu_state_h(os.path.join(outdir,"menu_state.h"), flat, args.num_units)
    emit_menu_state_cpp(os.path.join(outdir,"menu_state.cpp"), flat, offsets)

    emit_menu_eeprom_io(os.path.join(outdir,"menu_eeprom_io.h"), os.path.join(outdir,"menu_eeprom_io.cpp"))
    emit_menu_data_cpp(os.path.join(outdir,"menu_data.cpp"), flat, lang_order, offsets)

    emit_menu_bindings_h(os.path.join(outdir,"menu_bindings.h"), flat)
    emit_menu_bindings_cpp(os.path.join(outdir,"menu_bindings.cpp"), flat, offsets)
    emit_menu_callbacks_weak_cpp(os.path.join(outdir, "menu_callbacks_weak.cpp"), flat)

    emit_menu_presets_h(os.path.join(outdir, "menu_presets_autogen.h"), flat)

    # LINK: menu_meta.h (только метаданные для ESP32)
    emit_menu_meta_h(os.path.join(outdir, "menu_meta.h"), flat, lang_order, args.num_units)

    # LINK: menu_cache.h/.cpp (кэш значений для ESP32)
    emit_menu_cache_h(os.path.join(outdir, "menu_cache.h"), flat, args.num_units)
    emit_menu_cache_cpp(os.path.join(outdir, "menu_cache.cpp"))

    print(f"Generated in {outdir}:")
    print("  menu_ids.h, menu_types.h, menu_eeprom.h, menu_state.h/.cpp, menu_eeprom_io.h/.cpp, menu_data.cpp,")
    print("  menu_bindings.h/.cpp, menu_callbacks_weak.cpp, menu_presets_autogen.h")
    print("  menu_meta.h, menu_cache.h (LINK)")
    print(f"EEPROM total size: {total} bytes (incl. {EE_SYS_AREA_SIZE} bytes for SYS AREA)")
    # print(f"EEPROM total size: {total} bytes (incl. SYS={EE_SYS_AREA_SIZE}, ERRLOG={ERRLOG_HDR_SIZE + ERRLOG_REC_SIZE * ERRLOG_CAP})")
    print(f"EEPROM total size: {total} bytes (incl. SYS={EE_SYS_AREA_SIZE + EE_SYS_CAL_SIZE +  EE_SCALE_TEMP_SIZE}, ERRLOG={ERRLOG_HDR_SIZE + ERRLOG_REC_SIZE * ERRLOG_CAP})")
    print(f"NUM_UNITS = {args.num_units}; scope: global/per_controller; SKIP_PERSIST_IDS = {sorted(SKIP_PERSIST_IDS)}")

if __name__ == "__main__":
    main()
