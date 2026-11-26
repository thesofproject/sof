# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
Transcribe a directory of WAV files with OpenVINO Whisper into a CSV file.

The Whisper model is compiled once and reused for every file, so large
keyword datasets (e.g. Piper TTS generated positives) can be screened in
one run. The CSV has columns "file" (path relative to the input directory)
and "text" (transcription), and can be used to drop clips where the
keyword is not recognizable.

Usage:
    python3 sof_mww_asr_transcribe.py --model whisper-medium-int4-ov \\
        ~/train/mww_wov2/wavs/banana banana_asr.csv
"""

import argparse
import csv
import sys
from pathlib import Path

import numpy as np
import openvino as ov
import soundfile as sf
from transformers import WhisperFeatureExtractor, WhisperTokenizer

WHISPER_FEATURE_SIZE = 80
WHISPER_NB_MAX_FRAMES = 3000
WHISPER_RATE = 16000
# <|startoftranscript|> <|en|> <|transcribe|> <|notimestamps|>
SOT_TOKENS = [50258, 50259, 50359, 50363]
EOS_TOKEN = 50257


class WhisperSplitPipeline:
    """Whisper pipeline with separate OpenVINO devices for encoder and decoder."""

    def __init__(self, model_path: str, encoder_device: str, decoder_device: str):
        core = ov.Core()
        print(f"Compiling encoder on {encoder_device}...", file=sys.stderr)
        encoder_model = core.read_model(str(Path(model_path) / "openvino_encoder_model.xml"))
        # NPU requires static shapes
        encoder_model.reshape({0: [1, WHISPER_FEATURE_SIZE, WHISPER_NB_MAX_FRAMES]})
        self.encoder = core.compile_model(encoder_model, encoder_device)

        print(f"Compiling decoder on {decoder_device}...", file=sys.stderr)
        self.decoder = core.compile_model(
            str(Path(model_path) / "openvino_decoder_model.xml"), decoder_device
        )
        self.has_beam_idx = any(
            inp.get_any_name() == "beam_idx" for inp in self.decoder.inputs
        )
        self.feature_extractor = WhisperFeatureExtractor.from_pretrained(model_path)
        self.tokenizer = WhisperTokenizer.from_pretrained(model_path)
        self.encoder_req = self.encoder.create_infer_request()

    def transcribe(self, audio: np.ndarray, max_tokens: int = 448) -> str:
        """Transcribe 16 kHz mono float32 audio to text with greedy decoding."""
        features = self.feature_extractor(audio, sampling_rate=WHISPER_RATE, return_tensors="np")
        self.encoder_req.set_tensor(
            "input_features", ov.Tensor(features.input_features.astype(np.float32))
        )
        self.encoder_req.infer()
        hidden_state = self.encoder_req.get_tensor("last_hidden_state").data.copy()

        # New request per clip so the stateful decoder KV cache starts empty
        decoder_req = self.decoder.create_infer_request()
        decoder_req.set_tensor("encoder_hidden_states", ov.Tensor(hidden_state))
        input_ids = np.array([SOT_TOKENS], dtype=np.int64)
        generated = []
        for _ in range(max_tokens):
            decoder_req.set_tensor("input_ids", ov.Tensor(input_ids))
            if self.has_beam_idx:
                decoder_req.set_tensor("beam_idx", ov.Tensor(np.array([0], dtype=np.int32)))
            decoder_req.infer()
            next_token = int(np.argmax(decoder_req.get_tensor("logits").data[0, -1, :]))
            if next_token == EOS_TOKEN:
                break
            generated.append(next_token)
            input_ids = np.array([[next_token]], dtype=np.int64)

        return self.tokenizer.decode([t for t in generated if t < EOS_TOKEN]).strip()


def load_audio(path: Path) -> np.ndarray:
    """Read an audio file as 16 kHz mono float32."""
    audio, sr = sf.read(str(path), dtype="float32")
    if audio.ndim > 1:
        audio = np.mean(audio, axis=1)
    if sr != WHISPER_RATE:
        import librosa
        audio = librosa.resample(audio, orig_sr=sr, target_sr=WHISPER_RATE)
    return audio


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--model", "-m", default="whisper-medium-int4-ov",
                        help="Whisper OpenVINO model directory")
    parser.add_argument("--encoder-device", default="CPU", help="OpenVINO encoder device")
    parser.add_argument("--decoder-device", default="CPU", help="OpenVINO decoder device")
    parser.add_argument("--recursive", "-r", action="store_true",
                        help="Also transcribe WAV files in subdirectories")
    parser.add_argument("wav_dir", type=Path, help="Directory of WAV files")
    parser.add_argument("csv_out", type=Path, help="Output CSV file")
    args = parser.parse_args()

    pattern = "**/*.wav" if args.recursive else "*.wav"
    files = sorted(args.wav_dir.glob(pattern))
    if not files:
        print(f"No WAV files found in {args.wav_dir}", file=sys.stderr)
        return 1

    pipe = WhisperSplitPipeline(args.model, args.encoder_device, args.decoder_device)

    with open(args.csv_out, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["file", "text"])
        for n, path in enumerate(files, 1):
            text = pipe.transcribe(load_audio(path))
            writer.writerow([str(path.relative_to(args.wav_dir)), text])
            f.flush()
            print(f"[{n}/{len(files)}] {path.name}: {text}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
