// Settings-page translations + Clay config builder. Language index matches the
// watch side (src/c/i18n.h): 0 = English, 1 = Nederlands, 2 = Français,
// 3 = Deutsch, 4 = Español. 255 = follow the watch's own locale.
var LANGS = ['en', 'nl', 'fr', 'de', 'es'];
var LANG_AUTO = 255;

var L = {
  nl: {
    base_url: 'Basis-URL',
    status_pages: "Status-pagina's (komma-gescheiden slugs)",
    default_page: 'Standaardpagina (slug)',
    display: 'Weergave',
    language: 'Taal',
    sort_by: 'Sorteren op',
    sort_page: 'Pagina-volgorde',
    sort_name: 'Naam',
    sort_status: 'Status (problemen eerst)',
    save: 'Opslaan'
  },
  en: {
    base_url: 'Base URL',
    status_pages: 'Status pages (comma-separated slugs)',
    default_page: 'Default page (slug)',
    display: 'Display',
    language: 'Language',
    sort_by: 'Sort by',
    sort_page: 'Page order',
    sort_name: 'Name',
    sort_status: 'Status (problems first)',
    save: 'Save'
  },
  de: {
    base_url: 'Basis-URL',
    status_pages: 'Statusseiten (kommagetrennte Slugs)',
    default_page: 'Standardseite (Slug)',
    display: 'Anzeige',
    language: 'Sprache',
    sort_by: 'Sortieren nach',
    sort_page: 'Seitenreihenfolge',
    sort_name: 'Name',
    sort_status: 'Status (Probleme zuerst)',
    save: 'Speichern'
  },
  fr: {
    base_url: 'URL de base',
    status_pages: 'Pages de statut (slugs séparés par des virgules)',
    default_page: 'Page par défaut (slug)',
    display: 'Affichage',
    language: 'Langue',
    sort_by: 'Trier par',
    sort_page: 'Ordre des pages',
    sort_name: 'Nom',
    sort_status: "Statut (problèmes d'abord)",
    save: 'Enregistrer'
  },
  es: {
    base_url: 'URL base',
    status_pages: 'Páginas de estado (slugs separados por comas)',
    default_page: 'Página por defecto (slug)',
    display: 'Pantalla',
    language: 'Idioma',
    sort_by: 'Ordenar por',
    sort_page: 'Orden de páginas',
    sort_name: 'Nombre',
    sort_status: 'Estado (fallos primero)',
    save: 'Guardar'
  }
};

function buildConfig(langIdx) {
  var s = L[LANGS[langIdx] || 'en'];
  return [
    { type: 'heading', defaultValue: 'PebbleKuma' },
    { type: 'section', items: [
      { type: 'heading', defaultValue: 'Uptime Kuma' },
      { type: 'input', messageKey: 'BASE_URL', label: s.base_url, defaultValue: '',
        attributes: { placeholder: 'https://status.company.com', type: 'url' } },
      { type: 'input', messageKey: 'STATUS_PAGES', label: s.status_pages, defaultValue: '',
        attributes: { placeholder: 'monitoring,public' } },
      { type: 'input', messageKey: 'DEFAULT_PAGE', label: s.default_page, defaultValue: '',
        attributes: { placeholder: 'monitoring' } }
    ] },
    { type: 'section', items: [
      { type: 'heading', defaultValue: s.display },
      { type: 'select', messageKey: 'LANGUAGE', label: s.language, defaultValue: String(langIdx),
        options: [
          { label: 'Automatic (watch language)', value: String(LANG_AUTO) },
          { label: 'English', value: '0' },
          { label: 'Nederlands', value: '1' },
          { label: 'Français', value: '2' },
          { label: 'Deutsch', value: '3' },
          { label: 'Español', value: '4' }
        ] },
      { type: 'select', messageKey: 'SORT_BY', label: s.sort_by, defaultValue: '0',
        options: [
          { label: s.sort_page, value: '0' },
          { label: s.sort_name, value: '1' },
          { label: s.sort_status, value: '2' }
        ] }
    ] },
    { type: 'submit', defaultValue: s.save }
  ];
}

module.exports = { LANGS: LANGS, LANG_AUTO: LANG_AUTO, buildConfig: buildConfig };
