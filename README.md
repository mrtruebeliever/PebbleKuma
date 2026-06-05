# PebbleKuma

A Pebble Time 2 watchapp that shows your [Uptime Kuma](https://github.com/louislam/uptime-kuma)
status-page monitors on the wrist. White/black UI with a `#5cdd8b` accent and a
bear menu icon (the Uptime Kuma mascot).

## Features

- Browse the monitors of a status page: scroll with up/down, status dot per row
  (green = up, red = down, yellow = pending, blue = maintenance) plus 24h uptime.
- Select a monitor for a **card** showing: a slim status bar with a vector
  status icon (♥ up / ⚠ down / ? pending / ⟳ maintenance, from pebble-dev's
  iconography), the name, an accent-outlined cloud badge with the 24h uptime %
  inside, current ping with min/avg/max, monitor type, last-checked time, last
  message (on problems), and a heartbeat bar graph. **Up/down page between
  monitor cards** (in the current sort order): the text and the status icon
  slide out, the data swaps, and they slide back in (the SDK cards-example
  pattern); a "3/14" counter sits top-right.
- Clear states when there's no data: **"Niet ingesteld"** (open the settings) vs
  **"Geen verbinding"** (configured but the fetch failed — e.g. VPN/network).
- **Sorting** by page order / name / status (problems first). Set a default in
  settings and switch on the fly in-app.
- Top list row is the **menu** — Select opens an ActionMenu with a "Sorteren"
  submenu and all configured status pages (long-press Select works anywhere too).

## Settings (Clay)

Open the app settings in the Pebble phone app:

- **Base URL** — e.g. `https://status.company.com`
- **Status pages** — comma-separated slugs, e.g. `monitoring,public`
- **Default page** — the slug shown on launch (falls back to the first)
- **Accent color** — defaults to `#5cdd8b`
- **Sort by** — page order / name / status (also switchable in-app)

The phone-side JS fetches `{base}/api/status-page/{slug}` and
`{base}/api/status-page/heartbeat/{slug}`, aggregates per monitor (name, latest
status, 24h uptime, recent heartbeats) and streams the result to the watch one
message at a time (throttled per ACK so the inbox buffer never overflows).

## Building & running

```sh
pebble clean && pebble build          # clean is required after resource changes
pebble install --emulator emery       # install on the emery emulator
pebble install --cloudpebble          # install to a real PT2 via Dev Connect
```

## Project layout

```
src/c/pebblekuma.c     main, window stack, AppMessage open
src/c/config.{c,h}     accent persist + AppMessage inbox/outbox
src/c/data.{c,h}       monitor/page data model + status colors
src/c/monitor_list.c   MenuLayer + StatusBarLayer + page ActionMenu
src/c/monitor_detail.c detail window + heartbeat bar graph
src/pkjs/index.js      Clay + Uptime Kuma fetch/aggregate/stream
src/pkjs/config.json   Clay settings form
resources/images/      bear menu icon (25x25, white on transparent)
```

## Documentation

Full SDK docs: <https://developer.repebble.com>
