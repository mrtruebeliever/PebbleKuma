# Changelog

All notable changes to PebbleKuma are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/).

## [1.3.0] - 2026-10-11

### Added
- **Monitor names follow the watch's text size.** At the default size the names
  are as before (24 bold, 18 on the 144px screen); a smaller or larger text
  size in the watch settings moves them one step (down to 14, up to 28 bold),
  and the rows grow to match.
- **Backlight colour follows the status (Pebble Time 2).** Red while a monitor
  on the active page is down, amber while one is pending, the watch's own
  colour otherwise.
- **Status in the launcher.** Kuma's line in the app list shows how the active
  page stood when the app closed: "2 DOWN · 14:06", "1 PENDING · 14:06" or
  "12/12 UP · 14:06". An offline launch leaves the previous line in place.
- **Pebble Round 2 (`gabbro`) and Pebble 2 Duo (`flint`).** On the round screen
  the header is just the centred clock, the selected row runs out to the bezel
  while the rows slide along the curved edge as the list scrolls, and the
  monitor card sits in a frame below the bezel with the status icon beside its
  label, the card count on the type line, and a narrower heartbeat strip. On
  black-and-white the highlight is black with white text, the card stays white,
  and each status gets its own mark — filled dot up, cross down, ring pending,
  square maintenance — instead of a colour that would land on the same black.
  The 144px screen gets smaller names on rows and cards.

### Changed
- Settings page moved from `pebble-clay` to `@rebble/clay` 1.1.
- The header clock reserves only the width it needs, so "PebbleKuma" fits on a
  144px screen.

## [1.2.0] - 2026-08-13

### Added
- **Touch navigation on the Pebble Time 2.** The monitor list scrolls with your
  finger and keeps gliding when you let go, a tap opens the monitor you touched,
  and on the detail card a vertical flick pages between monitors. A left-to-right
  swipe goes back, and from the monitor list it leaves the app, as BACK does.

### Changed
- **Reverted the 1.1.3 opt-in into the system touch bridge.** On firmware 4.33.1
  `app_touch_navigation_enable(true)` faults inside firmware on the first touch
  and kills the app. Touch is now handled from the raw touch stream inside the
  app (`src/c/touch_nav.c`). Reported as
  [PebbleOS#1865](https://github.com/coredevices/PebbleOS/issues/1865).

## [1.1.6] - 2026-08-13

### Added
- **Branded header bar**, replacing the plain white system status bar: accent
  fill, a small bear icon, "PebbleKuma", and the clock — same look PebbleDoist
  uses for its top bar.

### Changed
- **Monitor rows are more compact** (40px, was 44px) so **4 monitors fit on
  screen** at once instead of 3. The header bar keeps the same height the
  system status bar used, so it doesn't cost any of that extra room.

## [1.1.5] - 2026-08-13

### Changed
- **Scaled the list back down** — the 1.1.3 uptime-percentage bump grew each
  monitor row to 48px, which meant one fewer row fit on screen. The
  percentage is now bold GOTHIC_14 (was GOTHIC_18_BOLD) instead of a bigger
  font: same 44px row as before, and bold black-on-grey still makes the
  number stand out from the status word next to it.

## [1.1.4] - 2026-08-13

### Changed
- **Bigger text on the monitor detail card** — the ping, type/last-checked and
  error-message lines are now GOTHIC_18 (were GOTHIC_14), and the ping value
  is GOTHIC_18_BOLD (was GOTHIC_18), matching the punchier feel of the list
  view's uptime number.

## [1.1.3] - 2026-08-13

### Changed
- **Bigger uptime percentage** in the monitor list — it now draws separately
  from the status word at a larger, bolder size, matching the detail
  screen's uptime cloud.
- Opts into the watch's own touch navigation (Pebble SDK 4.33) on the
  monitor list.

## [1.1.2] - 2026-07-16

### Changed
- Smaller store download — the release build now minifies the JS bundle and strips the
  source map (`tools/release.py`), roughly halving the `.pbw` with no change in behavior.

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
