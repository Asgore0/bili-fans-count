# Changelog

## v4.87

- Refactored the macOS source from a single large implementation file into domain-oriented `src/parts/` modules while preserving the existing single-translation-unit build and test model.
- Made the main window a full-size transparent content window and cleared transient status-bar button highlighting so the top chrome no longer reads as a separate visual state from the content below.
- Replaced the menu-bar card's `NSPopover` host with a transparent borderless `NSPanel` on macOS 26+ so Apple's official `NSGlassEffectView` can render directly against the desktop/window content instead of being flattened by the popover background.
- Lightened the official glass tint for ultra-clear and clear modes, and unified the open/closed state checks so the main app and menu-bar card stay in sync.
- Disabled AppKit window restoration for the custom glass panel so the app does not relaunch into a stale menu-bar card instead of the main window.

## v4.86

- Tuned the official `NSGlassEffectView` menu-bar card with a subtle dark tint so Clear glass reads closer to BetterDisplay's dark transparent overlay instead of a pale milky panel on bright backgrounds.

## v4.85

- Wrapped the menu-bar card in Apple's official `NSGlassEffectView` on macOS 26 and newer, using the system Clear/Regular glass styles instead of custom-drawn popover glass when available.

## v4.84

- Added visible macOS-style traffic-light controls inside the main window chrome as a fallback for environments where AppKit keeps the standard buttons accessible but visually hidden.

## v4.83

- Restored the main window to a standard titled chrome instead of full-size-content chrome so macOS traffic-light controls have a real visible titlebar area.

## v4.82

- Explicitly restores and raises macOS standard traffic-light window buttons above the custom titlebar fill layer so the main window controls remain visible after chrome refreshes.

## v4.81

- Kept the titlebar fill overlay behind macOS standard window buttons so the red/yellow/green traffic-light controls remain visible, while retaining the v4.80 higher-transparency menu-bar popover glass.

## v4.80

- Made the menu-bar popover's single glass sheet significantly more transparent in ultra-clear and clear modes, closer to BetterDisplay's menu overlay while keeping the no-double-layer control hierarchy.

## v4.79

- Removed the last popover-only glass pill from the refresh schedule label and deleted the unused popover glass-control helper so the menu-bar card keeps a single-sheet hierarchy.

## v4.78

- Restored a single continuous dark/light material fill behind the menu-bar popover after removing control bases, keeping the popover readable without reintroducing per-control double layers or horizontal bands.

## v4.77

- Removed the remaining popover control bases: menu-bar segmented controls no longer draw a capsule background, border, or divider lines, bottom action icons now sit directly on the single glass sheet, and glass highlights now use continuous full-panel gradients to avoid hard horizontal bands.

## v4.76

- Flattened the menu-bar popover and main-window controls to avoid duplicate glass layers: segmented controls and checkboxes now own their own surface, icon buttons use a lighter single-layer well, dark-mode inspector divider lines are suppressed, and main-window segmented controls now use the same stable custom selected state as the popover.

## v4.75

- Reused the cached exact follower string when syncing the main-window status row, avoiding an extra NSNumberFormatter pass during visible UI refreshes.

## v4.74

- Routed menu-bar status item initialization through the guarded status-item updater, seeding title, icon, length, and tooltip caches before the first live refresh.

## v4.73

- Added an identity fast path for card and popover history-value setters, avoiding repeated array copies and equality scans when visible surfaces resync the same sparkline data.

## v4.72

- Centralized main-window schedule-label syncing so frequent countdown ticks avoid rebuilding the visible label string when the displayed schedule text has not changed.

## v4.71

- Shared the follower sparkline drawing implementation between the main card and menu-bar popover, preserving each view's spacing while reducing duplicated path-building logic.

## v4.70

- Removed temporary NSArray, NSValue, and NSMutableArray allocations from the menu-bar popover backplate drawing path by using stack-backed button and rect arrays instead.

## v4.69

