# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
Delete keyword WAVs whose ASR transcription does not match the keyword.

Reads a "file,text" CSV from sof_mww_asr_transcribe.py. A transcription
passes when it equals one of the --pass strings, compared case-insensitively
with punctuation removed and whitespace collapsed. Other files are deleted
from the WAV directory the CSV was made from.

Usage:
    python3 sof_mww_asr_filter.py --pass banana \\
        banana_asr.csv ~/train/mww_wov2/wavs/banana
"""

import argparse
import csv
import re
import sys
from collections import Counter
from pathlib import Path


def normalize(text: str) -> str:
    """Lowercase, replace non-alphanumerics with spaces, collapse whitespace."""
    return " ".join(re.sub(r"[^\w\s]|_", " ", text.lower()).split())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pass", dest="passes", action="append", required=True,
                        help="Accepted transcription, case-insensitive, may be repeated")
    parser.add_argument("--dry-run", "-n", action="store_true",
                        help="Only report what would be deleted")
    parser.add_argument("csv_in", type=Path, help="CSV from sof_mww_asr_transcribe.py")
    parser.add_argument("wav_dir", type=Path, help="Directory the CSV file paths are relative to")
    args = parser.parse_args()

    passes = {normalize(p) for p in args.passes}
    wav_dir = args.wav_dir.resolve()
    kept = 0
    rejected: Counter[str] = Counter()
    deleted = missing = 0

    with open(args.csv_in, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if normalize(row["text"]) in passes:
                kept += 1
                continue

            rejected[row["text"]] += 1
            path = (wav_dir / row["file"]).resolve()
            if not path.is_relative_to(wav_dir):
                print(f"Skipping path outside {wav_dir}: {row['file']}", file=sys.stderr)
                continue
            if not path.is_file():
                missing += 1
                continue
            if not args.dry_run:
                path.unlink()
            deleted += 1

    total = kept + sum(rejected.values())
    action = "Would delete" if args.dry_run else "Deleted"
    print(f"Kept {kept} of {total} files, {action.lower()} {deleted}, {missing} already missing")
    print("Rejected transcriptions (count):")
    for text, n in rejected.most_common(20):
        print(f"  {n:6d}  {text}")
    if len(rejected) > 20:
        print(f"  ... and {len(rejected) - 20} more")

    return 0


if __name__ == "__main__":
    sys.exit(main())
