#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

"""Select Mozilla Common Voice clips as continuous speech negatives.

Each input directory is one extracted Common Voice language release, e.g.
~/Music/CommonVoice/cv-corpus-27.0-2026-09-15/en. The clip list is read from
validated.tsv, or from the largest .tsv/.csv with a client_id and an audio
path column. The selection is deterministic: clips are ordered by a hash of
their file name, at most --max-per-speaker clips are taken per speaker, and
clips that are reported, have no transcript, or whose transcript matches
--exclude are skipped. The count is split evenly over the input directories,
and a release with too few usable clips leaves the rest of its share to the
others. Selected clips are cut to --max-seconds and converted to 16 kHz mono
16-bit WAV into --out-dir as cv_<lang>_<clip>.wav; old cv_*.wav files that
are no longer selected are removed.

Common Voice is CC0. Its terms forbid attempts to identify the speakers.
"""

import argparse
import concurrent.futures
import csv
import glob
import hashlib
import os
import re
import subprocess
import sys

PATH_COLS = ("path", "audio_file", "audio", "file", "filename")
TEXT_COLS = ("sentence", "transcription", "transcript", "text")


def find_table(root: str) -> str:
    v = sorted(glob.glob(os.path.join(root, "**", "validated.tsv"), recursive=True))
    if v:
        return v[0]
    cand = []
    for t in glob.glob(os.path.join(root, "**", "*.[tc]sv"), recursive=True):
        if "reported" in os.path.basename(t):
            continue
        with open(t, encoding="utf-8", errors="replace") as f:
            head = f.readline().lower()
        if "client_id" in head and any(c in head for c in PATH_COLS):
            cand.append((os.path.getsize(t), t))
    if not cand:
        raise FileNotFoundError(f"no Common Voice clip table found under {root}")
    return max(cand)[1]


def read_rows(table: str) -> list[dict]:
    csv.field_size_limit(sys.maxsize)
    with open(table, encoding="utf-8", errors="replace", newline="") as f:
        dialect = "excel-tab" if table.endswith(".tsv") else "excel"
        return list(csv.DictReader(f, dialect=dialect))


def reported(table: str, pcol: str) -> set[str]:
    out = set()
    for t in glob.glob(os.path.join(os.path.dirname(table), "*reported*.[tc]sv")):
        out |= {r.get(pcol, "") for r in read_rows(t)}
    return out - {""}


def find_audio(root: str, table: str, name: str) -> str | None:
    for d in (os.path.dirname(table), os.path.join(os.path.dirname(table), "clips"),
              os.path.join(os.path.dirname(table), "audios"),
              os.path.join(os.path.dirname(table), "audio_files"), root):
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    return None


def select(root: str, n: int, max_per_speaker: int, exclude: re.Pattern) -> list[tuple[str, str]]:
    table = find_table(root)
    rows = read_rows(table)
    if not rows:
        return []
    pcol = next(c for c in PATH_COLS if c in rows[0])
    tcol = next((c for c in TEXT_COLS if c in rows[0]), None)
    skip = reported(table, pcol)
    lang = os.path.basename(os.path.normpath(root)) or "cv"
    rows.sort(key=lambda r: hashlib.sha1(r[pcol].encode()).hexdigest())
    per_spk: dict[str, int] = {}
    out = []
    for r in rows:
        if len(out) >= n:
            break
        # Untranscribed clips cannot be screened for the keyword
        if r[pcol] in skip or (tcol and (not (r.get(tcol) or "").strip() or exclude.search(r[tcol]))):
            continue
        spk = r.get("client_id", "")
        if per_spk.get(spk, 0) >= max_per_speaker:
            continue
        src = find_audio(root, table, r[pcol])
        if not src:
            continue
        per_spk[spk] = per_spk.get(spk, 0) + 1
        base = os.path.splitext(os.path.basename(r[pcol]))[0]
        out.append((src, f"cv_{lang}_{base}.wav"))
    print(f"{root}: {table}, {len(rows)} clips, {len(skip)} reported, {len(out)} usable from {len(per_spk)} speakers")
    return out


def convert(src: str, dst: str, min_s: float, max_s: float) -> bool:
    if not os.path.isfile(dst):
        tmp = dst + ".tmp.wav"
        # -D: no random dither, for bit-exact repeatable clips
        r = subprocess.run(["sox", "-D", "-G", src, "-r", "16000", "-c", "1", "-b", "16", tmp,
                            "trim", "0", str(max_s)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if r.returncode != 0:
            return False
        os.replace(tmp, dst)
    if os.path.getsize(dst) < 44 + min_s * 32000:
        os.remove(dst)
        return False
    return True


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dirs", nargs="+", help="Extracted Common Voice language directories")
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--num", type=int, default=3000, help="Total clips (default 3000)")
    ap.add_argument("--max-per-speaker", type=int, default=5)
    ap.add_argument("--min-seconds", type=float, default=1.5)
    ap.add_argument("--max-seconds", type=float, default=10.0, help="Longer clips are cut (default 10)")
    ap.add_argument("--exclude", default=r"(?i)intel|in tel|英特尔|英特爾",
                    help="Skip clips whose transcript matches this regex")
    args = ap.parse_args()

    exclude = re.compile(args.exclude)
    os.makedirs(args.out_dir, exist_ok=True)
    avail = sorted((select(d, sys.maxsize, args.max_per_speaker, exclude) for d in args.dirs), key=len)
    # Even split; a release with too few clips leaves its share to the larger ones
    jobs = []
    for i, a in enumerate(avail):
        take = min(len(a), (args.num - len(jobs)) // (len(avail) - i))
        jobs += a[:take]

    keep = {name for _, name in jobs}
    for old in glob.glob(os.path.join(args.out_dir, "cv_*.wav")):
        if os.path.basename(old) not in keep:
            os.remove(old)

    with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as ex:
        ok = sum(ex.map(lambda j: convert(j[0], os.path.join(args.out_dir, j[1]), args.min_seconds, args.max_seconds), jobs))
    print(f"{ok} Common Voice clips in {args.out_dir} ({len(jobs) - ok} too short or failed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
