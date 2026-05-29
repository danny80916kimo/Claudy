#include "ui.h"
#include "mascot.h"
#include "theme.h"
#include "lvgl_port.h"
#include "gfx.h"
#include <Arduino.h>

namespace {
lv_obj_t *s_mascot;
lv_obj_t *s_state_label;
lv_obj_t *s_chip_bg;
lv_obj_t *s_chip_label;
lv_obj_t *s_tool_label;
lv_obj_t *s_msg_label;
lv_obj_t *s_bar;
lv_obj_t *s_bar_pct_label;
lv_obj_t *s_bar_detail_label;

lv_timer_t *s_anim_timer = nullptr;

void anim_cb(lv_timer_t *t) {
  mascot_draw(s_mascot, g_state.state);
  // Adjust period if state changed
  uint32_t want = mascot_anim_interval(g_state.state);
  if (want > 0 && want != t->period) lv_timer_set_period(t, want);
}

const char* tool_glyph(ToolIcon t) {
  switch (t) {
    case TOOL_READ:  return "R";
    case TOOL_EDIT:  return "E";
    case TOOL_WRITE: return "W";
    case TOOL_BASH:  return "$";
    case TOOL_GREP:  return "G";
    case TOOL_WEB:   return "@";
    case TOOL_TASK:  return "T";
    default:         return "*";
  }
}

lv_color_t tool_color(ToolIcon t) {
  switch (t) {
    case TOOL_READ:  return rgb565_to_lv(0x3475);
    case TOOL_EDIT:  return rgb565_to_lv(0xC9A0);
    case TOOL_WRITE: return rgb565_to_lv(0xCAA0);
    case TOOL_BASH:  return rgb565_to_lv(0x39E7);
    case TOOL_GREP:  return rgb565_to_lv(0x4A89);
    case TOOL_WEB:   return rgb565_to_lv(0x2C9F);
    case TOOL_TASK:  return rgb565_to_lv(0x880F);
    default:         return rgb565_to_lv(0x528A);
  }
}
}  // namespace

void ui_init() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  s_mascot = mascot_create(scr);
  if (!s_mascot) {
    Serial.println("FATAL: mascot alloc failed");
    while (true) delay(1000);
  }
  s_anim_timer = lv_timer_create(anim_cb, mascot_anim_interval(STATE_IDLE), nullptr);

  // State label (top of info region)
  s_state_label = lv_label_create(scr);
  lv_label_set_text(s_state_label, "Boot");
  lv_obj_set_style_text_font(s_state_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(s_state_label, lv_color_white(), 0);
  lv_obj_set_pos(s_state_label, INFO_X, INFO_Y);

  // Tool chip
  s_chip_bg = lv_obj_create(scr);
  lv_obj_set_size(s_chip_bg, 36, 28);
  lv_obj_set_pos(s_chip_bg, INFO_X, INFO_Y + 50);
  lv_obj_set_style_radius(s_chip_bg, 6, 0);
  lv_obj_set_style_border_width(s_chip_bg, 0, 0);
  lv_obj_set_style_pad_all(s_chip_bg, 0, 0);
  lv_obj_clear_flag(s_chip_bg, LV_OBJ_FLAG_SCROLLABLE);

  s_chip_label = lv_label_create(s_chip_bg);
  lv_obj_set_style_text_font(s_chip_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_chip_label, lv_color_white(), 0);
  lv_obj_center(s_chip_label);
  lv_label_set_text(s_chip_label, "");

  // Tool name (right of chip)
  s_tool_label = lv_label_create(scr);
  lv_obj_set_style_text_font(s_tool_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_tool_label, rgb565_to_lv(COL_DIM), 0);
  lv_obj_set_pos(s_tool_label, INFO_X + 46, INFO_Y + 56);
  lv_label_set_text(s_tool_label, "");

  // Message (2 lines, CJK)
  s_msg_label = lv_label_create(scr);
  lv_obj_set_width(s_msg_label, INFO_W);
  lv_label_set_long_mode(s_msg_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_msg_label, &lv_font_simsun_16_cjk, 0);
  lv_obj_set_style_text_color(s_msg_label, rgb565_to_lv(COL_DIM), 0);
  lv_obj_set_pos(s_msg_label, INFO_X, INFO_Y + 96);
  lv_label_set_text(s_msg_label, "Starting...");

  // Token bar
  s_bar = lv_bar_create(scr);
  lv_obj_set_size(s_bar, INFO_W - 100, BAR_H);
  lv_obj_set_pos(s_bar, INFO_X, BAR_Y);
  lv_bar_set_range(s_bar, 0, 1000);   // we'll scale used/max into 0..1000
  lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_bar, rgb565_to_lv(0x4A49), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_bar, rgb565_to_lv(COL_BAR_OK), LV_PART_INDICATOR);

  // Big percent (right of bar)
  s_bar_pct_label = lv_label_create(scr);
  lv_obj_set_style_text_font(s_bar_pct_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(s_bar_pct_label, lv_color_white(), 0);
  lv_obj_set_pos(s_bar_pct_label, INFO_X + (INFO_W - 100) + 12, BAR_Y - 6);
  lv_label_set_text(s_bar_pct_label, "");

  // Detail (above bar)
  s_bar_detail_label = lv_label_create(scr);
  lv_obj_set_style_text_font(s_bar_detail_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_bar_detail_label, rgb565_to_lv(0x8C71), 0);
  lv_obj_set_pos(s_bar_detail_label, INFO_X, BAR_Y - 24);
  lv_label_set_text(s_bar_detail_label, "");
}

