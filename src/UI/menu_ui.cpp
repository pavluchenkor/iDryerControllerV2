// ----------------------------------------------------------------------------
// Файл: menu_ui_impl.cpp — замена исходника.
// Назначение: Рендер и логика многоуровневого меню на базе U8G2.
// ВАЖНО: ЛОГИКА КОДА НЕ МЕНЯЛАСЬ - добавлены только ПОДРОБНЫЕ КОММЕНТАРИИ.
//
// ────────────────────────────────────────────────────────────────────────────
// ГЛОССАРИЙ И РАСШИФРОВКИ ТЕРМИНОВ (из menu_ui.h и контекста проекта)
//
//  • g_menu[]           - глобальный массив элементов меню (MenuItem), длиной MENU__COUNT.
//                         Каждый элемент содержит:
//                           - id          (MenuId)       : логический идентификатор пункта
//                           - parent      (int16_t)      : индекс родителя в g_menu или -1, если корень
//                           - type        (enum MenuType): тип пункта (см. MN_* ниже)
//                           - title[Lang] (const char*)  : локализованные заголовки ("RU", "EN", ...)
//                           - unit[Lang]  (const char*)  : локализованные единицы измерения (например, "°C")
//                           - u           (union)        : полезная нагрузка. Для ACTION - колбэк,
//                                                         для VALUE - ValueSpec (см. ниже).
//
//  • MenuType (псевдо):      // типы пунктов меню
//      MN_SUBMENU - пункт, у которого есть дети (подменю).
//      MN_ACTION  - действие: при enter вызывает функцию it.u.action.invoke().
//      MN_VALUE   - редактируемое числовое значение (min/max/step, формат справа).
//      MN_TOGGLE  - булевый переключатель (вкл/выкл), форматируется как On/Off или Вкл/Выкл.
//
//  • ValueSpec (псевдо):     // спецификация редактируемого значения для MN_VALUE/MN_TOGGLE
//      struct ValueSpec {
//        void*   ptr;      // указатель на "живую" переменную (float/u16/u8/i32/bool), которую меняем
//        float   minv;     // минимально допустимое значение (для целочисленных тоже хранится как float)
//        float   maxv;     // максимально допустимое значение
//        float   step;     // шаг изменения при "щёлке" энкодера (знак берётся из dir)
//        void  (*on_change)(void* p); // необязательный колбэк, вызывается после изменения (*p)
//        ValueType vtype;  // см. VT_* ниже
//      };
//
//  • ValueType (псевдо):     // тип редактируемого значения
//      VT_F32  - значение float  (32-бит с плавающей точкой)
//      VT_U16  - значение uint16_t
//      VT_U8   - значение uint8_t
//      VT_I32  - значение int32_t
//      VT_BOOL - значение bool   (переключатель)
//
//  • MenuId: логический ID пункта (например, MENU_ROOT - корень дерева меню).
//
//  • LangId: идентификатор языка (например, LANG_EN, LANG_RU). Используется для выбора title/unit.
//
//  • Поля/методы класса MenuUI (псевдо-обзор):
//      - U8G2* u8;                     // связь с U8G2 для рисования
//      - LangId lang;                  // текущий язык
//      - uint8_t lines;                // сколько строк показываем на экране
//      - uint8_t padX, padY;           // отступы слева/сверху
//      - uint8_t lineH;                // пиксельная высота строки
//      - int16_t currentParent;        // индекс текущего "родителя" в g_menu (какой список детей показываем)
//      - int16_t cursor;               // индекс текущего выбранного пункта в g_menu
//      - uint8_t scroll;               // смещение первой видимой строки среди детей
//      - bool editMode;                // редактируем ли прямо сейчас значение?
//      - float    backupValF;          // бэкап float при редактировании (для отмены)
//      - int32_t  backupValI;          // бэкап целочисленных/булевых (в общий слот)
//      - методы displayW()/displayH()  // размеры экрана (ширина/высота), обычно обёртки над u8g2
//      - метод const MenuItem& N()     // "текущий" пункт (обычно g_menu[cursor])
//
//  • Поведение:
//      - Навигация по дереву: currentParent задаёт "какой список детей мы листаем". Курсор всегда указывает
//        на КОНКРЕТНЫЙ элемент g_menu (не "позицию" в списке), поэтому используются функции "сопоставления"
//        между «позиция среди детей» ↔ «индекс в g_menu» (childAtPos / posOfChild).
//      - Отрисовка: слева - локализованный заголовок, справа - отформатированное значение (если это VALUE/TOGGLE).
//      - Редактирование: вращение энкодера → applyDelta(dir), click → выйти из редактирования, back → отмена.
//
// ────────────────────────────────────────────────────────────────────────────
// МЕЛКИЕ НЮАНСЫ ЭТОГО ФАЙЛА
//  • Дети "не подряд" - индексы детей одного родителя могут быть разбросаны по g_menu.
//    Поэтому мы сканируем ВСЕ пункты при поиске детей (O(N)). Это просто и надёжно.
//  • Скролл - целочисленный "верх окна" видимых строк; ensureVisible_any() его корректирует.
//  • Для VT_BOOL любое ненулевое dir инвертирует значение (логика "щёлк - flip").
// ----------------------------------------------------------------------------


