# PebbleKuma — appstore listing

**Tagline:** Your Uptime Kuma status page, on your wrist.

**Short description (≤ 140 chars):**
Watch your Uptime Kuma monitors from your Pebble: uptime, ping and heartbeats per monitor, with status-coloured cards. Touch on PT2.

**Full description:**

PebbleKuma shows the monitors from your own Uptime Kuma status page on a Pebble
Time 2. The list is a clean white page: a coloured status dot, the monitor name
and its 24-hour uptime, so one glance tells you whether anything needs you.
Select a monitor for a card in the colour of its state, with the numbers that
matter when something is off — ping with min/avg/max, when it was last checked,
the last message, and a heartbeat graph of recent checks. Up and down page
straight through to the next monitor.

FEATURES
• Monitor list — status dot (green up, red down, yellow pending, blue
  maintenance), name and 24h uptime on every row.
• Monitor cards — the background is a soft pastel of the monitor's status, with
  a vector status icon, an uptime badge, current ping with min/avg/max, the
  check type, the time of the last check, the last message when something is
  wrong, and a heartbeat bar graph.
• Paging — up and down move between monitors in the current sort order, without
  going back to the list.
• Sorting — by page order, by name, or by status with problems first (down,
  pending, maintenance, up, then lowest uptime). Set a default in the settings,
  change it on the fly from the in-app menu.
• Several status pages — configure more than one and switch between them from
  the menu.
• Touchscreen on Pebble Time 2: drag the list to scroll, tap a monitor to open
  its card, flick up or down to page between monitors, swipe right to go back.
  The buttons do everything the touchscreen does.
• The page's status in the app list ("2 DOWN · 14:06"), so you can check
  without opening the app.
• On the Pebble Time 2 the colour backlight turns red while something is down
  and amber while something is pending.
• Monitor names follow your watch's text size.
• Also built for the Pebble Round 2 and the black-and-white Pebble 2 Duo.
• Animated where it helps: the uptime percentage counts up and the heartbeat
  bars sweep in, on entry and when paging.
• Clear empty states, so "not configured yet" never looks like "everything is
  down".

SETUP
1. In Uptime Kuma, create (or open) a status page and note its slug.
2. In the Pebble phone app, open PebbleKuma's settings and fill in your Uptime
   Kuma URL and the status page slug. Add more pages if you have them, and pick
   the default sort order.

Your phone needs network access to your Uptime Kuma instance (same network, or
through a reverse proxy or VPN). Requires a Pebble Time 2.

**Category:** Tools & Utilities
**Icons:** store/icon-80.png, store/icon-144.png
**Screenshots:** store/screenshots/ (01 monitor list, 02 healthy card, 03 down
card, 04 maintenance card — captured against demo monitors, not a real instance)
**Demo:** store/pebblekuma-demo.gif
