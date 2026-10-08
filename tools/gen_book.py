"""score4_AI の定石CSV(opening_book_with_human_made_book.csv)から、
index.html に埋め込む BOOK オブジェクトを作り、index.html の `var BOOK=...;` 行を置き換える。

使い方: python3 tools/gen_book.py <opening_book_with_human_made_book.csv> [index.html]
盤面キーは64文字(セル = 棒 + 16*段)。先手=1, 後手=2, 空き=0。
"""
import csv, json, re, sys

def main():
    src = sys.argv[1]
    html = sys.argv[2] if len(sys.argv) > 2 else "index.html"
    book = {}
    with open(src, newline="") as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            cells = [int(v) for v in row[0].split(",")]
            assert len(cells) == 64, row
            key = "".join({0: "0", 1: "1", -1: "2"}[v] for v in cells)
            book[key] = int(row[1])
    line = "var BOOK=" + json.dumps(book, separators=(",", ":")) + ";"
    with open(html, encoding="utf-8") as f:
        text = f.read()
    text, n = re.subn(r"^var BOOK=.*;$", lambda _: line, text, count=1, flags=re.M)
    assert n == 1, "index.html に var BOOK= の行が見つかりません"
    with open(html, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"{len(book)} 局面を書き込みました")

if __name__ == "__main__":
    main()