// MenuUI
//  ├─ u8            -> дисплей U8G2 (куда рисуем)
//  ├─ lang          -> текущий язык
//  ├─ lines         -> сколько строк показываем (например, 4)
//  ├─ padX,padY     -> отступы
//  ├─ lineH         -> высота строки (в пикселях)
//  ├─ currentParent -> ИНДЕКС родителя, чьих детей листаем (g_menu[index])
//  ├─ cursor        -> ИНДЕКС пункта, который сейчас выделен (в g_menu)
//  ├─ scroll        -> «с какой строки» показываем детей (вершина окна)
//  ├─ editMode      -> редактируем значение или просто ходим?
//  ├─ backupValF/I  -> бэкап значения на время редактирования (для отмены)

//  g_menu[]: массив MenuItem
//   ├─ id         (логический ID)
//   ├─ parent     (индекс родителя в g_menu или -1 для корня)
//   ├─ type       (MN_SUBMENU / MN_ACTION / MN_VALUE / MN_TOGGLE)
//   ├─ title[Lang], unit[Lang]
//   └─ u          (для ACTION: function, для VALUE/TOGGLE: ValueSpec)
//       ValueSpec:
//         ptr      -> указатель на живую переменную
//         min,max,step
//         vtype    -> VT_F32 / VT_U16 / VT_U8 / VT_I32 / VT_BOOL
//         on_change(optional)

// Энкодер крутят → handleInput
//     ├─ encoderDelta ≠ 0 → moveCursor(step)
//     │     ├─ считаем позицию курсора среди детей currentParent  (posOfChild)
//     │     ├─ делаем шаг по кругу (вверх/вниз)
//     │     ├─ позицию → индекс реального элемента (childAtPos) → cursor
//     │     └─ ensureVisible_any → подправить scroll, чтобы cursor был видим
//     ├─ click → enter()
//     │     ├─ если SUBMENU → currentParent = cursor; cursor = первый ребёнок
//     │     ├─ если ACTION  → вызвать it.u.action.invoke()
//     │     └─ если VALUE/TOGGLE → startEdit() (вход в редактирование)
//     └─ goBack → back()
//           └─ поднимаемся к родителю currentParent
//              (cursor становится на заголовок того подменю, из которого вышли)

// Энкодер крутят → handleInput
//     ├─ encoderDelta ≠ 0 → applyDelta(dir)
//     │     ├─ читаем ptr и тип (vtype)
//     │     ├─ считаем новое значение = старое + dir*step (или flip для BOOL)
//     │     ├─ ограничиваем в [min..max]
//     │     └─ если изменилось → пишем по указателю + вызываем on_change()
//     ├─ click   → выходим из editMode (сохраняем как есть)
//     └─ goBack  → cancelEdit() (откат к backup, выйти из editMode)

// draw()
//  ├─ clearBuffer()
//  ├─ считаем: par = g_menu[currentParent]
//  │   └─ cnt = сколько у него детей (childCountOf)
//  ├─ если детей нет → рисуем одиночный пункт (drawRow(row=0, cursor, selected=true))
//  └─ иначе
//      ├─ для i = 0..lines-1:
//      │     pos = scroll + i
//      │     idx = childAtPos(currentParent, pos)
//      │     drawRow(i, idx, idx == cursor)
//      └─ sendBuffer()

//  drawRow(row, idx, selected)
//    ├─ title = it.title[lang] (с фоллбэком на EN)
//    ├─ если selected → рисуем тёмный прямоугольник на всю строку (инверсия)
//    ├─ слева: drawUTF8(title)
//    ├─ справа: formatValue(it, buf)  // для VALUE/TOGGLE; у SUBMENU/ACTION пусто
//    │     └─ формат в зависимости от vtype (число + единица измерения, или On/Off)
//    └─ drawUTF8(buf по правому краю)

// Мини-карта данных и переходов
// BEGIN:
//   currentParent = ROOT
//   cursor = первый ребёнок ROOT
//   scroll = 0

// [Редактирование OFF]
//   крутилка:
//     posOfChild → +1/-1 по кругу → childAtPos → cursor
//     ensureVisible_any → корректируем scroll
//   click:
//     SUBMENU → currentParent = cursor; cursor = 1-й ребёнок
//     ACTION  → invoke()
//     VALUE/TOGGLE → startEdit()
//   back:
//     currentParent = parent(currentParent)
//     cursor = тот подменю, из которого вышли