void ui_apply_state() {
  lv_label_set_text(s_state_label, stateName(g_state.state));

  if (g_state.tool == TOOL_NONE) {
    lv_obj_add_flag(s_chip_bg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_tool_label, "");
  } else {
    lv_obj_clear_flag(s_chip_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(s_chip_bg, tool_color(g_state.tool), 0);
    lv_label_set_text(s_chip_label, tool_glyph(g_state.tool));
    lv_label_set_text(s_tool_label, toolName(g_state.tool));
  }

  lv_label_set_text(s_msg_label, g_state.message);

  if (g_state.tokensMax > 0) {
    int pct1000 = (int)((uint64_t)g_state.tokensUsed * 1000 / g_state.tokensMax);
    if (pct1000 > 1000) pct1000 = 1000;
    lv_bar_set_value(s_bar, pct1000, LV_ANIM_ON);

    // Indicator color crosses thresholds
    lv_color_t c = rgb565_to_lv(COL_BAR_OK);
    if (pct1000 > 750) c = rgb565_to_lv(COL_BAR_WARN);
    if (pct1000 > 900) c = rgb565_to_lv(COL_BAR_HOT);
    lv_obj_set_style_bg_color(s_bar, c, LV_PART_INDICATOR);

    char pctbuf[8];
    snprintf(pctbuf, sizeof(pctbuf), "%d%%", pct1000 / 10);
    lv_label_set_text(s_bar_pct_label, pctbuf);

    char detbuf[32];
    if (g_state.tokensMax >= 1000000) {
      snprintf(detbuf, sizeof(detbuf), "%lu.%luk / 1M",
               (unsigned long)(g_state.tokensUsed / 1000),
               (unsigned long)((g_state.tokensUsed % 1000) / 100));
    } else {
      snprintf(detbuf, sizeof(detbuf), "%luk / %luk",
               (unsigned long)(g_state.tokensUsed / 1000),
               (unsigned long)(g_state.tokensMax / 1000));
    }
    lv_label_set_text(s_bar_detail_label, detbuf);
  } else {
    lv_label_set_text(s_bar_pct_label, "");
    lv_label_set_text(s_bar_detail_label, "");
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
  }
}
