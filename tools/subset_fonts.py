#!/usr/bin/env python3
"""index.html で使う文字だけをフォントから切り出し、fonts/ に woff2 として書き出す。

元のフォント(OFL)は google/fonts から取得する:
  https://github.com/google/fonts/tree/main/ofl/shipporiminchob1
  https://github.com/google/fonts/tree/main/ofl/zenkakugothicnew
  https://github.com/google/fonts/tree/main/ofl/ibmplexmono

使い方:
  python3 tools/subset_fonts.py <元の .ttf を置いたフォルダ>
  python3 tools/subset_fonts.py --check    # 同梱フォントに index.html の字がすべて入っているか確かめる

index.html の文言を変えて新しい字が増えたら、もう一度実行する(字が無いと端末標準の書体で表示される)。
"""
import os
import re
import sys

from fontTools import subset

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "fonts")

# (元のファイル, 書き出す名前, 文字の範囲)
FONTS = [
    ("ShipporiMinchoB1-Regular.ttf", "ShipporiMinchoB1-400.woff2", "text"),
    ("ShipporiMinchoB1-SemiBold.ttf", "ShipporiMinchoB1-600.woff2", "text"),
    ("ShipporiMinchoB1-ExtraBold.ttf", "ShipporiMinchoB1-800.woff2", "text"),
    ("ZenKakuGothicNew-Regular.ttf", "ZenKakuGothicNew-400.woff2", "text"),
    ("ZenKakuGothicNew-Bold.ttf", "ZenKakuGothicNew-700.woff2", "text"),
    ("IBMPlexMono-Regular.ttf", "IBMPlexMono-400-digits.woff2", "digits"),
]
LICENSES = [("OFL-shipporiminchob1.txt", "OFL-ShipporiMinchoB1.txt"),
            ("OFL-zenkakugothicnew.txt", "OFL-ZenKakuGothicNew.txt"),
            ("OFL-ibmplexmono.txt", "OFL-IBMPlexMono.txt")]


def used_text(pad=True):
    html = open(os.path.join(ROOT, "index.html"), encoding="utf-8").read()
    # 音の素材(base64)と定石のデータは文字ではないので除く
    html = re.sub(r"var KOTO_WAV=.*?;\n", "", html, flags=re.S)
    html = re.sub(r"var BOOK=.*?;\n", "", html, flags=re.S)
    chars = set(c for c in html if ord(c) >= 0x3000)
    if not pad:
        return "".join(sorted(chars))
    # 文言が少し変わっても字が欠けないよう、かなと記号は範囲ごと入れておく
    chars |= set(chr(c) for c in range(0x20, 0x7F))        # ASCII
    chars |= set(chr(c) for c in range(0x3040, 0x30A0))    # ひらがな
    chars |= set(chr(c) for c in range(0x30A0, 0x3100))    # カタカナ
    chars |= set("×・…—–‐、。「」『』（）〜！？：；＋－％　")
    return "".join(sorted(chars))


def check():
    from fontTools.ttLib import TTFont
    text = used_text(pad=False)
    ok = True
    for _, out, kind in FONTS:
        cmap = TTFont(os.path.join(OUT, out)).getBestCmap()
        need = "0123456789" if kind == "digits" else text
        missing = [c for c in need if c != " " and ord(c) not in cmap]
        if missing:
            ok = False
            print(f"{out}: 足りない字 {''.join(missing)}")
    print("ok fonts: 同梱フォントに index.html の字がすべて入っている" if ok else "NG: tools/subset_fonts.py で作り直す")
    return ok


def main():
    if sys.argv[1:] == ["--check"]:
        sys.exit(0 if check() else 1)
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    text = used_text()
    print(f"文字数: {len(text)}")
    for name, out, kind in FONTS:
        opts = subset.Options()
        opts.flavor = "woff2"
        opts.layout_features = ["*"]
        opts.name_IDs = ["*"]       # ライセンス表記などの名前情報を残す
        opts.notdef_outline = True
        f = subset.load_font(os.path.join(src, name), opts)
        s = subset.Subsetter(opts)
        s.populate(text="0123456789" if kind == "digits" else text)
        s.subset(f)
        path = os.path.join(OUT, out)
        subset.save_font(f, path, opts)
        print(f"{out}: {os.path.getsize(path) // 1024} KB")
    for a, b in LICENSES:
        with open(os.path.join(src, a), encoding="utf-8") as r, open(os.path.join(OUT, b), "w", encoding="utf-8") as w:
            w.write(r.read())


if __name__ == "__main__":
    main()
