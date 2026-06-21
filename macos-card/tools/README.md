# macOS icon tools

`generate_app_icon.py` regenerates the tracked `AppIcon.icns` asset for the macOS app.

Requirements:

- macOS with `iconutil`
- Python 3
- Pillow (`python3 -m pip install Pillow`)

Usage:

```sh
python3 macos-card/tools/generate_app_icon.py
```

For visual inspection of the intermediate iconset:

```sh
python3 macos-card/tools/generate_app_icon.py --keep-iconset
```

The kept iconset is written to `work/AppIcon.iconset/`, which is ignored by git.
