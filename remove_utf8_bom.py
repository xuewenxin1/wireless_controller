import os

ROOT = os.path.dirname(os.path.abspath(__file__))
SKIP_DIRS = {"obj", "Listings", ".vscode", "__pycache__"}
EXTS = {".c", ".h"}
BOM = b"\xef\xbb\xbf"


def strip_bom(path):
    raw = open(path, "rb").read()
    if not raw.startswith(BOM):
        return False
    open(path, "wb").write(raw[len(BOM):])
    return True


def main():
    count = 0
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            if os.path.splitext(fn)[1].lower() not in EXTS:
                continue
            if fn in ("convert_to_utf8.py", "remove_utf8_bom.py", "restore_gb2312.py"):
                continue
            path = os.path.join(dirpath, fn)
            if strip_bom(path):
                count += 1
                print("  %s" % os.path.relpath(path, ROOT))
    print("Removed BOM from %d files." % count)


if __name__ == "__main__":
    main()