// [Редактирование ON]
//   крутилка: applyDelta(dir) → меняем *ptr
//   click:    выходим из редактирования (значение остаётся)
//   back:     cancelEdit() → откат к backup → выходим

// Почему это работает «без математики по пикселям»

// 	•	Если шрифт моноширинный - можно позиционировать правую часть по количеству
//   символов, не вычисляя getUTF8Width.
//   Сейчас сделано по ширине в пикселях (красиво для пропорционального шрифта).
// 	•	Алгоритм работы с детьми не требует «позиций на экране» - он оперирует
//   индексами в g_menu и функциями «позиция ↔ индекс».
//   Отрисовка - это уже просто «нарисуй N строк, начиная со scroll».


// Памятка-коротыш
// 	•	currentParent - каких детей показываем
// 	•	cursor - кто выделен (индекс в g_menu)
// 	•	scroll - с какой строки детей рисуем
// 	•	editMode=false → кручу список; editMode=true → кручу значение
// 	•	enter():
// 	•	SUBMENU → «вниз»
// 	•	ACTION  → выполнить
// 	•	VALUE/TOGGLE → режим редактирования
// 	•	back() всегда «вверх» по дереву
// 	•	applyDelta() меняет *ptr и ограничивает min/max
// 	•	formatValue() делает красивый текст справа


#include "menu_ui.h"
#include "hardware/port_config.h"
#include <Arduino.h>

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_MENU
#define LOG_TAG "MENU"
#include "debug_log.h"

#ifndef MENU_DEBUG
#define MENU_DEBUG 1
#endif
#if MENU_DEBUG
#define MDBG(...)                                                                                                                                                                                      \
  do {                                                                                                                                                                                                 \
    Serial.printf(__VA_ARGS__);                                                                                                                                                                        \
  } while (0) // печать отладочных сообщений
#else
#define MDBG(...)                                                                                                                                                                                      \
  do {                                                                                                                                                                                                 \
  } while (0) // заглушка, когда отладка выключена
#endif

extern "C" uint8_t menu_get_active_controller(void);

// ---------- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ДЕТЕЙ НЕПОДРЯД ----------
// Эти три функции - мост между "позициями" внутри списка детей и реальными индексами g_menu.

// Подсчитать сколько у parent детей в g_menu (пробегает весь массив).
static uint16_t childCountOf(int16_t parent) {
  if (parent < 0) return 0; // отрицательный parent - невалидный
  uint16_t n = 0;
  for (int i = 0; i < MENU__COUNT; ++i)
    if (g_menu[i].parent == parent) ++n; // просто считаем совпадения parent
  return n;
}

// Вернуть индекс g_menu-элемента по позиции среди детей (0..cnt-1).
// Мы идём по всему g_menu, "перескакивая" только по совпадениям parent.
// Когда pos==0 для очередного совпадения - это и есть нужный ребёнок.
static int16_t childAtPos(int16_t parent, uint16_t pos) {
  if (parent < 0) return -1;
  for (int i = 0; i < MENU__COUNT; ++i) {
    if (g_menu[i].parent == parent) {
      if (pos == 0) return i; // нашли нужную позицию среди детей
      --pos;                  // иначе "отсчитываем" позицию дальше
    }
  }
  return -1; // позиция вне диапазона
}

// Вернуть порядковую позицию (0..cnt-1) ребёнка childIdx среди детей parent.
// Снова сканируем g_menu и считаем только совпадения по .parent.
static int16_t posOfChild(int16_t parent, int16_t childIdx) {
  if (parent < 0 || childIdx < 0) return -1;
  int16_t pos = 0; // текущая позиция среди детей
  for (int i = 0; i < MENU__COUNT; ++i) {
    if (g_menu[i].parent == parent) {
      if (i == childIdx) return pos; // дошли до нужного индекса g_menu
      ++pos;                         // считаем только детей (по parent)
    }
  }
  return -1; // не является ребёнком данного parent
}

// ensureVisible с учётом непоследовательных детей.
// Гарантирует, что позиция курсора попадает в окно видимых строк [scroll .. scroll+lines-1].
static void ensureVisible_any(const int16_t parentIndex, const int16_t cursor, uint8_t lines, uint8_t &scroll) {
  if (parentIndex < 0) {
    scroll = 0;
    return;
  }                                         // невалидный родитель - сбрасываем прокрутку
  uint16_t cnt = childCountOf(parentIndex); // количество детей
  if (cnt == 0) {
    scroll = 0;
    return;
  }                                              // детей нет - нечего скроллить
  int16_t pos = posOfChild(parentIndex, cursor); // позиция курсора среди детей
  if (pos < 0) {
    scroll = 0;
    return;
  } // курсор не ребёнок parent - сброс

  // Если курсор выше текущего окна - подтягиваем scroll вверх.
  if (pos < scroll) scroll = pos;
  // Если курсор ниже окна - сдвигаем scroll так, чтобы курсор оказался последней видимой строкой.
  else if ((uint16_t)pos >= (uint16_t)scroll + lines) scroll = pos - (lines - 1);
}

