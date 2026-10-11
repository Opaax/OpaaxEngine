"""Turns compiler errors from a build log into GitHub annotations.

Usage: python report_errors.py <build.log>

Annotations are limited per step (10 errors, 10 warnings, 10 notices), so the unique errors are
spread over the three levels: up to 30 distinct errors per run. Every message keeps "error" in its
text whatever its level.
"""
import re
import sys
from pathlib import Path

GCC = re.compile(r"^(?P<file>[^\s:][^:]*):(?P<line>\d+):(?:(?P<col>\d+):)?\s+(?:fatal\s+)?error:\s+(?P<msg>.*)$")
MSVC = re.compile(r"^\s*(?:\d+>)?(?P<file>[^\s].*?)\((?P<line>\d+)(?:,(?P<col>\d+))?\)\s*:\s+(?:fatal\s+)?error\s+(?P<code>\w+)\s*:\s*(?P<msg>.*?)(?:\s+\[.*\])?$")
CMAKE = re.compile(r"^CMake Error at (?P<file>[^:]+):(?P<line>\d+)")
LINKER = re.compile(r"(undefined reference to|unresolved external symbol|ld: symbol\(s\) not found|error LNK\d+)")

# When nothing above matched (a failed custom command, a killed compiler), the last lines that look
# like a failure are reported instead.
FALLBACK = re.compile(r"(error|\*\*\*|failed|killed|segmentation fault|abort)", re.IGNORECASE)


def main():
    log = Path(sys.argv[1])
    if not log.exists():
        print("no build log")
        return

    lines = log.read_text(encoding="utf-8", errors="replace").splitlines()
    seen = set()
    errors = []
    for raw in lines:
        line = raw.rstrip()
        for pattern in (GCC, MSVC, CMAKE):
            match = pattern.match(line)
            if match:
                file = match.group("file").strip()
                key = (file, match.group("line"), match.groupdict().get("msg", ""))
                if key not in seen:
                    seen.add(key)
                    errors.append((file, match.group("line"), match.groupdict().get("msg") or line))
                break
        else:
            if LINKER.search(line) and line not in seen:
                seen.add(line)
                errors.append(("", "", line))

    print(f"{len(errors)} distinct error(s) in the build log")

    if not errors:
        failures = [line.strip() for line in lines if FALLBACK.search(line)]
        errors = [("", "", line) for line in failures[-10:]]
        print(f"no compiler or linker error recognised; reporting the last {len(errors)} failure line(s)")

    root = str(Path.cwd()).replace("\\", "/") + "/"
    for index, (file, line, msg) in enumerate(errors[:30]):
        level = "error" if index < 10 else ("warning" if index < 20 else "notice")
        msg = msg.replace("%", "%25").replace("\r", "").replace("\n", " ")
        file = file.replace("\\", "/").replace(root, "")
        location = f" file={file},line={line}" if file and line else ""
        print(f"::{level}{location}::error: {msg[:500]}")


if __name__ == "__main__":
    main()
