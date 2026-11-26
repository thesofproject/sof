#!/usr/bin/python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2026 Intel Corporation.

"""Copy keyword WAV files to a directory with anonymized running names.

The WAV files of the input directory are copied to the output directory as
<label>_001.wav, <label>_002.wav, ... where the label defaults to the name of
the output directory. The files are shuffled with a fixed seed, so the numbers
do not reveal the speaker or the recording order, and a rerun gives the same
names. Existing <label>_NNN.wav files in the output directory are removed
first. No mapping to the original names is kept unless --map is given.

Example:
    sof_mww_anonymize_clips.py ~/Music/local_mww_train_dataset/orange_orig_names \\
        ~/Music/local_mww_train_dataset/orange
"""

import argparse
import glob
import os
import random
import re
import shutil
import sys


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("in_dir", help="Directory with the original WAV files")
    ap.add_argument("out_dir", help="Output directory, created if needed")
    ap.add_argument("--label", help="Name prefix (default: output directory name)")
    ap.add_argument("--seed", type=int, default=1, help="Shuffle seed (default 1)")
    ap.add_argument("--no-shuffle", action="store_true", help="Keep the sorted order of the original names")
    ap.add_argument("--map", help="Write an original name to new name CSV here, keep it outside the dataset")
    args = ap.parse_args()

    label = args.label or os.path.basename(os.path.normpath(args.out_dir))
    files = sorted(glob.glob(os.path.join(args.in_dir, "*.wav")))
    if not files:
        print(f"No WAV files in {args.in_dir}", file=sys.stderr)
        return 1
    if not args.no_shuffle:
        random.Random(args.seed).shuffle(files)

    os.makedirs(args.out_dir, exist_ok=True)
    old = re.compile(rf"{re.escape(label)}_\d+\.wav$")
    for f in os.listdir(args.out_dir):
        if old.match(f):
            os.remove(os.path.join(args.out_dir, f))

    width = max(3, len(str(len(files))))
    rows = []
    for i, src in enumerate(files, 1):
        name = f"{label}_{i:0{width}d}.wav"
        shutil.copyfile(src, os.path.join(args.out_dir, name))
        rows.append(f"{os.path.basename(src)},{name}")

    if args.map:
        with open(args.map, "w") as m:
            m.write("original,anonymized\n" + "\n".join(rows) + "\n")
    print(f"{len(files)} files from {args.in_dir} to {args.out_dir} as {label}_NNN.wav")
    return 0


if __name__ == "__main__":
    sys.exit(main())
