#!/usr/bin/env python3
"""
record-change.py — Append a formatted change entry to docs/xiangshan/porting-notes.md

Usage:
  python3 scripts/record-change.py \\
    --file "path/to/file.c" \\
    --title "Short description of change" \\
    --reason "Why this change was needed"

  # Multiple files:
  python3 scripts/record-change.py \\
    --file "path/a.c" --file "path/b.h" \\
    --title "Fix widget initialization" \\
    --reason "Widget crashed on startup due to uninitialized mutex"
"""
import argparse
import subprocess
import sys
from datetime import datetime

NOTES_FILE = "docs/xiangshan/porting-notes.md"

def get_diff(files):
    """Get git diff for the given files."""
    try:
        result = subprocess.run(
            ["git", "diff"] + files,
            capture_output=True, text=True, cwd="."
        )
        return result.stdout
    except Exception as e:
        return f"(diff unavailable: {e})"

def append_entry(file_paths, title, reason):
    """Append a new entry to porting-notes.md."""
    files_str = ", ".join(f"`{f}`" for f in file_paths)
    date_str = datetime.now().strftime("%Y-%m-%d %H:%M")
    diff = get_diff(file_paths)

    entry = f"""
### {title}

**Files:** {files_str}
**Date:** {date_str}

**Reason:** {reason}

```diff
{diff}
```
"""
    try:
        with open(NOTES_FILE, "a") as f:
            f.write(entry)
        print(f"✓ Entry appended to {NOTES_FILE}")
        return True
    except Exception as e:
        print(f"✗ Failed to append: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description="Record a change in porting-notes.md"
    )
    parser.add_argument(
        "--file", "-f", action="append", required=True,
        help="File path (can be specified multiple times)"
    )
    parser.add_argument(
        "--title", "-t", required=True,
        help="Short description of the change"
    )
    parser.add_argument(
        "--reason", "-r", required=True,
        help="Why this change was needed"
    )
    args = parser.parse_args()

    success = append_entry(args.file, args.title, args.reason)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
