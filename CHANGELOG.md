# Changelog

All notable changes to PebbleKuma are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/).

## [1.1.1] - 2026-06-05

### Changed
- A failed status fetch now shows an error row you can tap to retry, instead of
  staying on "Loading…".

### Fixed
- Detail card window is reused across openings rather than being destroyed from
  within its own unload handler, preventing a potential crash on rapid reopen.

## [1.1.0] - 2026-06-05

### Added
- Animated uptime percentage that counts up on card entry and when paging
  between cards (the cloud icon itself does not animate).
- Heartbeat bars now sweep in left-to-right, each rising from the bottom, on
  card entry and paging.
- HR-pulse icon before the heartbeat bars, drawn on a black panel so every
  status color stays legible.
- Multilingual UI and settings page: Nederlands, English, Deutsch, Français,
  with a new Language picker.
- App-store icons (80×80 and 144×144) and a README with screenshots.

### Changed
- Monitor cards now use a soft pastel of their status as the background
  (mint = up, melon = down, yellow = pending, sky = maintenance).
- Fixed vivid status palette (up `#00AA55`, down `#FF0000`, pending `#FFAA00`,
  maintenance `#00AAFF`) used for icons, heartbeats, and list dots.
- Monitor list redesigned as white + accent with dark, readable text and a
  legible page-row title.
- Bear menu icon gained an outline so it is visible on a white background.
- Status sort now breaks ties by uptime (lowest percentage first).

### Removed
- Accent-color picker from settings; the accent is fixed at `#5cdd8b`.
- "Heartbeats" text label on the detail card (replaced by the HR-pulse icon).
- Stray, unused `k_*.png` files.

## [1.0.0] - 2026-06-05

### Added
- Initial release: a Pebble Time 2 watchapp showing Uptime Kuma status-page
  monitors — list view, paged detail cards, page/sort ActionMenu, and a
  PebbleKit JS layer that fetches and streams monitor data from Uptime Kuma.
