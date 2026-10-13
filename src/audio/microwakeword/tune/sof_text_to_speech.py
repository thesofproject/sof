#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
Convert text words passed via command-line arguments to a 48 kHz WAV file
using Piper TTS (US English).

Requirements:
  - Python environment with piper-tts (and optionally soxr / scipy for high-quality resampling)
    (e.g., source ~/venvs/piper/bin/activate)
  - Piper voice model (default: ~/git/piper-sample-generator/models/en_US-libritts_r-medium.pt)

Usage:
  python3 sof_text_to_speech.py "hello world" -o output.wav
  python3 sof_text_to_speech.py hi intel -o hi_intel_48k.wav
  python3 sof_text_to_speech.py "wake up" --rate 48000 --speaker 0
"""

from __future__ import annotations

import argparse
import io
import os
import sys
import wave
from pathlib import Path

import numpy as np

# Try importing resampling engines (soxr -> scipy.signal -> linear fallback)
try:
    import soxr

    def resample_audio(audio: np.ndarray, in_sr: int, out_sr: int) -> np.ndarray:
        return soxr.resample(audio, in_sr, out_sr).astype(np.int16)

except ImportError:
    try:
        from math import gcd
        from scipy.signal import resample_poly

        def resample_audio(audio: np.ndarray, in_sr: int, out_sr: int) -> np.ndarray:
            g = gcd(in_sr, out_sr)
            up = out_sr // g
            down = in_sr // g
            resampled = resample_poly(audio.astype(np.float32), up, down)
            return np.clip(np.round(resampled), -32768, 32767).astype(np.int16)

    except ImportError:

        def resample_audio(audio: np.ndarray, in_sr: int, out_sr: int) -> np.ndarray:
            if in_sr == out_sr:
                return audio
            num_out_samples = int(round(len(audio) * float(out_sr) / in_sr))
            indices = np.linspace(0, len(audio) - 1, num_out_samples)
            resampled = np.interp(indices, np.arange(len(audio)), audio.astype(np.float32))
            return np.clip(np.round(resampled), -32768, 32767).astype(np.int16)


def find_default_voice() -> str | None:
    env_voice = os.environ.get("PIPER_VOICE")
    if env_voice and os.path.isfile(env_voice):
        return env_voice

    candidates = [
        os.path.expanduser("~/git/piper-sample-generator/models/en_US-libritts_r-medium.pt"),
        os.path.expanduser("~/git/piper-sample-generator/models/en_US-lessac-medium.onnx"),
        os.path.expanduser("~/git/piper-sample-generator/models/en_US-libritts_r-medium.onnx"),
        os.path.expanduser("~/.local/share/piper-voices/en/en_US/lessac/medium/en_US-lessac-medium.onnx"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None


def synthesize_to_wav(
    text: str,
    output_path: str | Path,
    voice_path: str,
    target_sample_rate: int = 48000,
    speaker_id: int | None = None,
    length_scale: float = 1.0,
    noise_scale: float = 0.667,
    noise_w: float = 0.8,
) -> None:
    try:
        from piper import PiperVoice
    except ImportError:
        try:
            from piper.voice import PiperVoice
        except ImportError as exc:
            raise RuntimeError(
                "piper-tts is not installed or importable. "
                "Please activate ~/venvs/piper (e.g. source ~/venvs/piper/bin/activate)"
            ) from exc

    if not os.path.isfile(voice_path):
        raise FileNotFoundError(f"Piper voice model not found: {voice_path}")

    if voice_path.endswith(".pt"):
        import subprocess
        import tempfile

        piper_repo = os.environ.get("PIPER_REPO", os.path.expanduser("~/git/piper-sample-generator"))
        env = os.environ.copy()
        if piper_repo and os.path.isdir(piper_repo):
            env["PYTHONPATH"] = piper_repo + (f":{env['PYTHONPATH']}" if "PYTHONPATH" in env else "")

        with tempfile.TemporaryDirectory() as tmpdir:
            cmd = [
                sys.executable,
                "-m",
                "piper_sample_generator",
                text,
                "--model",
                str(voice_path),
                "--max-samples",
                "1",
                "--length-scales",
                str(length_scale),
                "--output-dir",
                tmpdir,
            ]
            if speaker_id is not None:
                cmd.extend(["--max-speakers", str(speaker_id + 1)])
            res = subprocess.run(cmd, env=env, capture_output=True, text=True)
            if res.returncode != 0:
                raise RuntimeError(
                    f"piper_sample_generator failed (exit code {res.returncode}):\n{res.stderr}"
                )
            wav_files = sorted(Path(tmpdir).glob("*.wav"))
            if not wav_files:
                raise RuntimeError("piper_sample_generator did not produce any WAV files.")

            with wave.open(str(wav_files[0]), "rb") as wf:
                in_sr = wf.getframerate()
                raw_pcm = wf.readframes(wf.getnframes())

        audio_pcm16 = np.frombuffer(raw_pcm, dtype=np.int16)
    else:
        voice = PiperVoice.load(voice_path)
        in_sr = voice.config.sample_rate

        # Prepare synthesis configuration if supported
        synth_kwargs: dict = {}
        if speaker_id is not None:
            synth_kwargs["speaker_id"] = speaker_id

        try:
            from piper import SynthesisConfig

            nw_field = "noise_w_scale" if hasattr(SynthesisConfig, "noise_w_scale") else "noise_w"
            cfg = SynthesisConfig(
                speaker_id=speaker_id,
                length_scale=length_scale,
                noise_scale=noise_scale,
                **{nw_field: noise_w},
            )
            synth_kwargs["syn_config"] = cfg
        except (ImportError, TypeError):
            synth_kwargs["length_scale"] = length_scale
            synth_kwargs["noise_scale"] = noise_scale
            synth_kwargs["noise_w"] = noise_w

        buf = io.BytesIO()
        with wave.open(buf, "wb") as wf:
            if hasattr(voice, "synthesize_wav"):
                voice.synthesize_wav(text, wf, **synth_kwargs)
            else:
                voice.synthesize(text, wf, **synth_kwargs)

        buf.seek(0)
        with wave.open(buf, "rb") as wf:
            raw_pcm = wf.readframes(wf.getnframes())

        audio_pcm16 = np.frombuffer(raw_pcm, dtype=np.int16)

    # Resample to target rate if needed
    if in_sr != target_sample_rate:
        audio_out = resample_audio(audio_pcm16, in_sr, target_sample_rate)
    else:
        audio_out = audio_pcm16

    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with wave.open(str(output_path), "wb") as out_wf:
        out_wf.setnchannels(1)
        out_wf.setsampwidth(2)
        out_wf.setframerate(target_sample_rate)
        out_wf.writeframes(audio_out.tobytes())


def parse_args() -> argparse.Namespace:
    epilog = """
