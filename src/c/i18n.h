#pragma once

// UI languages. Index must match the phone-side LANGS array in pkjs/i18n.js.
typedef enum {
  LANG_NL = 0,
  LANG_EN,
  LANG_DE,
  LANG_FR,
  LANG_COUNT,
} Lang;

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
