from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = "/Users/asgorelee/Documents/Codex/2026-06-13/b-up-app"
FONT = "/System/Library/AssetsV2/com_apple_MobileAsset_Font8/86ba2c91f017a3749571a82f2c6d890ac7ffb2fb.asset/AssetData/PingFang.ttc"


def font(size, bold=False):
    return ImageFont.truetype(FONT, size, index=1 if bold else 0)


def text_shadow(draw, xy, text, fnt, fill, shadow=(0, 18, 28, 170), blur=False):
    x, y = xy
    if blur:
        layer = Image.new("RGBA", draw.im.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        ld.text((x, y + 2), text, font=fnt, fill=shadow)
        layer = layer.filter(ImageFilter.GaussianBlur(2))
        draw.bitmap((0, 0), layer, fill=None)
    else:
        draw.text((x, y + 2), text, font=fnt, fill=shadow)
    draw.text((x, y), text, font=fnt, fill=fill)


def center_text(draw, box, text, fnt, fill, shadow=(0, 18, 28, 170)):
    x0, y0, x1, y1 = box
    bbox = draw.textbbox((0, 0), text, font=fnt)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    text_shadow(draw, ((x0 + x1 - w) / 2, (y0 + y1 - h) / 2 - 2), text, fnt, fill, shadow)


def wallpaper(size, dark=True):
    w, h = size
    img = Image.new("RGBA", size, (0, 0, 0, 255))
    d = ImageDraw.Draw(img)
    if dark:
        top, mid, bottom = (7, 14, 24), (10, 36, 50), (36, 22, 44)
    else:
        top, mid, bottom = (231, 244, 251), (245, 251, 255), (255, 238, 236)
    for y in range(h):
        if y < h * 0.55:
            t = y / (h * 0.55)
            a, b = top, mid
        else:
            t = (y - h * 0.55) / (h * 0.45)
            a, b = mid, bottom
        c = tuple(int(a[i] * (1 - t) + b[i] * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=c + (255,))
    return img


def glass_card(size, dark=True):
    w, h = size
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    shadow = Image.new("RGBA", size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle((36, 46, w - 36, h - 34), radius=32, fill=(0, 18, 28, 22 if dark else 12))
    shadow = shadow.filter(ImageFilter.GaussianBlur(18))
    layer.alpha_composite(shadow)

    card = Image.new("RGBA", size, (0, 0, 0, 0))
    cd = ImageDraw.Draw(card)
    box = (46, 36, w - 46, h - 38)
    fill = (18, 34, 48, 31) if dark else (255, 255, 255, 20)
    stroke = (186, 238, 255, 52) if dark else (255, 255, 255, 48)
    cd.rounded_rectangle(box, radius=30, fill=fill, outline=stroke, width=1)

    # Thin refraction glints: enough to read as glass, still highly transparent.
    for offset, alpha in ((0, 58), (5, 22)):
        cd.arc((box[0] + 30, box[1] + 16 + offset, box[2] - 30, box[1] + 104 + offset),
               196, 344, fill=(255, 255, 255, alpha if dark else max(26, alpha - 14)), width=1)
    cd.arc((box[0] + 18, box[1] + 10, box[2] - 18, box[3] - 10),
           36, 84, fill=(126, 232, 255, 34 if dark else 24), width=1)
    cd.arc((box[0] + 22, box[1] + 10, box[2] - 22, box[3] - 12),
           220, 268, fill=(255, 118, 164, 24 if dark else 16), width=1)
    cd.line((box[0] + 52, box[1] + 54, box[2] - 54, box[1] + 22),
            fill=(255, 255, 255, 18 if dark else 12), width=1)
    layer.alpha_composite(card)
    return layer


def draw_widget(path, size, show_name=True):
    w, h = size
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    card = glass_card(size, dark=True)
    img.alpha_composite(card)
    d = ImageDraw.Draw(img)
    left, top, right, bottom = 84, 72, w - 84, h - 74

    if show_name:
        text_shadow(d, (left + 2, top + 22), "谐门东西", font(28, True), (246, 253, 255, 245))
    refresh_x = right - 58
    d.arc((refresh_x, top + 18, refresh_x + 38, top + 56), 32, 318, fill=(126, 232, 255, 230), width=4)
    d.polygon([(refresh_x + 35, top + 36), (refresh_x + 48, top + 28), (refresh_x + 42, top + 45)],
              fill=(126, 232, 255, 230))

    center_text(d, (left, top + 82, right, top + 168), "65,424", font(76, True), (75, 217, 255, 255))
    text_shadow(d, (left, bottom - 42), "实时统计", font(22, True), (132, 238, 255, 240))
    updated = "20:28 更新"
    bbox = d.textbbox((0, 0), updated, font=font(22))
    text_shadow(d, (right - bbox[2], bottom - 42), updated, font(22), (232, 244, 250, 230))
    img.save(path)


def draw_discovery(path):
    canvas = Image.new("RGB", (900, 620), (246, 249, 252))
    d = ImageDraw.Draw(canvas)
    d.text((48, 44), "App 图标与 ColorOS 卡片发现页", font=font(30, True), fill=(16, 24, 40))
    d.text((48, 88), "预览图保留高透明度，并补回玻璃边缘与折射感。", font=font(16), fill=(102, 112, 133))

    icon = Image.open(f"{ROOT}/app/src/main/res/drawable-nodpi/avatar_xiemen.png").resize((132, 132))
    widget = Image.open(f"{ROOT}/app/src/main/res/drawable-nodpi/widget_preview.png").resize((432, 216))
    d.rounded_rectangle((48, 150, 380, 372), radius=28, fill=(255, 255, 255), outline=(229, 235, 242), width=1)
    canvas.paste(icon, (82, 194), icon)
    d.text((236, 204), "BILI粉丝数", font=font(21, True), fill=(16, 24, 40))
    d.text((236, 240), "实时关注者 / 桌面卡片", font=font(15), fill=(102, 112, 133))
    d.text((236, 292), "谐门东西", font=font(15, True), fill=(0, 122, 166))

    d.rounded_rectangle((430, 150, 852, 450), radius=30, fill=(12, 22, 34), outline=(74, 120, 142), width=1)
    canvas.paste(widget, (425, 192), widget)
    d.text((468, 406), "卡片预览图", font=font(16, True), fill=(244, 250, 255))
    d.text((468, 434), "ColorOS 小组件/卡片列表会使用该预览", font=font(13), fill=(190, 207, 221))
    d.text((48, 510), "说明：负一屏私有入口仍取决于厂商卡片 SDK/审核；桌面小组件入口可直接识别 Android AppWidget。", font=font(14), fill=(102, 112, 133))
    canvas.save(path)


draw_widget(f"{ROOT}/app/src/main/res/drawable-nodpi/widget_preview.png", (720, 360), True)
draw_widget(f"{ROOT}/app/src/main/res/drawable-nodpi/card_preview.png", (720, 300), False)
draw_discovery(f"{ROOT}/outputs/b-up-fans-card-discovery-preview.png")
print("updated high transparency previews")
