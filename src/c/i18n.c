#include "i18n.h"
#include "config.h"

// [language][string]. Keep rows aligned with the StrId enum order.
static const char *const S[LANG_COUNT][STR_COUNT] = {
  [LANG_NL] = {
    [STR_PAGE_FMT]        = "Pagina: %s",
    [STR_MENU_SORT_FMT]   = "Menu · sortering: %s",
    [STR_SORT_PAGE]       = "pagina-volgorde",
    [STR_SORT_NAME]       = "naam",
    [STR_SORT_STATUS]     = "status",
    [STR_LOADING_T]       = "Laden…",
    [STR_LOADING_S]       = "Even geduld",
    [STR_ERROR_T]         = "Geen verbinding",
    [STR_ERROR_S]         = "Check VPN/netwerk · Select",
    [STR_UNCONF_T]        = "Niet ingesteld",
    [STR_UNCONF_S]        = "Stel in via de telefoon",
    [STR_EMPTY_T]         = "Geen monitors",
    [STR_MENU_SORT_TITLE] = "Sorteren",
    [STR_AM_PAGEORDER]    = "Pagina-volgorde",
    [STR_AM_NAME]         = "Naam",
    [STR_AM_STATUS]       = "Status (problemen eerst)",
    [STR_LAST_FMT]        = "Laatst: %s",
    [STR_NODATA]          = "(geen data)",
  },
  [LANG_EN] = {
    [STR_PAGE_FMT]        = "Page: %s",
    [STR_MENU_SORT_FMT]   = "Menu · sorting: %s",
    [STR_SORT_PAGE]       = "page order",
    [STR_SORT_NAME]       = "name",
    [STR_SORT_STATUS]     = "status",
    [STR_LOADING_T]       = "Loading…",
    [STR_LOADING_S]       = "Please wait",
    [STR_ERROR_T]         = "No connection",
    [STR_ERROR_S]         = "Check VPN/network · Select",
    [STR_UNCONF_T]        = "Not set up",
    [STR_UNCONF_S]        = "Set up via the phone",
    [STR_EMPTY_T]         = "No monitors",
    [STR_MENU_SORT_TITLE] = "Sort",
    [STR_AM_PAGEORDER]    = "Page order",
    [STR_AM_NAME]         = "Name",
    [STR_AM_STATUS]       = "Status (problems first)",
    [STR_LAST_FMT]        = "Last: %s",
    [STR_NODATA]          = "(no data)",
  },
  [LANG_DE] = {
    [STR_PAGE_FMT]        = "Seite: %s",
    [STR_MENU_SORT_FMT]   = "Menü · Sortierung: %s",
    [STR_SORT_PAGE]       = "Seitenfolge",
    [STR_SORT_NAME]       = "Name",
    [STR_SORT_STATUS]     = "Status",
    [STR_LOADING_T]       = "Laden…",
    [STR_LOADING_S]       = "Bitte warten",
    [STR_ERROR_T]         = "Keine Verbindung",
    [STR_ERROR_S]         = "VPN/Netzwerk prüfen · Select",
    [STR_UNCONF_T]        = "Nicht eingerichtet",
    [STR_UNCONF_S]        = "Über das Telefon einrichten",
    [STR_EMPTY_T]         = "Keine Monitore",
    [STR_MENU_SORT_TITLE] = "Sortieren",
    [STR_AM_PAGEORDER]    = "Seitenreihenfolge",
    [STR_AM_NAME]         = "Name",
    [STR_AM_STATUS]       = "Status (Probleme zuerst)",
    [STR_LAST_FMT]        = "Zuletzt: %s",
    [STR_NODATA]          = "(keine Daten)",
  },
  [LANG_FR] = {
    [STR_PAGE_FMT]        = "Page : %s",
    [STR_MENU_SORT_FMT]   = "Menu · tri : %s",
    [STR_SORT_PAGE]       = "ordre",
    [STR_SORT_NAME]       = "nom",
    [STR_SORT_STATUS]     = "statut",
    [STR_LOADING_T]       = "Chargement…",
    [STR_LOADING_S]       = "Veuillez patienter",
    [STR_ERROR_T]         = "Pas de connexion",
    [STR_ERROR_S]         = "Vérifiez VPN/réseau · Select",
    [STR_UNCONF_T]        = "Non configuré",
    [STR_UNCONF_S]        = "Configurer via le téléphone",
    [STR_EMPTY_T]         = "Aucun moniteur",
    [STR_MENU_SORT_TITLE] = "Trier",
    [STR_AM_PAGEORDER]    = "Ordre des pages",
    [STR_AM_NAME]         = "Nom",
    [STR_AM_STATUS]       = "Statut (problèmes d'abord)",
    [STR_LAST_FMT]        = "Dernier : %s",
    [STR_NODATA]          = "(pas de données)",
  },
};

const char *i18n(StrId id) {
  int lang = config_lang();
  if (lang < 0 || lang >= LANG_COUNT) { lang = LANG_NL; }
  if (id >= STR_COUNT) { return ""; }
  const char *s = S[lang][id];
  return s ? s : (S[LANG_EN][id] ? S[LANG_EN][id] : "");
}
