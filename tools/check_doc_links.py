#!/usr/bin/env python3
"""Check relative Markdown destinations and GitHub heading anchors."""
import re
import subprocess
import unicodedata
from functools import lru_cache
from html import unescape
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
FENCE = re.compile(r"^ {0,3}(`{3,}|~{3,})(.*)")
INLINE_CODE = re.compile(r"(`+).*?\1")
HEADING = re.compile(r"^ {0,3}#{1,6}\s+(.+?)\s*#*\s*$")
MARKDOWN_LINK = re.compile(r"\]\(\s*(?:<([^>\n]+)>|((?:[^\s()]|\([^()\n]*\))+))")
REFERENCE = re.compile(r"^ {0,3}\[[^\]]+\]:\s*(?:<([^>]+)>|(\S+))")
HTML_DESTINATION = re.compile(r"""<(?:a|img)\b[^>]*?\b(?:href|src)\s*=\s*["']([^"']+)["']""", re.I)
HTML_ANCHOR = re.compile(r"""<[^>]+\s(?:id|name)\s*=\s*["']([^"']+)["']""", re.I)


def markdown_lines(path):
    fence = None
    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        marker = FENCE.match(line)
        if fence is not None:
            if marker and marker[1].startswith(fence) and not marker[2].strip():
                fence = None
        elif marker:
            fence = marker[1]
        else:
            yield lineno, line


def heading_slug(heading):
    heading = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", heading)
    heading = unescape(re.sub(r"<[^>]+>", "", heading)).lower().replace(" ", "-")
    return "".join(c for c in heading if c in "_-" or unicodedata.category(c)[0] in "LMN")


@lru_cache(maxsize=None)
def anchors(path):
    headings = set()
    explicit = set()
    previous = ""
    for _, line in markdown_lines(path):
        explicit.update(HTML_ANCHOR.findall(line))
        match = HEADING.match(line)
        heading = match[1] if match else previous if re.fullmatch(r" {0,3}(?:=+|-+)\s*", line) else ""
        if heading:
            base = slug = heading_slug(heading)
            index = 1
            while slug in headings:
                slug = f"{base}-{index}"
                index += 1
            headings.add(slug)
        previous = "" if heading else line.strip()
    return headings | explicit


def destinations(line):
    line = INLINE_CODE.sub("", line)
    for pattern in (MARKDOWN_LINK, REFERENCE, HTML_DESTINATION):
        for match in pattern.finditer(line):
            yield next(value for value in match.groups() if value is not None)


def target_error(source, destination):
    link = urlsplit(unescape(destination))
    if link.scheme or link.netloc:
        return None
    path = unquote(link.path)
    target = (ROOT / path.lstrip("/") if path.startswith("/") else source.parent / path) if path else source
    target = target.resolve()
    if not target.is_relative_to(ROOT) or not target.exists():
        return "missing path"
    if link.fragment and target.is_dir() and (target / "README.md").is_file():
        target /= "README.md"
    if link.fragment and target.suffix.lower() == ".md" and unquote(link.fragment) not in anchors(target):
        return "missing anchor"
    return None


def main():
    files = subprocess.check_output(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", "*.md"], cwd=ROOT
    ).decode().strip("\0").split("\0")
    failures = []
    checked = 0
    for relative in sorted(set(files)):
        if not relative:
            continue
        source = ROOT / relative
        for lineno, line in markdown_lines(source):
            for destination in destinations(line):
                parsed = urlsplit(destination)
                if parsed.scheme or parsed.netloc:
                    continue
                checked += 1
                if error := target_error(source, destination):
                    failures.append(f"{relative}:{lineno}: {error}: {destination}")
    if failures:
        print("\n".join(failures))
        return 1
    print(f"PASS: {checked} relative links and anchors in {len(set(files))} Markdown files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
