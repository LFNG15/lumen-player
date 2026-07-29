#!/usr/bin/env python3
"""Replace dark-only white hover washes with Theme::hoverBg (mode-aware)."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1] / "src"
pat = re.compile(r"rgba\(\s*255\s*,\s*255\s*,\s*255\s*,\s*([0-9.]+)\s*\)")

total = 0
for path in list(root.rglob("*.cpp")) + list(root.rglob("*.h")):
    text = path.read_text(encoding="utf-8")
    def repl(m: re.Match) -> str:
        return '" + Theme::hoverBg(' + m.group(1) + ') + "'
    new, n = pat.subn(repl, text)
    if n:
        path.write_text(new, encoding="utf-8", newline="\n")
        print(f"{path.relative_to(root.parent)}: {n}")
        total += n
print(f"total replacements: {total}")