// ────────────────────────────────────────────────────────────────────────────
// Утилита для аккуратной обрезки UTF‑8 строки по количеству символов (не байт).
// Копирует символы целиком (1..4 байта), пока не исчерпаны maxChars или outN.
// Не рвёт многобайтные последовательности; добавляет '\0', если есть место.
// ────────────────────────────────────────────────────────────────────────────
static void utf8Trunc(const char *in, int maxChars, char *out, size_t outN) {
  int count = 0; // сколько символов скопировано
  size_t i = 0;  // позиция чтения в исходной строке (в байтах)
  while (in[i] && count < maxChars && outN > 1) {
    uint8_t c = (uint8_t)in[i];
    size_t step = 1; // длина текущего UTF‑8 символа (в байтах)
    // Определяем длину последовательности по ведущим битам:
    if ((c & 0x80) == 0x00) step = 1;      // ASCII (1 байт)
    else if ((c & 0xE0) == 0xC0) step = 2; // 110xxxxx (2 байта)
    else if ((c & 0xF0) == 0xE0) step = 3; // 1110xxxx (3 байта)
    else if ((c & 0xF8) == 0xF0) step = 4; // 11110xxx (4 байта)

    // Проверка границ входа/выхода - нельзя выходить за пределы.
    if (i + step > strlen(in) || step >= outN) break;

    memcpy(out, in + i, step); // копируем символ целиком
    out += step;               // двигаем выходной указатель
    outN -= step;              // уменьшаем доступный размер
    i += step;                 // двигаем входной указатель
    count++;                   // увеличиваем число символов
  }
  if (outN) *out = 0; // завершаем строку нулём (если осталось место)
}

// Перевод булева в «On/Off» или «Вкл/Выкл» по текущему языку.
static inline const char *tr_bool(bool v, LangId l) {
  // return (l == LANG_EN) ? (v ? "On" : "Off") : (v ? "Вкл" : "Выкл");
  return v ? "+" : "-";
}

// ----------------- MenuUI -----------------

// Поиск индекса элемента в g_menu по логическому идентификатору (MenuId).
int16_t MenuUI::findIndexById(MenuId id) const {
  for (int16_t i = 0; i < MENU__COUNT; ++i)
    if (g_menu[i].id == id) return i; // нашли - возвращаем индекс
  return -1;                          // иначе - не найден
}

const char *MenuUI::getItemName(int16_t idx) {
  if (idx < 0) return "";
  const MenuItem &it = g_menu[idx];
  LangId L = curLang();
  return it.title[L] ? it.title[L] : it.title[LANG_EN];
}

// Настройка шрифта и расчёт высоты строки так, чтобы уместить все lines на экране.
void MenuUI::setFont() {
  u8->setFont(u8g2_font_unifont_t_cyrillic);                  // универсальный шрифт с кириллицей
  u8->setFontMode(0);                                         // режим рендера без наложений
  u8->setFontPosTop();                                        // координата Y - верхняя кромка глифа
  lineH = u8->getMaxCharHeight();                             // базовая высота строки по шрифту
  if (lineH * lines > displayH()) lineH = displayH() / lines; // ужимаем строку при необходимости
}

// void MenuUI::setFont() {
//   u8->setFont(u8g2_font_unifont_t_cyrillic);
//   u8->setFontMode(0);
//   u8->setFontPosTop();
//   lineH = u8->getMaxCharHeight();
//   // учтём +1 строку на шапку
//   while ((uint16_t)lineH * (uint16_t)(lines + 1) > displayH() && lineH > 1) {
//     lineH--;
//   }
// }

// Инициализация UI: привязка дисплея, языка, установка корня,
// переход курсора на первого ребёнка и корректный scroll.
void MenuUI::begin(U8G2 *u8in, const uint8_t *langPtr) {
  u8 = u8in;
  lang_ptr_ = langPtr; // теперь UI будет читать язык из *lang_ptr_
  editMode = false;

  setFont(); // применяем шрифт/lineH

  // Находим корень по логическому id MENU_ROOT.
  int16_t root = findIndexById(MENU_ROOT);
  if (root < 0) root = 0; // если не нашли - fallback на 0
  currentParent = root;   // теперь листаем детей корня

  // Курсор ставим на первого РЕАЛЬНОГО ребёнка (по .parent), а не "по диапазону".
  uint16_t cnt = childCountOf(currentParent);
  cursor = (cnt > 0) ? childAtPos(currentParent, 0) : currentParent;

  // Сбрасываем прокрутку и обеспечиваем, чтобы курсор был виден.
  scroll = 0;
  ensureVisible_any(currentParent, cursor, lines, scroll);
}

