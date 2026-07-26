#pragma once

// UI languages. Index must match the phone-side LANGS array in pkjs/i18n.js.
// English is index 0 and the fallback: a user whose watch is set to a language
// this app does not carry gets English, not Dutch.
typedef enum {
  LANG_EN = 0,
  LANG_NL = 1,
  LANG_FR = 2,
  LANG_DE = 3,
  LANG_ES = 4,
  LANG_COUNT,
} Lang;

// Sentinel stored/sent for "follow the watch's system locale" (see config.c).
#define LANG_AUTO 255

// Translatable strings. Entries marked "(fmt)" carry a printf placeholder.
typedef enum {
  STR_PAGE_FMT = 0,      // "Pagina: %s" (fmt)
  STR_MENU_SORT_FMT,     // "Menu · sortering: %s" (fmt)
  STR_SORT_PAGE,         // short sort label (subtitle)
  STR_SORT_NAME,
  STR_SORT_STATUS,
  STR_LOADING_T,
  STR_LOADING_S,
  STR_ERROR_T,
  STR_ERROR_S,
  STR_UNCONF_T,
  STR_UNCONF_S,
  STR_EMPTY_T,
  STR_MENU_SORT_TITLE,   // ActionMenu "Sorteren"
  STR_AM_PAGEORDER,      // ActionMenu full labels
  STR_AM_NAME,
  STR_AM_STATUS,
  STR_LAST_FMT,          // "Laatst: %s" (fmt)
  STR_NODATA,            // "(geen data)"
  STR_COUNT,
} StrId;

// Returns the string for `id` in the current language (config_lang()).
const char *i18n(StrId id);
