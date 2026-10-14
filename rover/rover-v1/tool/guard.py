#!/usr/bin/env python3
"""Rover C3 guard: run before every commit (checks the working tree as it is).

    python tool/guard.py          # all checks
    python tool/guard.py --fast   # steps 1-5 only (no builds, no tests)

Exit 0 = READY TO COMMIT, 1 = BLOCKED. Why each check exists: spec/core/guards.md
"""
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIRMWARE = "firmware"
CORE = "firmware/lib/RoverCore"
CONFIG = "firmware/include/config.h"
PROTOCOL = "spec/core/protocol.md"
CODE_MAP = "spec/code-map.md"
BOARDS = ("supermini", "xiao")
SOURCE_DIRS = {  # folder -> file endings that count as source
    "firmware/src": (".cpp", ".h"), "firmware/include": (".h",), "firmware/lib": (".cpp", ".h"),
    "firmware/test": (".cpp", ".h"), "app/lib": (".dart",), "tool": (".py",),
}
SKIP_DIRS = {".git", ".pio", "build", ".dart_tool", "__pycache__", ".idea", ".vscode"}
STD_HEADERS = {"stddef.h", "stdint.h", "stdio.h", "string.h", "stdlib.h", "stdbool.h", "math.h",
               "limits.h", "algorithm", "cstddef", "cstdint", "cstdio", "cstring", "cstdlib", "cmath",
               "climits", "utility", "type_traits"}
AI_TRAILER = re.compile(r"(?im)^\s*co-authored-by:.*(claude|anthropic|copilot|openai|gpt|gemini|codeium|cursor)"
                        r"|noreply@anthropic\.com|generated (with|by) .*(claude|ai\b|copilot|gpt)")
HOME = os.path.expanduser("~")
MINGW_BIN = os.path.join(HOME, ".platformio", "packages", "toolchain-gccmingw32", "bin")


# ---------- Helpers ----------
def sh(args, cwd=ROOT, path_extra=None):
    env = dict(os.environ, PYTHONIOENCODING="utf-8")
    if path_extra:
        env["PATH"] = path_extra + os.pathsep + env.get("PATH", "")
    try:
        r = subprocess.run(args, cwd=cwd, capture_output=True, text=True, encoding="utf-8",
                           errors="replace", env=env)
    except OSError as e:
        return 1, str(e)
    return r.returncode, (r.stdout or "") + (r.stderr or "")


def fail(message, lines=()):
    print(f"  BLOCKED - {message}")
    for line in list(lines)[:25]:
        print(f"    {line}")
    return False


def skip(why):
    print(f"  SKIPPED - {why}")
    return None


