import math

from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = "/Users/asgorelee/Documents/Codex/2026-06-13/b-up-app"
FONT = "/System/Library/AssetsV2/com_apple_MobileAsset_Font8/86ba2c91f017a3749571a82f2c6d890ac7ffb2fb.asset/AssetData/PingFang.ttc"


def f(size, bold=False):
    return ImageFont.truetype(FONT, size, index=1 if bold else 0)


def gradient(size, colors):
    w, h = size
    img = Image.new("RGBA", size)
    d = ImageDraw.Draw(img)
    c1, c2, c3 = colors
    for y in range(h):
        for x in range(w):
            tx = x / max(1, w - 1)
            ty = y / max(1, h - 1)
            t = (tx + ty) / 2
            if t < 0.5:
                u = t / 0.5
                a, b = c1, c2
            else:
                u = (t - 0.5) / 0.5
                a, b = c2, c3
            d.point((x, y), tuple(int(a[i] * (1 - u) + b[i] * u) for i in range(4)))
    return img


def rounded_mask(size, radius):
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius=radius, fill=255)
    return mask


def center_text(draw, box, text, font, fill):
    b = draw.textbbox((0, 0), text, font=font)
    x = box[0] + (box[2] - box[0] - (b[2] - b[0])) / 2
    y = box[1] + (box[3] - box[1] - (b[3] - b[1])) / 2 - b[1] / 2
    draw.text((x, y), text, font=font, fill=fill)


def shadow_text(draw, pos, text, font, fill, shadow=(0, 20, 34, 86), offset=(2, 2)):
    draw.text((pos[0] + offset[0], pos[1] + offset[1]), text, font=font, fill=shadow)
    draw.text(pos, text, font=font, fill=fill)


def shadow_center_text(draw, box, text, font, fill, shadow=(0, 20, 34, 98), offset=(0, 3)):
    shifted = (box[0] + offset[0], box[1] + offset[1], box[2] + offset[0], box[3] + offset[1])
    center_text(draw, shifted, text, font, shadow)
    center_text(draw, box, text, font, fill)


def draw_refresh_icon(draw, center, radius, fill, width):
    cx, cy = center
    box = (cx - radius, cy - radius, cx + radius, cy + radius)
    draw.arc(box, 34, 326, fill=fill, width=width)
    angle = math.radians(34)
    x = cx + radius * math.cos(angle)
    y = cy + radius * math.sin(angle)
    draw.polygon(
        [
            (x + radius * 0.42, y - radius * 0.02),
            (x - radius * 0.06, y - radius * 0.32),
            (x + radius * 0.02, y + radius * 0.26),
        ],
        fill=fill,
    )


def make_icon(size):
    scale = size / 512
    base = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    bg = gradient(
        (size, size),
        [
            (8, 20, 34, 255),
            (0, 161, 214, 255),
            (255, 118, 96, 255),
        ],
    )
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, size - 1, size - 1), fill=255)
    base.paste(bg, (0, 0), mask)

    shine = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shine)
    sd.ellipse((int(-70 * scale), int(-110 * scale), int(430 * scale), int(260 * scale)), fill=(255, 255, 255, 56))
    sd.ellipse((int(250 * scale), int(280 * scale), int(620 * scale), int(620 * scale)), fill=(255, 255, 255, 38))
    base.alpha_composite(shine)

    glass = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glass)
    inset = int(58 * scale)
    gd.ellipse(
        (inset, inset, size - inset, size - inset),
        fill=(255, 255, 255, 56),
        outline=(255, 255, 255, 128),
        width=max(1, int(4 * scale)),
    )
    base.alpha_composite(glass)

    draw = ImageDraw.Draw(base)
    center_text(draw, (0, int(12 * scale), size, size), "谐", f(int(242 * scale), True), (255, 255, 255, 245))
    return base


def make_avatar():
    size = 512
    icon = make_icon(size)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, size - 1, size - 1), fill=255)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    out.paste(icon, (0, 0), mask)
    return out


