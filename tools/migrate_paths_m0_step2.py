"""
M0 Step 2 — path migration.

Rewrites the deploy-form path strings stored in serialized JSON (manifests,
scenes) to the new project-root-relative form:

    "EngineAssets/  ->  "Engine/Assets/
    "GameAssets/    ->  "Game/Assets/

Idempotent: running twice is a no-op.

Usage:
    python tools/migrate_paths_m0_step2.py            # apply
    python tools/migrate_paths_m0_step2.py --dry-run  # preview only

Run from the repository root.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPLACEMENTS = (
    ('"EngineAssets/', '"Engine/Assets/'),
    ('"GameAssets/',   '"Game/Assets/'),
)

SCAN_ROOTS = (
    Path("Engine/Assets"),
    Path("Game/Assets"),
)


def migrate_file(path: Path, dry_run: bool) -> tuple[int, str | None]:
    """Returns (replacement_count, new_text_if_changed_or_None)."""
    original = path.read_text(encoding="utf-8")
    updated = original
    total = 0
    for old, new in REPLACEMENTS:
        count = updated.count(old)
        if count:
            updated = updated.replace(old, new)
            total += count

    if total == 0:
        return 0, None

    if not dry_run:
        path.write_text(updated, encoding="utf-8")

    return total, updated


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true",
                        help="Print what would change without writing.")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent

    files_scanned = 0
    files_changed = 0
    total_replacements = 0

    for root in SCAN_ROOTS:
        abs_root = repo_root / root
        if not abs_root.exists():
            print(f"[skip] {root} — not found")
            continue

        for json_path in abs_root.rglob("*.json"):
            files_scanned += 1
            count, _ = migrate_file(json_path, args.dry_run)
            if count:
                files_changed += 1
                total_replacements += count
                rel = json_path.relative_to(repo_root)
                action = "would update" if args.dry_run else "updated"
                print(f"  {action}: {rel}  (+{count} replacements)")

    print()
    print(f"Scanned: {files_scanned} JSON file(s)")
    print(f"{'Would change' if args.dry_run else 'Changed'}: "
          f"{files_changed} file(s), {total_replacements} replacement(s)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
