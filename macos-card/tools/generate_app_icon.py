from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:
    raise SystemExit(
        "Pillow is required to generate the macOS icon. Install it with: python3 -m pip install Pillow"
    ) from exc


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "macos-card" / "BILIFansCard.app" / "Contents" / "Resources" / "AppIcon.icns"
FONT_CANDIDATES = [
    Path("/System/Library/Fonts/SFNSRounded.ttf"),
    Path("/System/Library/Fonts/SFNS.ttf"),
    Path("/System/Library/Fonts/Helvetica.ttc"),
    Path("/System/Library/Fonts/Supplemental/Arial Bold.ttf"),
]


def font(size):
    for candidate in FONT_CANDIDATES:
        if not candidate.exists():
            continue
        try:
            return ImageFont.truetype(str(candidate), size)
        except OSError:
            continue
    raise SystemExit("No usable macOS system font was found for icon generation.")


def draw_icon(size):
    scale = size / 1024
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    radius = int(226 * scale)
    bounds = [int(48 * scale), int(48 * scale), int(976 * scale), int(976 * scale)]
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(bounds, radius=radius, fill=255)

    body = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    body_pixels = body.load()
    top = bounds[1]
    bottom = bounds[3]
    height = max(1, bottom - top)
    for y in range(bounds[1], bounds[3] + 1):
        t = (y - top) / height
        red = int(38 * (1 - t) + 0 * t)
        green = int(184 * (1 - t) + 128 * t)
        blue = int(255 * (1 - t) + 238 * t)
        for x in range(bounds[0], bounds[2] + 1):
            body_pixels[x, y] = (red, green, blue, 255)
    image.paste(body, (0, 0), mask)
    draw = ImageDraw.Draw(image)

    stroke_width = max(1, int(16 * scale))
    draw.rounded_rectangle(
        [bounds[0] + stroke_width // 2, bounds[1] + stroke_width // 2, bounds[2] - stroke_width // 2, bounds[3] - stroke_width // 2],
        radius=radius,
        outline=(255, 255, 255, 62),
        width=stroke_width,
    )

    letter_font = font(int(690 * scale))
    letter = "B"
    box = draw.textbbox((0, 0), letter, font=letter_font)
    text_width = box[2] - box[0]
    text_height = box[3] - box[1]
    x = (size - text_width) / 2 - box[0] - int(6 * scale)
    y = (size - text_height) / 2 - box[1] - int(28 * scale)
    draw.text((x, y), letter, font=letter_font, fill=(255, 255, 255, 255))
    return image


def save_iconset(iconset):
    base = draw_icon(1024)
    for points in [16, 32, 128, 256, 512]:
        for scale in [1, 2]:
            pixels = points * scale
            name = f"icon_{points}x{points}{'@2x' if scale == 2 else ''}.png"
            resample = Image.Resampling.LANCZOS
            base.resize((pixels, pixels), resample).save(iconset / name)


def run_iconutil(iconset):
    iconutil = shutil.which("iconutil")
    if not iconutil:
        raise SystemExit("iconutil was not found. Run this script on macOS with Xcode command line tools installed.")
    try:
        subprocess.run([iconutil, "-c", "icns", str(iconset), "-o", str(OUT)], check=True)
    except subprocess.CalledProcessError as exc:
        raise SystemExit(f"iconutil failed while converting {iconset} to {OUT}") from exc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--keep-iconset", action="store_true")
    args = parser.parse_args()

    OUT.parent.mkdir(parents=True, exist_ok=True)
    if args.keep_iconset:
        iconset = ROOT / "work" / "AppIcon.iconset"
        if iconset.exists():
            for item in iconset.iterdir():
                item.unlink()
        iconset.mkdir(parents=True, exist_ok=True)
        save_iconset(iconset)
        run_iconutil(iconset)
    else:
        with tempfile.TemporaryDirectory(prefix="bilifans-icon-") as tmp:
            iconset = Path(tmp) / "AppIcon.iconset"
            iconset.mkdir()
            save_iconset(iconset)
            run_iconutil(iconset)
    print(OUT)


if __name__ == "__main__":
    main()