def make_widget_preview():
    w, h = 720, 360
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    shadow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle((36, 46, w - 36, h - 34), radius=32, fill=(0, 18, 28, 22))
    shadow = shadow.filter(ImageFilter.GaussianBlur(18))
    img.alpha_composite(shadow)

    card = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    cd = ImageDraw.Draw(card)
    box = (46, 36, w - 46, h - 38)
    cd.rounded_rectangle(box, radius=30, fill=(18, 34, 48, 31), outline=(186, 238, 255, 52), width=1)
    for offset, alpha in ((0, 58), (5, 22)):
        cd.arc((box[0] + 30, box[1] + 16 + offset, box[2] - 30, box[1] + 104 + offset),
               196, 344, fill=(255, 255, 255, alpha), width=1)
    cd.arc((box[0] + 18, box[1] + 10, box[2] - 18, box[3] - 10),
           36, 84, fill=(126, 232, 255, 34), width=1)
    cd.arc((box[0] + 22, box[1] + 10, box[2] - 22, box[3] - 12),
           220, 268, fill=(255, 118, 164, 24), width=1)
    cd.line((box[0] + 52, box[1] + 54, box[2] - 54, box[1] + 22),
            fill=(255, 255, 255, 18), width=1)
    shadow_text(cd, (84, 94), "谐门东西", f(28, True), (246, 253, 255, 245), shadow=(0, 18, 28, 170), offset=(0, 2))
    draw_refresh_icon(cd, (604, 108), 18, (126, 232, 255, 230), 4)
    shadow_center_text(cd, (84, 154, w - 84, 238), "65,424", f(76, True), (75, 217, 255, 255), shadow=(0, 18, 28, 170), offset=(0, 2))
    shadow_text(cd, (84, 276), "实时统计", f(22, True), (132, 238, 255, 240), shadow=(0, 18, 28, 170), offset=(0, 2))
    shadow_text(cd, (544, 276), "20:28 更新", f(22), (232, 244, 250, 230), shadow=(0, 18, 28, 170), offset=(0, 2))
    img.alpha_composite(card)
    return img


def make_card_preview():
    w, h = 720, 300
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    shadow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle((36, 46, w - 36, h - 34), radius=32, fill=(0, 18, 28, 18))
    shadow = shadow.filter(ImageFilter.GaussianBlur(18))
    img.alpha_composite(shadow)

    card = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    cd = ImageDraw.Draw(card)
    box = (46, 38, w - 46, h - 42)
    cd.rounded_rectangle(box, radius=30, fill=(18, 34, 48, 31), outline=(186, 238, 255, 52), width=1)
    for offset, alpha in ((0, 58), (5, 22)):
        cd.arc((box[0] + 30, box[1] + 16 + offset, box[2] - 30, box[1] + 104 + offset),
               196, 344, fill=(255, 255, 255, alpha), width=1)
    cd.arc((box[0] + 18, box[1] + 10, box[2] - 18, box[3] - 10),
           36, 84, fill=(126, 232, 255, 34), width=1)
    cd.arc((box[0] + 22, box[1] + 10, box[2] - 22, box[3] - 12),
           220, 268, fill=(255, 118, 164, 24), width=1)
    cd.line((box[0] + 52, box[1] + 54, box[2] - 54, box[1] + 22),
            fill=(255, 255, 255, 18), width=1)
    draw_refresh_icon(cd, (618, 108), 15, (126, 232, 255, 220), 4)
    shadow_center_text(cd, (84, 112, w - 84, 204), "65,424", f(74, True), (75, 217, 255, 255), shadow=(0, 18, 28, 170), offset=(0, 2))
    shadow_text(cd, (86, 226), "实时统计", f(22, True), (132, 238, 255, 240), shadow=(0, 18, 28, 170), offset=(0, 2))
    shadow_text(cd, (520, 226), "20:28 更新", f(22), (232, 244, 250, 230), shadow=(0, 18, 28, 170), offset=(0, 2))
    img.alpha_composite(card)
    return img


for dpi, size in {
    "mdpi": 48,
    "hdpi": 72,
    "xhdpi": 96,
    "xxhdpi": 144,
    "xxxhdpi": 192,
}.items():
    make_icon(size).save(f"{ROOT}/app/src/main/res/mipmap-{dpi}/ic_launcher.png")

make_avatar().save(f"{ROOT}/app/src/main/res/drawable-nodpi/avatar_xiemen.png")
make_widget_preview().save(f"{ROOT}/app/src/main/res/drawable-nodpi/widget_preview.png")
make_card_preview().save(f"{ROOT}/app/src/main/res/drawable-nodpi/card_preview.png")
print("generated icon, avatar, and clear glass widget previews")
