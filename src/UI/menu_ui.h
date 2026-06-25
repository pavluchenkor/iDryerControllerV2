#pragma once
#include <U8g2lib.h>
#include <functional>
#include "menu/menu_types.h"
#include "menu/menu_ids.h"
#include "menu_ui_helpers.h"


extern const MenuItem g_menu[];

class MenuUI {
public:
  void begin(U8G2* u8in, const uint8_t* langPtr);
  
  void jumpTo(MenuId id, bool enterChild = false);
  void setLang(LangId l) { lang_fallback_ = l; setFont(); }
  void setLangSource(const uint8_t *p)
  { // доп. сеттер источника языка
    lang_ptr_ = p;
  }

  void setShowUnits(bool on) { showUnits = on; }
  
  void handleInput(int8_t encoderDelta, bool click, bool goBack);
  void draw();
  const char* getItemName(int16_t idx);

  typedef void (*SaveCallback)(void);
  void setSaveCallback(SaveCallback cb);
  void setFont();

private:
  // UI state
  U8G2*  u8   = nullptr;
  const uint8_t* lang_ptr_ = nullptr; 
  // если lang_ptr_ задан - берём язык оттуда; иначе используем фолбэк
  LangId lang_fallback_ = LANG_EN;
  inline LangId curLang() const {
    return lang_ptr_ ? static_cast<LangId>(*lang_ptr_) : lang_fallback_;
  }
  // layout
  uint8_t lines  = 3;
  uint8_t padX   = 0;
  uint8_t padY   = 0;
  uint8_t lineH  = 16;

  // navigation
  int16_t currentParent = 0; // какой список показываем
  int16_t cursor         = 0;
  uint8_t scroll         = 0;
  bool    editMode       = false;

  // edit backup
  float   backupValF = 0.0f;
  int32_t backupValI = 0;
  bool showUnits = false;
  
  SaveCallback save_cb_ = nullptr;

  // helpers
  int16_t findIndexById(MenuId id) const;
  void drawHeader();
  const MenuItem& N() const { return g_menu[cursor]; }
  
  void enter();
  void back();
  void moveCursor(int8_t step);
  void startEdit();
  void cancelEdit();
  void applyDelta(int8_t dir);
  void drawRow(uint8_t row, int16_t idx, bool selected);
  void formatValue(const MenuItem &it, char *buf, size_t bufsize, bool withUnits = true);

  uint16_t utf8Width(const char* s) const { return u8 ? u8->getUTF8Width(s) : 0; }
  uint8_t  displayW() const { return u8 ? u8->getDisplayWidth()  : 128; }
  uint8_t  displayH() const { return u8 ? u8->getDisplayHeight() : 64; }
};

#ifdef __cplusplus
extern "C" {
#endif
void menu_ui_apply_language(uint8_t lang_index);
#ifdef __cplusplus
}
#endif