from PIL import Image, ImageDraw, ImageFont

ROOT = "/Users/asgorelee/Documents/Codex/2026-06-13/b-up-app"
FONT = "/System/Library/AssetsV2/com_apple_MobileAsset_Font8/86ba2c91f017a3749571a82f2c6d890ac7ffb2fb.asset/AssetData/PingFang.ttc"


def font(size, bold=False):
    return ImageFont.truetype(FONT, size, index=1 if bold else 0)


canvas = Image.new("RGB", (760, 420), (246, 249, 252))
draw = ImageDraw.Draw(canvas)
draw.text((48, 42), "主屏图标居中修正", font=font(30, True), fill=(16, 24, 40))
draw.text((48, 86), "重新调整了图标里的文字视觉重心，让 launcher 上看起来更居中。", font=font(16), fill=(102, 112, 133))

icon = Image.open(f"{ROOT}/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png").resize((160, 160))
avatar = Image.open(f"{ROOT}/app/src/main/res/drawable-nodpi/avatar_xiemen.png").resize((160, 160))

draw.rounded_rectangle((64, 150, 312, 350), radius=30, fill=(255, 255, 255), outline=(229, 235, 242), width=1)
canvas.paste(icon, (108, 170), icon)
draw.text((126, 338), "Launcher", font=font(15, True), fill=(102, 112, 133))

draw.rounded_rectangle((396, 150, 644, 350), radius=30, fill=(255, 255, 255), outline=(229, 235, 242), width=1)
canvas.paste(avatar, (440, 170), avatar)
draw.text((470, 338), "App 内头像", font=font(15, True), fill=(102, 112, 133))

out = f"{ROOT}/outputs/b-up-fans-icon-centered-preview.png"
canvas.save(out)
print(out)