- Cached reusable system and monospaced-digit fonts in the main card, menu-bar popover, and self-drawn segmented controls, reducing repeated font creation during redraws while preserving the existing layout and glass styling.

## v4.68

- Cached reusable text-shadow objects for the main card and menu-bar popover drawing paths, reducing repeated NSShadow allocations during redraws without changing the liquid-glass appearance.

## v4.67

- Added a native passthrough titlebar fill layer and delayed chrome re-sync so dark-mode windows no longer fall back to a white titlebar strip after macOS rebuilds the system titlebar views.

## v4.66

- Routed reusable SF Symbol creation through one cached template-image helper, so page buttons, popover icon buttons, and card controls share the same AppKit image cache.

## v4.65

- Cached reusable template SF Symbol images for page buttons, popover icon buttons, and card controls, reducing repeated AppKit image creation while preserving existing fallbacks.

## v4.64

- Added guarded title and state updates for validated menu items, reducing redundant AppKit property writes when right-click and main-menu items are refreshed.

## v4.63

- Reused a single settings snapshot while syncing the menu-bar popover and main schedule label, reducing repeated defaults reads and keeping visible controls in the same refresh pass consistent.

## v4.62

- Reused the known auto-refresh enabled state while building countdown text for the menu bar item, popover, main schedule label, and status menu, reducing repeated settings reads during time-sensitive UI ticks.

## v4.61

- Reused the current UID, timestamp, and trend-range title within each history refresh/export pass, reducing repeated settings reads and keeping cutoff calculations consistent.

## v4.60

- Reused the native traffic-light button references while synchronizing titlebar chrome, avoiding repeated AppKit button lookups during recursive window-frame updates.

## v4.59

- Made native titlebar chrome synchronization use guarded writes, so dark-mode titlebar repair no longer forces redundant redraws during window activation, deactivation, or screen changes.

## v4.58

- Recursively synchronized the native macOS titlebar chrome with the app's dark liquid-glass surface, preventing the top strip from falling back to white in dark mode.
- Prefilled the current UID history cache immediately after saving a history point, avoiding a full UserDefaults history reread/filter/sort before the next trend refresh.

## v4.57

- Cached the app version and generated User-Agent string once per process, avoiding repeated bundle metadata reads and string formatting during Bilibili API refreshes.

## v4.56

- Cached the status-bar exact and compact follower-count strings, avoiding repeated number formatting during countdown-only menu bar updates when the follower count has not changed.

## v4.55

- Cached launch-at-login availability for the current process, avoiding repeated bundle-path checks while still reading the actual SMAppService enabled state live.

## v4.54

- Reused cached trend text for the main dashboard history label and trend CSV tooltip, keeping status-bar and main-page trend wording in one history-refresh pass instead of rebuilding those strings during every control sync.

## v4.53

- Cached the current trend-summary text when history metrics are refreshed, so status-bar tooltip countdown updates can reuse it instead of rebuilding the trend label every tick.

## v4.52

- Avoided redundant hidden/enabled/alpha writes on the main card's inline refresh and close buttons during hover and refresh-state updates, trimming small but frequent UI churn.

## v4.51

- Added change guards to the main liquid card's frequently synced display properties, avoiding redundant redraws when follower data, labels, trend points, or appearance values are already unchanged.

## v4.50

- Moved the main card's refreshing state into the refresh lifecycle itself, so the primary refresh button disables immediately during a request without relying on menu-bar popover synchronization and the popover no longer triggers unnecessary main-card redraws.

## v4.49

- Reduced the menu-bar popover's double-glass look by replacing its full custom outer rim with a lighter inner sheen and lowering the redundant custom shadow that sits on top of AppKit's native popover shadow.

## v4.48

- Reapplied the native window chrome when the main or preferences window becomes active, resigns active state, changes screens, or gets new backing properties, keeping the titlebar aligned with the liquid-glass surface after AppKit lifecycle changes.

## v4.47

- Reused the sorted-history cutoff index for trend baseline/range slicing so trend charts and trend CSV exports no longer scan every retained point to find the visible range.

