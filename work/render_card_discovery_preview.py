from PIL import Image, ImageDraw, ImageFont

ROOT = "/Users/asgorelee/Documents/Codex/2026-06-13/b-up-app"
FONT = "/System/Library/AssetsV2/com_apple_MobileAsset_Font8/86ba2c91f017a3749571a82f2c6d890ac7ffb2fb.asset/AssetData/PingFang.ttc"


def font(size, bold=False):
    return ImageFont.truetype(FONT, size, index=1 if bold else 0)


canvas = Image.new("RGB", (900, 620), (246, 249, 252))
draw = ImageDraw.Draw(canvas)

draw.text((48, 44), "App 图标与 ColorOS 卡片发现页", font=font(30, True), fill=(16, 24, 40))
draw.text((48, 88), "已加入启动图标、卡片名称、描述和预览图。", font=font(16), fill=(102, 112, 133))

icon = Image.open(f"{ROOT}/app/src/main/res/drawable-nodpi/avatar_xiemen.png").resize((132, 132))
card = Image.open(f"{ROOT}/app/src/main/res/drawable-nodpi/widget_preview.png").resize((432, 216))

draw.rounded_rectangle((48, 150, 380, 372), radius=28, fill=(255, 255, 255), outline=(229, 235, 242), width=1)
canvas.paste(icon, (82, 194), icon)
draw.text((236, 204), "谐门东西", font=font(21, True), fill=(16, 24, 40))
draw.text((236, 240), "实时关注者 / 桌面卡片", font=font(15), fill=(102, 112, 133))
draw.text((236, 292), "谐门东西卡片", font=font(15, True), fill=(0, 122, 166))

draw.rounded_rectangle((430, 150, 852, 450), radius=30, fill=(12, 22, 34), outline=(186, 238, 255), width=1)
canvas.paste(card, (425, 192), card)
draw.text((468, 406), "卡片预览图", font=font(16, True), fill=(244, 250, 255))
draw.text((468, 434), "ColorOS 小组件/卡片列表会使用该预览", font=font(13), fill=(190, 207, 221))

draw.text((48, 510), "说明：ColorOS 桌面“卡片/小组件”入口识别的是 Android AppWidget 元数据；负一屏等 OPPO 私有卡片入口可能还需要厂商 SDK/审核。", font=font(14), fill=(102, 112, 133))

out = f"{ROOT}/outputs/b-up-fans-card-discovery-preview.png"
canvas.save(out)
print(out)