// Программный прыжок к пункту по MenuId.
// enterChild=true и тип - подменю → входим внутрь и ставим курсор на первого ребёнка.
// иначе - делаем этот пункт выделенным в его родителе.
void MenuUI::jumpTo(MenuId id, bool enterChild) {
  MDBG("jumpTo: id=%d child=%d\n", (int)id, (int)enterChild);
  int16_t idx = findIndexById(id);
  if (idx < 0) return; // нет такого id

  if (enterChild && g_menu[idx].type == MN_SUBMENU) {
    currentParent = idx; // теперь этот пункт - родитель
    uint16_t cnt = childCountOf(currentParent);
    cursor = (cnt > 0) ? childAtPos(currentParent, 0) : currentParent; // на первого ребёнка
  } else {
    currentParent = (g_menu[idx].parent >= 0) ? g_menu[idx].parent : idx; // родитель или сам
    cursor = idx;                                                         // выделяем сам пункт
  }

  scroll = 0;
  ensureVisible_any(currentParent, cursor, lines, scroll); // корректируем видимость
}

// Универсальный обработчик событий ввода.
// Если в editMode - обрабатываем изменение значения/выход/отмену.
// Если не в editMode - перемещение курсора / вход / шаг назад по дереву.
void MenuUI::handleInput(int8_t encoderDelta, bool click, bool goBack) {
  // if (encoderDelta != 0 || click || goBack) {
  //   MDBG("in: d=%d click=%d back=%d edit=%d cur=%d parent=%d\n",
  //        (int)encoderDelta, (int)click, (int)goBack,
  //        (int)editMode, (int)cursor, (int)currentParent);
  // }

  if (editMode) {
    if (encoderDelta) applyDelta(encoderDelta); // вращение → изменить значение
    if (click) {                                // клик → выйти из редактирования (сохранить)
      if (save_cb_) save_cb_();                 // сохраняем в EEPROM
      MDBG("Save changes");
      // port_mode: значение сохранено в EEPROM и в menu.port*_mode (для отображения).
      // Железо работает по снапшоту (g_hw_port*), который не меняется до ребута.
      editMode = false;
    }
    if (goBack) cancelEdit(); // назад → отменить (восстановить backup)
    return;
  }
  if (encoderDelta) moveCursor(encoderDelta); // не редактируем - двигаем курсор
  if (click) enter();                         // клик → "войти" / "начать редактировать"
  if (goBack) back();                         // назад → уровень вверх
}

// Переместить курсор среди детей текущего родителя (циклически).
void MenuUI::moveCursor(int8_t step) {
  const MenuItem &par = g_menu[currentParent];
  if (par.type != MN_SUBMENU) return; // если текущий - не подменю, двигаться некуда

  uint16_t cnt = childCountOf(currentParent);
  if (cnt == 0) return; // у подменю нет детей

  int16_t pos = posOfChild(currentParent, cursor); // позиция курсора среди детей
  if (pos < 0) pos = 0;                            // страховка

  // Циклический переход: >0 - вниз на 1, ≤0 - вверх на 1
  int32_t np = (int32_t)pos + (step > 0 ? 1 : -1);
  if (np < 0) np = cnt - 1;
  if ((uint32_t)np >= cnt) np = 0;

  // Новая позиция → реальный индекс в g_menu
  cursor = childAtPos(currentParent, (uint16_t)np);

  // Держим курсор в видимой области
  ensureVisible_any(currentParent, cursor, lines, scroll);
}

// Обработчик "enter":
//   • SUBMENU → стать новым parent и перейти к первому ребёнку
//   • ACTION  → выполнить колбэк invoke()
//   • VALUE/TOGGLE → войти в режим редактирования
void MenuUI::enter() {
  const MenuItem &it = g_menu[cursor];
  // MDBG("enter: cur=%d id=%d type=%d\n", (int)cursor, (int)it.id, (int)it.type);

  if (it.type == MN_SUBMENU) {
    currentParent = cursor; // проваливаемся в подменю
    uint16_t cnt = childCountOf(currentParent);
    cursor = (cnt > 0) ? childAtPos(currentParent, 0) : currentParent; // курсор - на 1-го ребёнка
    scroll = 0;
    ensureVisible_any(currentParent, cursor, lines, scroll);
  } else if (it.type == MN_ACTION) {
    if (it.u.action.invoke) it.u.action.invoke(); // выполнить действие
  } else if (it.type == MN_VALUE || it.type == MN_TOGGLE) {
    startEdit(); // начать редактирование значения
  }
}

