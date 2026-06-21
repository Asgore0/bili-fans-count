# Changelog

## v4.26

- Switched the menu bar item back to native macOS template-image and title rendering so dark and light menu bars inherit system tint correctly.
- Removed full-size titlebar content overlap while keeping the window chrome color aligned with the app surface.
- Synced menu bar popover segmented controls with the current glass appearance immediately.
- Hardened trend history filtering to sort data before using sorted-only helpers.
- Documented the macOS icon generator and added clearer dependency/font/tool fallbacks.

## v4.25

- Extended the app's glass content layer into the macOS title bar so the top chrome matches the main surface color.

## v4.24

- Smoothed the main preview card glass layer by removing the mid-surface gradient seam.
- Removed the oval capsule background around the menu bar follower number.

## v4.23

- Simplified the macOS app icon to a clean continuous blue B mark without oversized internal mask layers.
- Clipped icon artwork to the rounded-square body before creating the icns asset.

## v4.22

- Removed the hard horizontal split in the menu bar popover by making the glass bloom continuous across the whole surface.
- Forced the running macOS app to apply the bundled B icon as its Dock icon, avoiding stale LaunchServices icon cache.

## v4.21

- Reused thread-local date formatters for history CSV and period keys to reduce repeated allocations.
- Avoided re-sorting already sorted history during trend rendering and CSV export.
- Cached segmented history CSV backfill checks per UID for the current run.
- Skipped unchanged main-window label and tooltip writes during visible countdown updates.
- Reused the updated-time date formatter for non-today timestamps.

## v4.20

- Updated the macOS app icon to a blue rounded-square B mark.
- Reworked menu bar and popover B marks to use a cleaner rounded B.
- Made the menu bar B transparent black/white instead of a blue badge.

## v4.19

- Removed the hard horizontal divider artifact in the menu bar glass popover.

## v4.18

- Redrew the macOS menu bar status item as a custom glass capsule in dark mode.
- Kept the menu bar icon, number, and selected state on the same visual system as the popover.

## v4.17

- Reworked the menu bar popover with a BetterDisplay-style dark liquid glass surface.
- Unified dark-mode title bars with the app's glass chrome.
- Simplified popover control wells so the menu bar widget reads as one coherent glass layer.

## v4.16

- Removed an accidental "透明度" label from the lower-left main window panel.
- Kept the appearance segmented control while reducing visual clutter.

## v4.15

- Softened dark-mode glass rendering on the upper-left and lower-left panels.
- Reworked the main preview card backdrop to avoid hard horizontal divider bands.

## v4.13

- Added dark-mode glass polish across the main app and status popover.
- Reduced hard separator lines in the right-side inspector panel.

## v4.12

- Added monthly segmented CSV history files under Application Support.
- Backfilled existing local history into monthly CSV files when missing.
- Kept long-term history local while preserving a lightweight UI cache.

## v4.11

- Fixed UID/link text fields becoming non-editable after glass styling.
- Changed "copy history CSV" to copy the latest 7 days of CSV data.

## Earlier

- Added macOS AppKit app, menu bar display, status popover, refresh controls, trend ranges, cached fallback data, and liquid glass styling.
- Added Android app, home-screen widget, ColorOS/HyperOS-compatible widget metadata, and custom UID monitoring.
