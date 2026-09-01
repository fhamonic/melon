#!/usr/bin/env python3
"""Fails if a file names a std:: symbol no header it includes is guaranteed to provide.

melon is header-only, so a header that reaches a std:: name through an include
it does not make itself compiles here and breaks in a consumer's translation
unit whose include order, or whose standard library, differs. Every job in the
build matrix but the MSVC one compiles against libstdc++, whose <ranges> drags
in <bits/move.h>, <bits/stl_pair.h> and <type_traits>; nothing in CI fails on a
missing <utility> or <type_traits>, which is why this check is source-level.

An include counts as provided when the file includes it directly, or when the
standard mandates it: [ranges.syn] opens with `#include <iterator>`, which opens
with `#include <concepts>`, so <ranges> alone is enough for std::same_as. That
guarantee is normative -- relying on it is not an accidental transitive include.
Reaching a std:: header through another *melon* header is not covered: it holds
only until someone reorders the includes in the header being leaned on.
"""
import argparse
import pathlib
import re
import sys

SYMBOL_USE_RE = re.compile(r"(?<![\w:])std::(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*")
INCLUDE_RE = re.compile(r"^\s*#\s*include\s*<([^>]+)>", re.M)


def load_table(path):
    mandated, symbols, section = {}, {}, None
    for line in pathlib.Path(path).read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("["):
            section = line
            continue
        key, *headers = line.split()
        (mandated if section == "[mandated]" else symbols)[key] = headers
    return mandated, symbols


def blank_comments_and_literals(source):
    """Overwrites comment and literal bodies with spaces, keeping every offset.

    Line numbers and match positions stay valid, so findings can be reported
    against the original file without a second pass over it.
    """
    out, i, n = [], 0, len(source)
    while i < n:
        c = source[i]
        if c == "/" and source.startswith("//", i):
            j = source.find("\n", i)
            j = n if j < 0 else j
        elif c == "/" and source.startswith("/*", i):
            j = source.find("*/", i + 2)
            j = n if j < 0 else j + 2
        # A ' after an identifier character is a digit separator (500'000),
        # not a literal: treating it as one swallows code up to the next quote.
        elif c == '"' or (c == "'" and not (i and (source[i - 1].isalnum()
                                                   or source[i - 1] == "_"))):
            j = i + 1
            while j < n and source[j] != c:
                j += 2 if source[j] == "\\" else 1
            j = min(j + 1, n)
        else:
            out.append(c)
            i += 1
            continue
        out.append("".join(ch if ch == "\n" else " " for ch in source[i:j]))
        i = j
    return "".join(out)


def provided_headers(source, mandated):
    """The file's own includes, closed over the synopsis-mandated ones."""
    found = set(INCLUDE_RE.findall(source))
    pending = list(found)
    while pending:
        for header in mandated.get(pending.pop(), ()):
            if header not in found:
                found.add(header)
                pending.append(header)
    return found


def resolve(use, symbols):
    """Longest known prefix: std::chrono::steady_clock::now names <chrono>."""
    parts = use.split("::")
    while len(parts) > 1:
        name = "::".join(parts)
        if name in symbols:
            return name, symbols[name]
        parts.pop()
    return None, None


def check(path, mandated, symbols, strict):
    source = path.read_text()
    provided = provided_headers(source, mandated)
    code = blank_comments_and_literals(source)
    findings, seen = [], set()
    for mo in SYMBOL_USE_RE.finditer(code):
        name, headers = resolve(mo.group(0), symbols)
        if headers is None or name in seen:
            continue
        seen.add(name)
        # The table lists multi-header symbols in preference order, so the
        # first entry is the one the standard documents the symbol under.
        accepted = headers[:1] if strict else headers
        if not provided.intersection(accepted):
            findings.append(
                (code.count("\n", 0, mo.start()) + 1, name, accepted))
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", help="files or directories to check")
    parser.add_argument("--table", default=str(
        pathlib.Path(__file__).with_name("std_include_table.txt")))
    parser.add_argument("--strict", action="store_true",
                        help="demand the preferred header, not any provider")
    args = parser.parse_args()

    mandated, symbols = load_table(args.table)
    files = []
    for path in map(pathlib.Path, args.paths):
        if path.is_dir():
            files += sorted(path.rglob("*.hpp")) + sorted(path.rglob("*.cpp"))
        else:
            files.append(path)

    total = 0
    for path in files:
        for line, name, headers in check(path, mandated, symbols, args.strict):
            print("{}:{}: uses {} but includes none of {}".format(
                path, line, name, " ".join("<" + h + ">" for h in headers)))
            total += 1
    if total:
        print("\n{} symbol(s) reached through an include the file does not "
              "make and the standard does not mandate.".format(total),
              file=sys.stderr)
        return 1
    print("{} files checked, every std:: symbol covered.".format(len(files)))
    return 0


sys.exit(main())