// Шаг "назад" по дереву: подняться к родителю currentParent, а курсор
// поставить на заголовок подменю, из которого вышли.
void MenuUI::back() {
  int16_t parentOfCurrent = g_menu[currentParent].parent;
  MDBG("back: parent=%d pOfP=%d\n", (int)currentParent, (int)parentOfCurrent);
  if (parentOfCurrent >= 0) {
    cursor = currentParent;          // курсор - на заголовке уходящего подменю
    currentParent = parentOfCurrent; // поднимаемся на уровень вверх
    ensureVisible_any(currentParent, cursor, lines, scroll);
  }
}


void MenuUI::startEdit() {
  const MenuItem &it = N();
  if (!(it.type == MN_VALUE || it.type == MN_TOGGLE)) return;

  switch (it.u.value.vtype) {
  case VT_F32: {
    float v = 0.f;
    if (ui_read_from_menuitem(it, &v)) backupValF = v;
  } break;
  case VT_U16: {
    uint16_t v = 0;
    if (ui_read_from_menuitem(it, &v)) backupValI = (int32_t)v;
  } break;
  case VT_U8: {
    uint8_t v = 0;
    if (ui_read_from_menuitem(it, &v)) backupValI = (int32_t)v;
  } break;
  case VT_I32: {
    int32_t v = 0;
    if (ui_read_from_menuitem(it, &v)) backupValI = v;
  } break;
  case VT_BOOL: {
    bool v = false;
    if (ui_read_from_menuitem(it, &v)) backupValI = (int32_t)v;
  } break;
  }

  // hasScreen() читает снапшот — заморозка больше не нужна
  editMode = true;
}


void MenuUI::setSaveCallback(SaveCallback cb) {
  Serial.println("");
  Serial.println("cb");
  save_cb_ = cb;
}


void MenuUI::cancelEdit() {
  const MenuItem &it = N();
  bool changed = false;
  switch (it.u.value.vtype) {
  case VT_F32:
    changed = ui_apply_from_menuitem(it, backupValF);
    break;
  case VT_U16:
    changed = ui_apply_from_menuitem(it, (float)(uint16_t)backupValI);
    break;
  case VT_U8:
    changed = ui_apply_from_menuitem(it, (float)(uint8_t)backupValI);
    break;
  case VT_I32:
    changed = ui_apply_from_menuitem(it, (float)backupValI);
    break;
  case VT_BOOL:
    changed = ui_apply_from_menuitem(it, (backupValI ? 1.f : 0.f));
    break;
  }
  (void)changed;

  editMode = false;
  if (save_cb_) save_cb_();
  Serial.println();
  Serial.println("cancelEdit");
}


// Изменение значения (крутилка энкодера): применяем шаг v.step*dir,
// ограничиваем в [minv..maxv] и, если изменилось, вызываем on_change().

void MenuUI::applyDelta(int8_t dir) {
  MenuItem &it = const_cast<MenuItem &>(N());
  ValueSpec &v = it.u.value;

  // clamp helper
  auto clampf = [](float x, float a, float b) {
    if (x < a) x = a;
    if (x > b) x = b;
    return x;
  };

  switch (v.vtype) {
  case VT_F32: {
    float cur = 0.f;
    if (!ui_read_from_menuitem(it, &cur)) return;
    float nv = clampf(cur + dir * v.step, v.minv, v.maxv);
    if (nv != cur) ui_apply_from_menuitem(it, nv);
  } break;
  case VT_U16: {
    uint16_t cur = 0;
    if (!ui_read_from_menuitem(it, &cur)) return;
    int32_t nv = (int32_t)cur + (int32_t)dir * (int32_t)v.step;
    if (nv < (int32_t)v.minv) nv = (int32_t)v.minv;
    if (nv > (int32_t)v.maxv) nv = (int32_t)v.maxv;
    if ((uint16_t)nv != cur) ui_apply_from_menuitem(it, (float)(uint16_t)nv);
  } break;
  case VT_U8: {
    uint8_t cur = 0;
    if (!ui_read_from_menuitem(it, &cur)) return;
    int32_t nv = (int32_t)cur + (int32_t)dir * (int32_t)v.step;
    if (it.id == MENU_PORT1_MODE || it.id == MENU_PORT2_MODE || it.id == MENU_PORT3_MODE) {
      // Циклический переход с валидацией для port mode
      uint8_t port = (it.id == MENU_PORT1_MODE) ? 1 : (it.id == MENU_PORT2_MODE) ? 2 : 3;
      int32_t range = (int32_t)v.maxv - (int32_t)v.minv + 1;
      for (int attempt = 0; attempt < range; attempt++) {
        if (nv > (int32_t)v.maxv) nv = (int32_t)v.minv;
        if (nv < (int32_t)v.minv) nv = (int32_t)v.maxv;
        if (isPortModeValid(port, (PortMode)(uint8_t)nv)) break;
        nv += (int32_t)dir;
      }
      if (!isPortModeValid(port, (PortMode)(uint8_t)nv)) nv = cur; // все недопустимы
    } else {
      if (nv < (int32_t)v.minv) nv = (int32_t)v.minv;
      if (nv > (int32_t)v.maxv) nv = (int32_t)v.maxv;
    }
    if ((uint8_t)nv != cur) ui_apply_from_menuitem(it, (float)(uint8_t)nv);
  } break;
  case VT_I32: {
    int32_t cur = 0;
    if (!ui_read_from_menuitem(it, &cur)) return;
    int32_t nv = cur + (int32_t)dir * (int32_t)v.step;
    if (nv < (int32_t)v.minv) nv = (int32_t)v.minv;
    if (nv > (int32_t)v.maxv) nv = (int32_t)v.maxv;
    if (nv != cur) ui_apply_from_menuitem(it, (float)nv);
  } break;
  case VT_BOOL: {
    bool cur = false;
    if (!ui_read_from_menuitem(it, &cur)) return;
    bool nv = (cur ^ (dir != 0));
    if (nv != cur) ui_apply_from_menuitem(it, nv ? 1.f : 0.f);
  } break;
  }
}


