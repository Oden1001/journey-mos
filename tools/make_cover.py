#!/usr/bin/env python3
"""Compose Journey Mos cover art from a Journey screenshot."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "docs" / "assets" / "journey-ref.jpg"
OUT_RELEASE = ROOT / "dist" / "release"
OUT_DOCS = ROOT / "docs" / "assets"


def load_font(size: int) -> ImageFont.ImageFont:
    for p in (
        r"C:\Windows\Fonts\segoeui.ttf",
        r"C:\Windows\Fonts\calibri.ttf",
        r"C:\Windows\Fonts\arial.ttf",
    ):
        if Path(p).exists():
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def crop_to_ratio(img: Image.Image, ratio: float, bias_top: float = 0.25) -> Image.Image:
    sw, sh = img.size
    src_ratio = sw / sh
    if src_ratio > ratio:
        new_w = int(sh * ratio)
        left = (sw - new_w) // 2
        return img.crop((left, 0, left + new_w, sh))
    new_h = int(sw / ratio)
    top = int((sh - new_h) * bias_top)
    top = max(0, min(top, sh - new_h))
    return img.crop((0, top, sw, top + new_h))


def bottom_fade(img: Image.Image, start_frac: float = 0.5, max_a: int = 160) -> Image.Image:
    w, h = img.size
    base = img.convert("RGBA")
    overlay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    y0 = int(h * start_frac)
    for y in range(y0, h):
        t = (y - y0) / max(1, h - y0)
        a = int(max_a * (t**1.35))
        od.line([(0, y), (w, y)], fill=(20, 10, 30, a))
    return Image.alpha_composite(base, overlay)


def draw_title(img: Image.Image, title: str, sub: str) -> Image.Image:
    w, h = img.size
    draw = ImageDraw.Draw(img)
    title_font = load_font(max(48, w // 20))
    sub_font = load_font(max(22, w // 50))

    def size(font: ImageFont.ImageFont, text: str) -> tuple[int, int]:
        b = draw.textbbox((0, 0), text, font=font)
        return b[2] - b[0], b[3] - b[1]

    tw, th = size(title_font, title)
    sw, sh = size(sub_font, sub)
    tx = (w - tw) // 2
    ty = h - int(h * 0.20)
    sx = (w - sw) // 2
    sy = ty + th + max(12, h // 90)

    for dx, dy, a in ((0, 3, 120), (0, 6, 60), (2, 2, 80)):
        draw.text((tx + dx, ty + dy), title, font=title_font, fill=(0, 0, 0, a))
        draw.text((sx + dx, sy + dy), sub, font=sub_font, fill=(0, 0, 0, a))

    draw.text((tx, ty), title, font=title_font, fill=(255, 244, 230, 245))
    draw.text((sx, sy), sub, font=sub_font, fill=(230, 210, 190, 210))

    line_y = ty + th + 8
    line_w = min(280, max(120, tw // 2))
    lx = (w - line_w) // 2
    draw.rectangle([lx, line_y, lx + line_w, line_y + 2], fill=(255, 210, 150, 180))
    return img


def make_wide() -> Image.Image:
    img = Image.open(SRC).convert("RGB")
    img = crop_to_ratio(img, 16 / 9, bias_top=0.22)
    img = img.resize((1920, 1080), Image.Resampling.LANCZOS)
    img = ImageEnhance.Contrast(img).enhance(1.08)
    img = ImageEnhance.Color(img).enhance(1.05)
    img = bottom_fade(img)
    img = draw_title(img, "Journey Mos", "F8 Menu  ·  Companion Tools")
    return img.convert("RGB")


def make_square() -> Image.Image:
    img = Image.open(SRC).convert("RGB")
    sw, sh = img.size
    m = min(sw, sh)
    left = (sw - m) // 2
    top = max(0, (sh - m) // 5)
    img = img.crop((left, top, left + m, top + m)).resize((1080, 1080), Image.Resampling.LANCZOS)
    img = ImageEnhance.Contrast(img).enhance(1.08)
    img = bottom_fade(img, start_frac=0.62, max_a=170)
    img = draw_title(img, "Journey Mos", "F8 · Companion")
    return img.convert("RGB")


def main() -> int:
    if not SRC.exists():
        print(f"[!] missing reference: {SRC}")
        return 1
    OUT_RELEASE.mkdir(parents=True, exist_ok=True)
    OUT_DOCS.mkdir(parents=True, exist_ok=True)

    wide = make_wide()
    square = make_square()

    paths = [
        OUT_RELEASE / "cover.png",
        OUT_DOCS / "journey-mos-cover.png",
        OUT_RELEASE / "cover-square.png",
        OUT_DOCS / "journey-mos-cover-square.png",
    ]
    wide.save(paths[0], "PNG", optimize=True)
    wide.save(paths[1], "PNG", optimize=True)
    square.save(paths[2], "PNG", optimize=True)
    square.save(paths[3], "PNG", optimize=True)
    for p in paths:
        print(f"[+] {p} ({p.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
