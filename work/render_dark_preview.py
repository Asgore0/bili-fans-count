from PIL import Image, ImageDraw, ImageFont

W, H = 900, 844
PHONE_W, PHONE_H = 390, 844
FONT = "/System/Library/AssetsV2/com_apple_MobileAsset_Font8/86ba2c91f017a3749571a82f2c6d890ac7ffb2fb.asset/AssetData/PingFang.ttc"

DARK = (244, 250, 255)
MUTED = (190, 207, 221)
SUBTLE = (122, 146, 164)
CYAN = (75, 217, 255)
CYAN_TEXT = (127, 229, 255)
WHITE = (255, 255, 255)


def font(size, bold=False):
    return ImageFont.truetype(FONT, size, index=1 if bold else 0)


def center(draw, text, x0, x1, y, fnt, fill):
    box = draw.textbbox((0, 0), text, font=fnt)
    draw.text((x0 + (x1 - x0 - box[2] + box[0]) / 2, y), text, font=fnt, fill=fill)


def rounded(draw, xy, radius, fill, outline=None, width=1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


phone = Image.new("RGB", (PHONE_W, PHONE_H), (7, 15, 26))
draw = ImageDraw.Draw(phone)
for y in range(PHONE_H):
    if y < PHONE_H * 0.55:
        t = y / (PHONE_H * 0.55)
        a, b = (7, 15, 26), (9, 31, 45)
    else:
        t = (y - PHONE_H * 0.55) / (PHONE_H * 0.45)
        a, b = (9, 31, 45), (32, 18, 37)
    c = tuple(int(a[i] * (1 - t) + b[i] * t) for i in range(3))
    draw.line([(0, y), (PHONE_W, y)], fill=c)

hero = Image.new("RGBA", (PHONE_W - 44, 362), (22, 34, 48, 130))
hd = ImageDraw.Draw(hero)
hd.rounded_rectangle((0, 0, PHONE_W - 45, 361), radius=28, fill=(22, 34, 48, 130), outline=(186, 238, 255, 88), width=1)
hd.rounded_rectangle((1, 1, PHONE_W - 46, 80), radius=27, fill=(255, 255, 255, 30))
phone.paste(hero, (22, 44), hero)

rounded(draw, (105, 66, 285, 94), 999, (35, 85, 105), (80, 145, 165), 1)
center(draw, "BILIBILI LIVE STAT", 105, 285, 73, font(11, True), CYAN_TEXT)

avatar = Image.new("RGBA", (68, 68), (0, 0, 0, 0))
ad = ImageDraw.Draw(avatar)
for x in range(68):
    t = x / 67
    c = (int(0 * (1 - t) + 89 * t), int(151 * (1 - t) + 221 * t), int(204 * (1 - t) + 255 * t), 255)
    ad.line([(x, 0), (x, 68)], fill=c)
mask = Image.new("L", (68, 68), 0)
ImageDraw.Draw(mask).ellipse((0, 0, 67, 67), fill=255)
phone.paste(avatar, (161, 116), mask)
center(draw, "谐", 161, 229, 132, font(26, True), WHITE)

center(draw, "谐门东西", 22, PHONE_W - 22, 202, font(30, True), DARK)
center(draw, "实时关注者", 22, PHONE_W - 22, 248, font(14), MUTED)
center(draw, "65,424", 22, PHONE_W - 22, 286, font(56, True), CYAN)
center(draw, "followers", 22, PHONE_W - 22, 366, font(13, True), (151, 174, 191))

panel = Image.new("RGBA", (PHONE_W - 44, 124), (22, 34, 48, 118))
pd = ImageDraw.Draw(panel)
pd.rounded_rectangle((0, 0, PHONE_W - 45, 123), radius=20, fill=(22, 34, 48, 118), outline=(186, 238, 255, 75), width=1)
pd.rounded_rectangle((1, 1, PHONE_W - 46, 52), radius=19, fill=(255, 255, 255, 24))
phone.paste(panel, (22, 424), panel)
rounded(draw, (40, 440, 118, 472), 999, (32, 77, 96))
draw.text((52, 447), "刷新成功", font=font(14, True), fill=CYAN_TEXT)
draw.text((258, 447), "10s 自动刷新", font=font(14), fill=MUTED)
draw.text((40, 490), "UID: 3546718146661176", font=font(14), fill=MUTED)
draw.text((40, 518), "更新时间: 20:28:00", font=font(14), fill=MUTED)

rounded(draw, (113, 590, 277, 640), 25, (0, 151, 204))
center(draw, "立即刷新", 113, 277, 604, font(16, True), WHITE)
center(draw, "bilibili public data", 22, PHONE_W - 22, 684, font(12), SUBTLE)

canvas = Image.new("RGB", (W, H), (6, 11, 18))
canvas.paste(phone, (40, 0))
cd = ImageDraw.Draw(canvas)
cd.text((500, 78), "黑色模式透明效果", font=font(28, True), fill=DARK)
cd.text((500, 120), "主界面和 ColorOS 小组件都会切到暗色 Liquid Glass。", font=font(15), fill=MUTED)

wall = Image.new("RGB", (330, 500), (7, 15, 26))
wd = ImageDraw.Draw(wall)
for y in range(500):
    t = y / 499
    c = (int(7 * (1 - t) + 32 * t), int(15 * (1 - t) + 18 * t), int(26 * (1 - t) + 37 * t))
    wd.line([(0, y), (330, y)], fill=c)
wd.rounded_rectangle((0, 0, 329, 499), radius=34, outline=(40, 66, 82), width=1)
widget = Image.new("RGBA", (270, 170), (0, 0, 0, 0))
w = ImageDraw.Draw(widget)
w.rounded_rectangle((0, 0, 269, 169), radius=26, fill=(22, 34, 48, 150), outline=(186, 238, 255, 100), width=2)
for y in range(170):
    alpha = max(0, 26 - int(y * 0.11))
    w.line([(1, y + 1), (268, y + 1)], fill=(255, 255, 255, alpha))
w.text((18, 18), "谐门东西", font=font(13, True), fill=DARK)
w.text((236, 17), "↻", font=font(14, True), fill=CYAN_TEXT)
w.text((64, 54), "65,424", font=font(44, True), fill=CYAN)
w.text((18, 140), "实时统计", font=font(12, True), fill=CYAN_TEXT)
w.text((178, 140), "更新 20:28", font=font(12), fill=MUTED)
wall.paste(widget, (30, 170), widget)
canvas.paste(wall, (500, 180))
cd.text((500, 718), "跟随系统深色模式自动切换", font=font(15, True), fill=DARK)
cd.text((500, 748), "保留半透明、边缘高光和桌面小组件刷新能力。", font=font(14), fill=MUTED)

out = "outputs/b-up-fans-dark-liquid-preview.png"
canvas.save(out)
print(out)