// Подготовить правую «значимую» часть строки: форматирование значения в текст.
// Для SUBMENU/ACTION - строка пустая. Для VALUE/TOGGLE - по типу vtype.

void MenuUI::formatValue(const MenuItem &it, char *out, size_t n, bool withUnits) {
  if (it.type == MN_ACTION || it.type == MN_SUBMENU) {
    out[0] = 0;
    return;
  }
  const ValueSpec &v = it.u.value;
  LangId L = curLang();

  switch (v.vtype) {
  case VT_F32: {
    float val = 0.f;
    if (!ui_read_from_menuitem(it, &val)) {
      out[0] = 0;
      return;
    }
    if (fabsf(val - (int)val) == 0.0f) {
      if (withUnits && it.unit[L]) snprintf(out, n, "%d %s", (int)val, it.unit[L]);
      else snprintf(out, n, "%d", (int)val);
    } else {
      if (withUnits && it.unit[L]) snprintf(out, n, "%.2f %s", val, it.unit[L]);
      else snprintf(out, n, "%.2f", val);
    }
    // if (withUnits && it.unit[L]) snprintf(out, n, "%.2f %s", val, it.unit[L]);
    // else snprintf(out, n, "%.2f", val);
  } break;
  case VT_U16: {
    uint16_t val = 0;
    if (!ui_read_from_menuitem(it, &val)) {
      out[0] = 0;
      return;
    }
    if (withUnits && it.unit[L]) snprintf(out, n, "%u %s", (unsigned)val, it.unit[L]);
    else snprintf(out, n, "%u", (unsigned)val);
  } break;
  case VT_U8: {
    uint8_t val = 0;
    if (!ui_read_from_menuitem(it, &val)) {
      out[0] = 0;
      return;
    }
    if (it.id == MENU_PORT1_MODE || it.id == MENU_PORT2_MODE || it.id == MENU_PORT3_MODE) {
      snprintf(out, n, "%s", portModeToString((PortMode)val));
    } else if (it.id == MENU_LANGUAGE) {
      static const char *lang_names[] = {"RU", "EN"};
      snprintf(out, n, "%s", lang_names[val < 2 ? val : 0]);
    } else if (it.id == MENU_CONTROLLER_CHOICE) {
      snprintf(out, n, "%u", (unsigned)(val + 1));
    } else if (withUnits && it.unit[L]) snprintf(out, n, "%u %s", (unsigned)val, it.unit[L]);
    else snprintf(out, n, "%u", (unsigned)val);
  } break;
  case VT_I32: {
    int32_t val = 0;
    if (!ui_read_from_menuitem(it, &val)) {
      out[0] = 0;
      return;
    }
    if (withUnits && it.unit[L]) snprintf(out, n, "%ld %s", (long)val, it.unit[L]);
    else snprintf(out, n, "%ld", (long)val);
  } break;
  case VT_BOOL: {
    bool val = false;
    if (!ui_read_from_menuitem(it, &val)) {
      out[0] = 0;
      return;
    }
    snprintf(out, n, "%s", tr_bool(val, L));
  } break;
  }
}


// Отрисовать одну строку меню: слева title, справа value (если есть).
// Если строка выделена - инвертируем цвета (заливаем фон и рисуем текст «тёмным»).