Setup Instructions:
  1. Setup Piper TTS virtual environment:
     python3 -m venv ~/venvs/piper
     source ~/venvs/piper/bin/activate
     pip install --upgrade pip
     pip install piper-sample-generator piper-tts soxr

  2. Clone piper-sample-generator and download en_US-libritts_r-medium.pt:
     git clone https://github.com/rhasspy/piper-sample-generator ~/git/piper-sample-generator
     mkdir -p ~/git/piper-sample-generator/models
     wget -O ~/git/piper-sample-generator/models/en_US-libritts_r-medium.pt \\
       https://github.com/rhasspy/piper-sample-generator/releases/download/v2.0.0/en_US-libritts_r-medium.pt
"""
    parser = argparse.ArgumentParser(
        description="Synthesize text to 48 kHz mono 16-bit WAV using Piper TTS (US English).",
        epilog=epilog,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "words",
        nargs="+",
        help="Words or phrase to synthesize.",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="output_48k.wav",
        help="Output WAV file path (default: output_48k.wav)",
    )
    parser.add_argument(
        "--model",
        "-m",
        default=find_default_voice(),
        help="Path to Piper voice model (.pt or .onnx, default: en_US-libritts_r-medium.pt)",
    )
    parser.add_argument(
        "--rate",
        "-r",
        type=int,
        default=48000,
        help="Target sample rate in Hz (default: 48000)",
    )
    parser.add_argument(
        "--speaker",
        "-s",
        type=int,
        default=None,
        help="Speaker ID for multi-speaker models (optional)",
    )
    parser.add_argument(
        "--length-scale",
        type=float,
        default=1.0,
        help="Speaking speed multiplier (lower = faster, default 1.0)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    text = " ".join(args.words).strip()
    if not text:
        print("Error: No text provided.", file=sys.stderr)
        return 1

    if not args.model:
        print(
            "Error: No Piper voice model found. Specify with --model or set PIPER_VOICE.",
            file=sys.stderr,
        )
        return 1

    print(f"Synthesizing: \"{text}\"")
    print(f"Model       : {args.model}")
    print(f"Output      : {args.output} ({args.rate} Hz mono)")

    synthesize_to_wav(
        text=text,
        output_path=args.output,
        voice_path=args.model,
        target_sample_rate=args.rate,
        speaker_id=args.speaker,
        length_scale=args.length_scale,
    )

    print(f"Done: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
