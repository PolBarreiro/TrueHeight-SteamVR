"""Generate TrueHeight's PNG and multi-resolution Windows icon.

The artwork is intentionally constructed from simple project-owned geometric
shapes so every release can reproduce the exact same assets without stock art.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
SIZE = 1024
NAVY = "#0B1F33"
CYAN = "#22D3EE"
CORAL = "#FF6B5E"
WHITE = "#F4FBFF"
STEAM_ASSET_DIR = ROOT / "packaging" / "steam" / "store-assets"


def create_mark() -> Image.Image:
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    # Compact headset body. The cyan rim remains visible on light and dark UI.
    draw.rounded_rectangle(
        (96, 286, 928, 724), radius=190, fill=NAVY, outline=CYAN, width=48
    )
    # Shape the lower edge into the two cheek/strap lobes.
    draw.rounded_rectangle((144, 528, 380, 744), radius=106, fill=NAVY)
    draw.rounded_rectangle((644, 528, 880, 744), radius=106, fill=NAVY)
    draw.arc((96, 286, 928, 724), 0, 360, fill=CYAN, width=48)

    # Two bold lens bars survive down-sampling to tray-icon sizes.
    draw.line((240, 492, 424, 492), fill=CYAN, width=64)
    draw.line((600, 492, 784, 492), fill=CYAN, width=64)

    # The coral double arrow is the literal "height lock" cue.
    draw.line((512, 146, 512, 878), fill=CORAL, width=52)
    draw.polygon(((512, 92), (426, 208), (598, 208)), fill=CORAL)
    draw.polygon(((512, 932), (426, 816), (598, 816)), fill=CORAL)
    return image


def load_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    fonts = Path("C:/Windows/Fonts")
    candidates = (
        ["segoeuib.ttf", "arialbd.ttf"] if bold else ["segoeui.ttf", "arial.ttf"]
    )
    for name in candidates:
        path = fonts / name
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default(size=size)


def fit_font(text: str, max_width: int, initial_size: int) -> ImageFont.FreeTypeFont:
    size = initial_size
    while size > 12:
        font = load_font(size, bold=True)
        left, top, right, bottom = font.getbbox(text)
        if right - left <= max_width:
            return font
        size -= 2
    return load_font(12, bold=True)


def make_background(width: int, height: int) -> Image.Image:
    image = Image.new("RGBA", (width, height), "#071522")
    draw = ImageDraw.Draw(image, "RGBA")

    # Soft vertical color shift and a restrained height-grid motif.
    for y in range(height):
        amount = y / max(1, height - 1)
        color = (
            int(7 + 7 * amount),
            int(21 + 12 * amount),
            int(34 + 18 * amount),
            255,
        )
        draw.line((0, y, width, y), fill=color)

    grid_step = max(34, height // 7)
    for y in range(grid_step, height, grid_step):
        draw.line((0, y, width, y), fill=(34, 211, 238, 24), width=max(1, height // 350))
    for index, y in enumerate(range(grid_step, height, grid_step), start=1):
        tick = width // 28 if index % 2 else width // 18
        draw.line((0, y, tick, y), fill=(255, 107, 94, 125), width=max(2, height // 180))

    # A quiet cyan glow behind the main lock-up.
    glow_radius = int(min(width, height) * 0.56)
    glow_layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
    glow = ImageDraw.Draw(glow_layer, "RGBA")
    center = (int(width * 0.34), height // 2)
    for radius in range(glow_radius, 0, -max(2, glow_radius // 80)):
        alpha = int(18 * (1 - radius / glow_radius) ** 2)
        glow.ellipse(
            (
                center[0] - radius,
                center[1] - radius,
                center[0] + radius,
                center[1] + radius,
            ),
            fill=(34, 211, 238, alpha),
        )
    return Image.alpha_composite(image, glow_layer)


def paste_contained(
    canvas: Image.Image, artwork: Image.Image, box: tuple[int, int, int, int]
) -> None:
    left, top, right, bottom = box
    max_width = max(1, right - left)
    max_height = max(1, bottom - top)
    scale = min(max_width / artwork.width, max_height / artwork.height)
    size = (max(1, int(artwork.width * scale)), max(1, int(artwork.height * scale)))
    resized = artwork.resize(size, Image.Resampling.LANCZOS)
    position = (
        left + (max_width - size[0]) // 2,
        top + (max_height - size[1]) // 2,
    )
    canvas.alpha_composite(resized, position)


def draw_wordmark(
    canvas: Image.Image,
    box: tuple[int, int, int, int],
    include_subtitle: bool = True,
) -> None:
    left, top, right, bottom = box
    draw = ImageDraw.Draw(canvas)
    width = right - left
    height = bottom - top
    title = "TRUEHEIGHT"
    title_font = fit_font(title, width, max(24, int(height * 0.42)))
    title_box = draw.textbbox((0, 0), title, font=title_font)
    title_height = title_box[3] - title_box[1]
    subtitle = "STEAMVR HEIGHT STABILIZER"
    subtitle_font = fit_font(subtitle, width, max(11, int(height * 0.12)))
    subtitle_box = draw.textbbox((0, 0), subtitle, font=subtitle_font)
    subtitle_height = subtitle_box[3] - subtitle_box[1]
    gap = max(4, height // 28)
    total_height = title_height + (gap + subtitle_height if include_subtitle else 0)
    content_top = top + (height - total_height) // 2
    title_y = content_top - title_box[1]
    draw.text((left, title_y), title, font=title_font, fill=WHITE)
    if include_subtitle:
        subtitle_top = content_top + title_height + gap
        subtitle_y = subtitle_top - subtitle_box[1]
        draw.text((left, subtitle_y), subtitle, font=subtitle_font, fill=CYAN)


def make_landscape(width: int, height: int, include_subtitle: bool = True) -> Image.Image:
    canvas = make_background(width, height)
    padding = max(12, int(height * 0.10))
    mark_width = min(int(width * 0.40), int(height * 0.88))
    paste_contained(canvas, create_mark(), (padding, padding, padding + mark_width, height - padding))
    text_left = padding + mark_width + max(12, int(width * 0.025))
    draw_wordmark(
        canvas,
        (text_left, padding, width - padding, height - padding),
        include_subtitle=include_subtitle,
    )
    return canvas


def make_portrait(width: int, height: int) -> Image.Image:
    canvas = make_background(width, height)
    padding = max(18, int(width * 0.08))
    paste_contained(
        canvas,
        create_mark(),
        (padding, int(height * 0.07), width - padding, int(height * 0.58)),
    )
    draw_wordmark(
        canvas,
        (padding, int(height * 0.59), width - padding, int(height * 0.88)),
        include_subtitle=True,
    )
    return canvas


def make_library_logo() -> Image.Image:
    canvas = Image.new("RGBA", (1280, 720), (0, 0, 0, 0))
    paste_contained(canvas, create_mark(), (100, 170, 470, 550))
    draw_wordmark(canvas, (500, 190, 1180, 530), include_subtitle=False)
    return canvas


def main() -> None:
    image = create_mark()
    png_path = ROOT / "docs" / "images" / "trueheight-logo-v2.png"
    ico_path = ROOT / "res" / "trueheight-icon-v2.ico"
    image.save(png_path, optimize=True)
    image.save(
        ico_path,
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    STEAM_ASSET_DIR.mkdir(parents=True, exist_ok=True)
    assets = {
        "header-capsule-920x430.png": make_landscape(920, 430),
        "small-capsule-462x174.png": make_landscape(462, 174, include_subtitle=False),
        "main-capsule-1232x706.png": make_landscape(1232, 706),
        "vertical-capsule-748x896.png": make_portrait(748, 896),
        "library-capsule-600x900.png": make_portrait(600, 900),
        "library-hero-3840x1240.png": make_landscape(3840, 1240),
        "library-logo-1280x720.png": make_library_logo(),
        "library-header-920x430.png": make_landscape(920, 430, include_subtitle=False),
    }
    for name, asset in assets.items():
        asset.save(STEAM_ASSET_DIR / name, optimize=True)
    print(png_path)
    print(ico_path)
    for name in assets:
        print(STEAM_ASSET_DIR / name)


if __name__ == "__main__":
    main()