void MenuUI::drawRow(uint8_t row, int16_t idx, bool selected) {
  const MenuItem &it = g_menu[idx];
  LangId L = curLang();
  const char *title = it.title[L] ? it.title[L] : it.title[LANG_EN];

  const uint8_t xL = padX;
  const uint8_t xR = displayW() - padX;
  const uint8_t y0 = padY + row * lineH;

  const bool isValueType = (it.type == MN_VALUE || it.type == MN_TOGGLE);
  const bool editHere = (editMode && isValueType && selected);

  u8->setFontMode(0);
  u8->setFontPosTop();
  u8->setDrawColor(1);

  if (selected && !editHere) {
    u8->drawBox(0, y0, displayW(), lineH);
    u8->setDrawColor(0);
  }

  // левый текст
  u8->drawUTF8(xL, y0 + 1, title);

  // ---- правая часть ----
  if (it.type == MN_TOGGLE || (it.type == MN_VALUE && it.u.value.vtype == VT_BOOL)) {
    // Рисуем иконку 8x8: пустой квадрат = Off, залитый = On
    bool v = *(bool *)it.u.value.ptr;

    const uint8_t box = 12;             // размер иконки
    int16_t x = xR - box - 1;           // по правому краю
    int16_t y = y0 + (lineH - box) / 2; // по центру строки

    // Блок "toggle" со свечением при редактировании
    if (editHere) {
      u8->setDrawColor(2); // XOR mode
    }

    if (v) {
      u8->drawBox(x, y, box, box); // ON = закрашенный квадрат
    } else {
      u8->drawFrame(x, y, box, box); // OFF = рамка
    }

    // Вернём нормальный режим рисования, чтобы остальной текст был корректный
    u8->setDrawColor(1);
  } else {
    // для чисел/текста
    char val[32] = {0};
    formatValue(it, val, sizeof(val), showUnits);
    if (val[0]) {
      uint16_t w = utf8Width(val);
      int16_t x = xR - (int16_t)w;
      if (x < xL + 48) x = xL + 48;

      if (editHere) {
        // const int16_t bx = (x - 2 > 0) ? (x - 2) : 0;
        // const uint16_t wmax = (uint16_t)(displayW() - bx);
        // const uint16_t bw = ((uint16_t)(w + 4) < wmax) ? (uint16_t)(w + 4) : wmax;
        const int16_t bx = max<int16_t>(x - 2, 0);
        const uint16_t bw = min<uint16_t>((uint16_t)(w + 4), (uint16_t)(displayW() - bx));
        u8->setDrawColor(1);
        u8->drawBox(bx, y0, bw, lineH);
        u8->setDrawColor(0);
        u8->drawUTF8(x, y0 + 1, val);
        u8->setDrawColor(1);
      } else {
        u8->drawUTF8(x, y0 + 1, val);
      }
    }
  }

  u8->setDrawColor(1);
}


void MenuUI::drawHeader() {
  const MenuItem &par = g_menu[currentParent];
  LangId L = curLang();
  // const char* title = par.title[L] ? par.title[L] : par.title[LANG_EN];

  const char *baseTitle = par.title[L] ? par.title[L] : par.title[LANG_EN];

  // добавляем [U#] к заголовку
  uint8_t u = menu_get_active_controller();
  char title[64];
#if NUM_UNITS > 1
  snprintf(title, sizeof(title), "%s U%u", baseTitle, (unsigned)(u + 1));
#else
  snprintf(title, sizeof(title), "%s", baseTitle);
#endif


  u8->setFontMode(0);
  u8->setFontPosTop();

  // 3) Центрируем по горизонтали
  uint16_t w = utf8Width(title);
  int16_t x = (displayW() - w) / 2;
  if (x < 0) x = 0;

  u8->drawUTF8(x, padY, title);
  u8->drawHLine(0, padY + lineH - 1, displayW());
  // 4) Возвращаем цвет в нормальный (белый для остального меню)
  // u8->setDrawColor(1);
}

void MenuUI::draw() {
  // Не рисуем если экрана нет (Port3: SCR->LNK)
  if (!hasScreen()) return;

  u8->clearBuffer();

  // 1) Шапка с названием текущего родителя
  drawHeader(); // рисуем заголовок в строке 0

  // 2) Список под шапкой - начинаем со строки 1
  const MenuItem &par = g_menu[currentParent];
  const uint16_t cnt = (par.type == MN_SUBMENU) ? childCountOf(currentParent) : 0;
  const uint8_t baseRow = 1; // строка 0 занята шапкой

  if (cnt == 0) {
    // Родитель без детей - показываем текущий пункт под шапкой
    drawRow(baseRow, cursor, true);
  } else {
    if (scroll > cnt - 1) scroll = 0;
    for (uint8_t i = 0; i < lines; ++i) {
      const uint16_t pos = scroll + i;
      if (pos >= cnt) break;
      const int16_t idx = childAtPos(currentParent, pos);
      // рисуем на строке (1 + i), чтобы не залезать на шапку
      drawRow(baseRow + i, idx, idx == cursor);
    }
  }

  u8->sendBuffer();
}

// C-интерфейс для смены языка извне (например, из кода на C/ISR/и т.п.).
extern "C" void menu_ui_apply_language(uint8_t lang_index) {
  extern MenuUI ui;               // где-то глобально объявлен MenuUI ui;
  ui.setLang((LangId)lang_index); // применяем язык
}
