import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
SKIP_DIRS = {"obj", "Listings", ".vscode", "mcu-sdk", "__pycache__"}
EXTS = {".c", ".h"}


def decode_bytes(raw):
    if raw.startswith(b"\xef\xbb\xbf"):
        return raw[3:].decode("utf-8"), "utf-8-sig"

    try:
        return raw.decode("utf-8"), "utf-8"
    except UnicodeDecodeError:
        pass

    for enc in ("gb2312", "gbk"):
        try:
            return raw.decode(enc), enc
        except UnicodeDecodeError:
            pass

    return raw.decode("latin-1"), "latin-1"


def encode_utf8(text):
    return text.encode("utf-8")


def convert_file(path):
    raw = open(path, "rb").read()
    if not raw:
        return "empty"

    text, src_enc = decode_bytes(raw)
    out = encode_utf8(text.replace("\r\n", "\n").replace("\n", "\r\n"))

    if out == raw:
        return "unchanged"

    open(path, "wb").write(out)
    return src_enc


def main():
    converted = []
    unchanged = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            if os.path.splitext(fn)[1].lower() not in EXTS:
                continue
            path = os.path.join(dirpath, fn)
            if fn == "convert_to_utf8.py":
                continue
            result = convert_file(path)
            rel = os.path.relpath(path, ROOT)
            if result == "unchanged":
                unchanged.append(rel)
            elif result != "empty":
                converted.append((rel, result))

    print("Converted %d files to UTF-8 (with BOM):" % len(converted))
    for rel, enc in converted:
        print("  %s  (%s -> utf-8)" % (rel, enc))
    if unchanged:
        print("Already UTF-8: %d files" % len(unchanged))


if __name__ == "__main__":
    main()