## v4.46

- Reused the sorted-history cutoff index for one-week CSV filtering so exporting recent history can slice the range tail instead of scanning every retained history point.

## v4.45

- Counted in-range trend/history points with a binary search over already sorted history data, reducing repeated range-count work as long-running histories grow.

## v4.44

- Reused the status-bar schedule text while syncing the menu-bar popover, avoiding a second countdown/settings read during status title refreshes.

## v4.43

- Cached the resolved macOS light/dark appearance between system appearance notifications so repeated liquid-glass drawing no longer re-reads user defaults on every paint pass.

## v4.42

- Forced appearance-dependent segmented controls and glass buttons to redraw when macOS light/dark or reduced-transparency settings change, preventing stale control chrome after live appearance switches.

## v4.41

- Matched the preferences window's dark-mode glass wash to the main dashboard chrome so secondary settings surfaces no longer fall back to a flatter gray blur.

## v4.40

- Made light/dark chrome detection independent of the app's previously applied `NSApp.appearance`, so the titlebar can recover cleanly when the system or app-specific appearance returns to light mode.

## v4.39

- Explicitly synchronized the whole AppKit application appearance with the active system light/dark mode before applying window chrome, keeping the native titlebar from drifting back to light chrome in dark mode.

## v4.38

- Restored dark-mode titlebar consistency by opting the app bundle into system dark appearance and repainting the native titlebar chrome containers with the active window chrome color.

## v4.37

- Reused the current display-tick schedule text when updating the status-bar tooltip and visible countdown labels, avoiding duplicate countdown string calculation on each tick.

## v4.36

- Cached settings values within each main/preferences window sync pass to reduce repeated user-default reads during visible UI refreshes.

## v4.35

- Removed obsolete status-bar text width measurement now that the menu-bar item uses native variable length sizing.

## v4.34

- Extended guarded control updates to the menu-bar popover so its buttons and segmented controls avoid redundant enabled, alpha, tooltip, tint, state, and selected-segment writes.

## v4.33

- Added shared guarded-control setters and used them in main and preferences window syncs to avoid rewriting unchanged segment, checkbox, enabled, alpha, title, and tooltip state.

## v4.32

- Avoided repeated launch-at-login support checks while syncing the main window, preferences window, status popover, and status menu state.

## v4.31

- Removed additional redundant visible-surface syncs from UID switching and refresh-failure handling so refresh paths do less repeated UI work.

## v4.30

- Removed a duplicate main-window control sync from status bar title updates because popover syncing already refreshes all visible control surfaces.

## v4.29

- Reduced redundant high-frequency UI property writes while syncing visible main-window controls, status messages, and the menu-bar widget button.

## v4.28

- Forced the macOS titlebar frame, window background, and content roots to share the same system light/dark chrome so dark mode no longer regresses to a white title strip.
- Avoided redundant titlebar style and layer-background writes when the window chrome is already in the correct state.
- Centralized system appearance and accessibility-transparency refreshes through a main-thread-safe redraw path.
- Added one bounded retry for transient Bilibili request failures such as timeouts, dropped connections, HTTP 429, and 5xx responses.
- Kept business/API parsing failures non-retriable to avoid unnecessary background traffic and rate-limit pressure.

## v4.27

- Slimmed the liquid-glass layer stack so system blur is the single base layer and custom drawing only adds optical highlights, rim light, and soft depth.
- Removed the extra visual-effect layer behind the main preview card.
- Removed the extra visual-effect root from the menu bar popover and made its rim a single edge so the top card reads as one glass surface.
- Restored full-size titlebar chrome with a small safe area so dark mode no longer falls back to a white macOS title strip.
- Avoided writing and invalidating hidden menu bar popover views during routine status and refresh updates.
- Reworked main panels, preferences panels, popover controls, and segmented controls to avoid stacked glass backplates.
- Deleted the unused thick glass backdrop path to prevent future regressions back to double-layered surfaces.

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