def read(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8", errors="replace") as f:
        return f.read()


def walk(top, exts):
    for d, dirs, names in os.walk(os.path.join(ROOT, top)):
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        for name in names:
            if name.endswith(exts):
                yield os.path.relpath(os.path.join(d, name), ROOT).replace("\\", "/")


def find_pio():
    for candidate in (os.environ.get("PIO"), shutil.which("pio"),
                      os.path.join(HOME, ".platformio", "penv", "Scripts", "pio.exe"),
                      os.path.join(HOME, ".platformio", "penv", "bin", "pio")):
        if candidate and os.path.isfile(candidate):
            return candidate
    return None


# ---------- 1. Code map ----------
def check_code_map():
    text = read(CODE_MAP)
    named = set(re.findall(r"[\w.-]+\.(?:cpp|h|py|dart)\b", text))
    named |= {f"{stem}.{ext}" for stem in re.findall(r"([\w-]+)\.h/\.cpp", text) for ext in ("h", "cpp")}
    sources = [p for top, exts in SOURCE_DIRS.items() for p in walk(top, exts)]
    missing = [p for p in sources if os.path.basename(p) not in named]
    if missing:
        return fail(f"source files not named in {CODE_MAP}:", missing)
    print(f"  ok ({len(sources)} source files mapped)")
    return True


# ---------- 2. Doc links ----------
def unfenced(rel):
    return re.sub(r"(?ms)^```.*?^```", "", read(rel))


def anchors(rel):
    """GitHub heading slugs: lowercase, drop punctuation, each whitespace char becomes '-'."""
    return {re.sub(r"\s", "-", re.sub(r"[^\w\s-]", "", h.strip().lower()))
            for h in re.findall(r"(?m)^#+\s+(.+)$", unfenced(rel))}


def check_docs():
    docs = ["CLAUDE.md", *walk("spec", (".md",))]
    bad, count = [], 0
    for doc in docs:
        # links inside code are examples, not references
        for n, line in enumerate(re.sub(r"`[^`\n]*`", "", unfenced(doc)).splitlines(), 1):
            for target in re.findall(r"\]\(<?([^)\s>]+)>?\)", line):
                if re.match(r"[a-z]+:", target):
                    continue
                count += 1
                path, _, anchor = target.partition("#")
                rel = os.path.normpath(os.path.join(os.path.dirname(doc), path)).replace("\\", "/") if path else doc
                if not os.path.exists(os.path.join(ROOT, rel)):
                    bad.append(f"{doc}:{n}  MISSING FILE {target}")
                elif anchor and rel.endswith(".md") and anchor not in anchors(rel):
                    bad.append(f"{doc}:{n}  MISSING ANCHOR {target}")
    if bad:
        return fail("broken doc links:", bad)
    print(f"  ok ({count} links across {len(docs)} docs)")
    return True


# ---------- 3. Protocol constants ----------
def check_protocol():
    config, doc = read(CONFIG), read(PROTOCOL)
    bad = []
    fw = re.search(r"PROTOCOL_VERSION\s*=\s*(\d+)", config)
    spec = re.search(r"\*\*Version:\*\*\s*(\d+)", doc)
    if not fw or not spec or fw.group(1) != spec.group(1):
        bad.append(f"PROTOCOL_VERSION {fw and fw.group(1)} in {CONFIG} vs **Version:** {spec and spec.group(1)} in {PROTOCOL}")
    for uuid in re.findall(r'"([0-9A-Fa-f]{8}-[0-9A-Fa-f-]{27})"', config):
        if uuid.lower() not in doc.lower():
            bad.append(f"{uuid} is in {CONFIG} but not in {PROTOCOL}")
    if bad:
        return fail("firmware and protocol.md disagree:", bad)
    print(f"  ok (protocol v{fw.group(1)})")
    return True


# ---------- 4. RoverCore purity ----------
def check_core():
    own = {os.path.basename(p) for p in walk(CORE, (".h",))}
    bad = []
    for rel in walk(CORE, (".cpp", ".h")):
        for n, line in enumerate(read(rel).splitlines(), 1):
            m = re.match(r'\s*#\s*include\s*([<"])([^>"]+)[>"]', line)
            if m and (m.group(2) not in own if m.group(1) == '"' else m.group(2) not in STD_HEADERS):
                bad.append(f"{rel}:{n}  {line.strip()}")
    if bad:
        return fail("RoverCore must stay pure C++ (standard headers and its own only):", bad)
    print("  ok")
    return True


# ---------- 5. AI trailers ----------
def check_trailers():
    if not os.path.isdir(os.path.join(ROOT, ".git")):
        print("  ok (not a git repository yet)")
        return True
    code, out = sh(["git", "log", "--format=%H%n%B%n--end--"])
    if code:
        print("  ok (no commits yet)")
        return True
    bad = [block.splitlines()[0][:10] for block in out.split("--end--") if block.strip() and AI_TRAILER.search(block)]
    if bad:
        return fail("commit messages with AI attribution:", bad)
    print("  ok")
    return True


# ---------- 6. Firmware build ----------
def check_firmware():
    pio = find_pio()
    if not pio:
        return skip("PlatformIO not found (spec/core/build.md)")
    args = [pio, "run", "-d", FIRMWARE]
    for board in BOARDS:
        args += ["-e", board]
    code, out = sh(args)
    if code:
        return fail("pio run failed:", [line for line in out.splitlines() if "error" in line.lower()] or out.splitlines()[-15:])
    sizes = re.findall(r"Flash:.*?used (\d+) bytes", out)
    print(f"  ok ({', '.join(f'{b} {int(s):,} bytes' for b, s in zip(BOARDS, sizes))})")
    return True


# ---------- 7. Unit tests ----------
def check_tests():
    pio = find_pio()
    if not pio:
        return skip("PlatformIO not found (spec/core/build.md)")
    extra = None
    if not shutil.which("g++"):
        if not os.path.isfile(os.path.join(MINGW_BIN, "g++.exe")):
            return skip("no g++ on PATH (spec/core/build.md#unit-tests-on-the-pc)")
        extra = MINGW_BIN
    code, out = sh([pio, "test", "-d", FIRMWARE, "-e", "native"], path_extra=extra)
    summary = re.search(r"(\d+) test cases: (.*?) in", out)
    if code:
        return fail("pio test failed:", [line for line in out.splitlines() if "FAIL" in line or "rror" in line] or out.splitlines()[-15:])
    print(f"  ok ({summary.group(1) + ' tests, ' + summary.group(2) if summary else 'passed'})")
    return True


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    steps = [("Code map", check_code_map), ("Doc links", check_docs), ("Protocol constants", check_protocol),
             ("RoverCore purity", check_core), ("AI trailers", check_trailers),
             ("Firmware build", check_firmware), ("Unit tests", check_tests)]
    if "--fast" in sys.argv:
        steps = steps[:5]
    results = []
    for n, (title, fn) in enumerate(steps, 1):
        print(f"\n[{n}/{len(steps)}] {title}")
        results.append(fn())
    skipped = results.count(None)
    if False in results:
        print("\nBLOCKED - fix the above, then run this again.")
        return 1
    print("\nREADY TO COMMIT" + (f" ({skipped} step(s) SKIPPED: see above)" if skipped else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
