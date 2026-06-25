# BILI Fans Count

实时显示 B 站 UP 主粉丝数的桌面与移动端小工具。当前主线是 macOS 菜单栏/桌面卡片应用，仓库也保留 Android AppWidget / ColorOS 卡片版本源码。

默认监控账号为「谐门东西」，也可以输入 B 站 UID 或空间链接切换到任意 UP 主。

## Highlights

- macOS 原生 AppKit 应用，菜单栏数字、小窗卡片和完整程序页共享同一份数据。
- 支持 UID / B 站空间链接输入，切换账号时优先展示本地缓存，再刷新实时数据。
- 自动刷新间隔可选，支持手动刷新、粉丝变化提示、登录启动、窗口置顶和仅顶栏模式。
- 7 / 30 / 90 天趋势折线图，近 7 天 CSV 一键复制。
- 长期历史按 UID 和月份落盘到本地 CSV 文件，同时保留轻量 UI 缓存。
- macOS 暗黑模式下使用贴近系统风格的液态玻璃视觉。
- Android 版本包含桌面小组件、ColorOS/HyperOS 兼容入口和失败缓存显示。

## Project Layout

```text
.
├── app/                         Android app and widgets
├── macos-card/                  Formal macOS app
│   ├── src/BILIFansCard.m       Thin macOS app entry point
│   ├── src/parts/               Domain-oriented AppKit implementation parts
│   ├── tests/UIDParsingTest.m   macOS unit-style checks
│   ├── package-macos.sh         Universal app + DMG packaging script
│   └── BILIFansCard.app/        Source bundle metadata and icon
├── work/                        Asset generation helpers
└── outputs/                     Local build artifacts, ignored by git
```

`macos-card-special/` is intentionally treated as a local special edition and is not part of the formal GitHub source set.

The macOS app is intentionally kept as one Objective-C translation unit so the
lightweight tests can import the entry point and exercise internal static
helpers. Implementation details live in `macos-card/src/parts/` by domain:

- `BFCShared.inc`: constants, formatting, glass drawing, parsing, and history helpers.
- `BFCSettings.inc`: defaults, latest-result cache, history cache, and CSV persistence.
- `BFCBiliClient.inc`: Bilibili API requests and fallback behavior.
- `BFCViews.inc`: liquid-glass card, dashboard, preferences, and menu-bar panel views.
- `BFCAppDelegate.inc`: menus, windows, status item, refresh lifecycle, and commands.

## Requirements

### macOS

- macOS 12 or later
- Xcode Command Line Tools
- `clang`, `codesign`, `hdiutil`

### Android

- JDK 17
- Android Gradle Plugin 8.7.x
- Android SDK compileSdk 35

## Build And Test

### macOS Checks

```bash
./macos-card/check-macos.sh
```

### macOS DMG

```bash
./macos-card/package-macos.sh verified
```

The DMG is written to `outputs/`.

### Android Debug APK

```bash
./gradlew :app:assembleDebug
```

## Local Data

The macOS app stores settings in `NSUserDefaults`. Historical CSV files are written under:

```text
~/Library/Application Support/com.example.bilifanscard/HistoryCSV/<uid>/<yyyy-MM>.csv
```

The app reads public Bilibili endpoints:

- `https://api.bilibili.com/x/web-interface/card`
- `https://api.bilibili.com/x/relation/stat`

No server component is included in this project.

## Release Notes

The latest packaged local release is `v4.87`. Generated DMGs and APKs are intentionally ignored by git; publish them as GitHub Release assets when needed.

## License

MIT. See [LICENSE](LICENSE).
