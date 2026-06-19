from PIL import Image, ImageDraw, ImageFont

W, H = 390, 844
BG_TOP = (247, 252, 255)
BG_MID = (235, 248, 252)
BG_BOTTOM = (255, 244, 239)
TEXT = (31, 41, 55)
DARK = (16, 24, 40)
MUTED = (102, 112, 133)
SECONDARY = (71, 84, 103)
BILI = (0, 161, 214)
BILI_DARK = (0, 122, 166)
CORAL = (255, 118, 96)
BORDER = (229, 231, 235)
WHITE = (255, 255, 255)

FONT = "/System/Library/AssetsV2/com_apple_MobileAsset_Font8/86ba2c91f017a3749571a82f2c6d890ac7ffb2fb.asset/AssetData/PingFang.ttc"


def font(size, bold=False):
    index = 1 if bold else 0
    return ImageFont.truetype(FONT, size, index=index)


def center_text(draw, text, y, fnt, fill):
    bbox = draw.textbbox((0, 0), text, font=fnt)
    x = (W - (bbox[2] - bbox[0])) / 2
    draw.text((x, y), text, font=fnt, fill=fill)


def rounded(draw, xy, radius, fill, outline=None, width=1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


img = Image.new("RGB", (W, H), BG_TOP)
draw = ImageDraw.Draw(img)

for y in range(H):
    if y < H * 0.55:
        t = y / (H * 0.55)
        c = tuple(int(BG_TOP[i] * (1 - t) + BG_MID[i] * t) for i in range(3))
    else:
        t = (y - H * 0.55) / (H * 0.45)
        c = tuple(int(BG_MID[i] * (1 - t) + BG_BOTTOM[i] * t) for i in range(3))
    draw.line([(0, y), (W, y)], fill=c)

hero = Image.new("RGBA", (W - 44, 362), (255, 255, 255, 176))
hero_draw = ImageDraw.Draw(hero)
hero_draw.rounded_rectangle((0, 0, W - 44, 362), radius=26, fill=(255, 255, 255, 176), outline=(255, 255, 255, 156), width=1)
img.paste(hero, (22, 44), hero)

badge = (109, 66, 281, 94)
rounded(draw, badge, 999, (226, 246, 252), (180, 230, 242), 1)
center_text(draw, "BILIBILI LIVE STAT", 73, font(11, True), BILI_DARK)

avatar = Image.new("RGBA", (68, 68), (0, 0, 0, 0))
ad = ImageDraw.Draw(avatar)
for x in range(68):
    t = x / 67
    c = tuple(int(BILI[i] * (1 - t) + CORAL[i] * t) for i in range(3))
    ad.line([(x, 0), (x, 68)], fill=c + (255,))
mask = Image.new("L", (68, 68), 0)
ImageDraw.Draw(mask).ellipse((0, 0, 67, 67), fill=255)
img.paste(avatar, (161, 116), mask)
center_text(draw, "谐", 132, font(26, True), WHITE)

center_text(draw, "谐门东西", 202, font(30, True), DARK)
center_text(draw, "实时关注者", 248, font(14), MUTED)
center_text(draw, "65,424", 286, font(56, True), BILI)
center_text(draw, "followers", 366, font(13, True), SECONDARY)

panel = Image.new("RGBA", (W - 44, 124), (255, 255, 255, 142))
pd = ImageDraw.Draw(panel)
pd.rounded_rectangle((0, 0, W - 44, 124), radius=18, fill=(255, 255, 255, 142), outline=(255, 255, 255, 120), width=1)
img.paste(panel, (22, 424), panel)

rounded(draw, (40, 440, 118, 472), 999, (226, 246, 252))
draw.text((52, 447), "刷新成功", font=font(14, True), fill=BILI_DARK)
draw.text((258, 447), "10s 自动刷新", font=font(14), fill=MUTED)
draw.text((40, 490), "UID: 3546718146661176", font=font(14), fill=MUTED)
draw.text((40, 518), "更新时间: 20:07:38", font=font(14), fill=MUTED)

button = Image.new("RGBA", (164, 50), (0, 0, 0, 0))
bd = ImageDraw.Draw(button)
for x in range(164):
    t = x / 163
    c2 = (int(BILI[0] * (1 - t) + 31 * t), int(BILI[1] * (1 - t) + 184 * t), int(BILI[2] * (1 - t) + 219 * t))
    bd.line([(x, 0), (x, 50)], fill=c2 + (255,))
bmask = Image.new("L", (164, 50), 0)
ImageDraw.Draw(bmask).rounded_rectangle((0, 0, 163, 49), radius=25, fill=255)
img.paste(button, (113, 590), bmask)
center_text(draw, "立即刷新", 604, font(16, True), WHITE)

center_text(draw, "bilibili public data", 684, font(12), (132, 143, 160))

phone = img

canvas = Image.new("RGB", (900, 844), (245, 248, 250))
canvas.paste(phone, (40, 0))
draw = ImageDraw.Draw(canvas)

draw.text((500, 78), "ColorOS 桌面小组件", font=font(28, True), fill=DARK)
draw.text((500, 120), "添加到桌面后可展示实时关注者数据，点按右上角刷新。", font=font(15), fill=MUTED)

wall = Image.new("RGB", (330, 500), (247, 252, 255))
wd = ImageDraw.Draw(wall)
for y in range(500):
    t = y / 499
    c = (
        int(239 * (1 - t) + 255 * t),
        int(250 * (1 - t) + 241 * t),
        int(255 * (1 - t) + 235 * t),
    )
    wd.line([(0, y), (330, y)], fill=c)
wd.rounded_rectangle((0, 0, 329, 499), radius=34, outline=(225, 235, 242), width=1)

widget = Image.new("RGBA", (270, 170), (0, 0, 0, 0))
shadow = Image.new("RGBA", (290, 190), (0, 0, 0, 0))
sd = ImageDraw.Draw(shadow)
sd.rounded_rectangle((10, 12, 280, 182), radius=28, fill=(40, 88, 120, 24))
wall.paste(shadow, (20, 150), shadow)
g = Image.new("RGBA", (270, 170), (255, 255, 255, 0))
gd = ImageDraw.Draw(g)
for y in range(170):
    t = y / 169
    c = (
        int(255 * (1 - t) + 244 * t),
        int(255 * (1 - t) + 253 * t),
        int(255 * (1 - t) + 255 * t),
        int(204 * (1 - t) + 154 * t),
    )
    gd.line([(0, y), (270, y)], fill=c)
mask = Image.new("L", (270, 170), 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, 269, 169), radius=26, fill=255)
widget.paste(g, (0, 0), mask)
wdraw = ImageDraw.Draw(widget)
wdraw.rounded_rectangle((0, 0, 269, 169), radius=26, outline=(255, 255, 255, 215), width=2)
wdraw.rounded_rectangle((18, 18, 96, 48), radius=15, fill=(226, 246, 252, 210), outline=(188, 229, 240, 160))
wdraw.text((30, 25), "实时统计", font=font(12, True), fill=BILI_DARK)
wdraw.ellipse((218, 16, 252, 50), fill=(0, 161, 214, 230))
wdraw.text((226, 20), "↻", font=font(18, True), fill=WHITE)
wdraw.text((18, 70), "谐门东西", font=font(17, True), fill=DARK)
wdraw.text((18, 96), "65,424", font=font(38, True), fill=BILI)
wdraw.text((18, 145), "更新 20:13", font=font(12), fill=MUTED)
wall.paste(widget, (30, 170), widget)

canvas.paste(wall, (500, 180))
draw.text((500, 718), "安装 APK 后：长按桌面 -> 小组件 -> 谐门东西", font=font(15, True), fill=DARK)
draw.text((500, 748), "系统周期刷新约 30 分钟；点击组件按钮会立即拉取最新数据。", font=font(14), fill=MUTED)

out = "outputs/b-up-fans-liquid-widget-preview.png"
canvas.save(out)
print(out)
