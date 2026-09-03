#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2026, Intel Corporation.

"""Live SOF mel capture from compress PCM with DTX-aware Whisper transcription.

Captures mel frames from ALSA compress device (crecord) with embedded VAD flag.
Frame format: [magic(int32), frame_number(uint32), reserved(int32),
               energy(int32), noise_energy(int32), vad_flag(int32),
               mel[0..79](int32)]

With DTX enabled, the DSP sends a configurable number of trailing silence
frames (e.g. 20 = 200ms) after each speech-to-silence transition, then
suppresses further silence. This gives the host enough silence to detect
end-of-speech via a wall-clock patience timer.

Usage:
    python sof_mel_to_text_live_compress.py [--card 0] [--device 54] [--model whisper-medium-int4-ov]
"""

import argparse
import os
import queue
import struct
import subprocess
import threading
import time
import numpy as np
import openvino as ov
import huggingface_hub as hf_hub
from pathlib import Path

# SOF compress mel frame format constants (with DSP data header)
SOF_MAGIC_BYTES = struct.pack('<i', 0x6D666363)  # ASCII 'mfcc' as int32
SOF_NUM_HEADER = 6            # magic, frame_number, reserved, energy, noise_energy, vad_flag
SOF_Q_FORMAT = 23            # Q9.23 fixed-point
SOF_NUM_MEL = 80
SOF_FRAME_INTS = SOF_NUM_HEADER + SOF_NUM_MEL  # 86 int32 per frame
SOF_FRAME_BYTES = SOF_FRAME_INTS * 4  # 344 bytes per frame
SOF_HOP_SEC = 0.01           # 10 ms per STFT hop

# Speech buffering
SILENCE_PATIENCE_S = 0.2     # seconds of silence patience before triggering
MIN_SPEECH_MS = 500          # minimum speech duration to send to Whisper
MIN_SPEECH_FRAMES = MIN_SPEECH_MS // 10  # 50 frames at 10ms/frame
MAX_SPEECH_MS = 60000        # max speech buffer before forced transcription
MAX_SPEECH_FRAMES = MAX_SPEECH_MS // 10  # 6000 frames at 10ms/frame

# Whisper model constants
WHISPER_FEATURE_SIZE = 80
WHISPER_NB_MAX_FRAMES = 3000  # 30 seconds at 10ms per frame


def decode_mel_frame(raw_ints):
    """Convert 80 int32 Q9.23 values to float32 mel coefficients."""
    return raw_ints.astype(np.float32) / (2 ** SOF_Q_FORMAT)


# ---------- Whisper inference ----------

class WhisperTranscriber:
    """Whisper encoder+decoder using OpenVINO, runs in a background thread."""

    def __init__(self, model_path, encoder_device="NPU", decoder_device="CPU"):
        self.model_path = model_path
        core = ov.Core()
        encoder_xml = str(Path(model_path) / "openvino_encoder_model.xml")
        decoder_xml = str(Path(model_path) / "openvino_decoder_model.xml")
        # NPU requires static shapes — fix [?,?,3000] to [1,80,3000]
        encoder_model = core.read_model(encoder_xml)
        encoder_model.reshape({0: [1, WHISPER_FEATURE_SIZE, WHISPER_NB_MAX_FRAMES]})
        self.encoder = core.compile_model(encoder_model, encoder_device)
        self.decoder = core.compile_model(decoder_xml, decoder_device)
        self._load_tokenizer()
        self._busy = False
        self._lock = threading.Lock()

    def _load_tokenizer(self):
        """Load Whisper tokenizer."""
        try:
            from transformers import WhisperTokenizer
            self.tokenizer = WhisperTokenizer.from_pretrained(self.model_path)
            self._tokenizer_type = "hf"
        except ImportError:
            import openvino_genai as ov_genai
            self.tokenizer = ov_genai.Tokenizer(self.model_path)
            self._tokenizer_type = "ov"

    def is_busy(self):
        with self._lock:
            return self._busy

    def transcribe_async(self, mel_frames, callback):
        """Run transcription in a background thread.

        Args:
            mel_frames: list of np.ndarray [80] mel frames
            callback: function(text) called with result
        """
        with self._lock:
            if self._busy:
                return False
            self._busy = True

        t = threading.Thread(target=self._run, args=(mel_frames, callback),
                             daemon=True)
        t.start()
        return True

    def _run(self, mel_frames, callback):
        try:
            text = self._transcribe(mel_frames)
            callback(text)
        except Exception as e:
            print(f"  [Whisper ERROR] {e}", flush=True)
        finally:
            with self._lock:
                self._busy = False

    def _transcribe(self, mel_frames):
        """Encode mel frames and decode to text."""
        n_frames = len(mel_frames)
        if n_frames == 0:
            return ""

        # Stack frames into [80, n_frames]
        features = np.column_stack(mel_frames).astype(np.float32)

        # Pad to 3000 frames
        silence_val = features.min()
        padded = np.full((WHISPER_FEATURE_SIZE, WHISPER_NB_MAX_FRAMES),
                         silence_val, dtype=np.float32)
        n = min(n_frames, WHISPER_NB_MAX_FRAMES)
        padded[:, :n] = features[:, :n]

        # Encoder
        t0 = time.time()
        encoder_input = padded.reshape(1, WHISPER_FEATURE_SIZE, WHISPER_NB_MAX_FRAMES)
        encoder_req = self.encoder.create_infer_request()
        encoder_req.set_tensor("input_features", ov.Tensor(encoder_input))
        encoder_req.infer()
        hidden_state = encoder_req.get_tensor("last_hidden_state").data.copy()
        t1 = time.time()
        print(f"  [Whisper] encoder: {t1-t0:.2f}s", flush=True)

        # Decoder: greedy decode
        token_ids = self._greedy_decode(hidden_state)
        t2 = time.time()
        print(f"  [Whisper] decoder: {t2-t1:.2f}s ({len(token_ids)} tokens)",
              flush=True)

        # Convert to text
        text_tokens = [t for t in token_ids if t < 50257]
        text = self.tokenizer.decode(text_tokens)

        return text.strip()

    def _greedy_decode(self, hidden_state, max_tokens=448):
        """Greedy decoding loop."""
        sot_tokens = [50258, 50259, 50359, 50363]
        eos_token = 50257

        decoder_req = self.decoder.create_infer_request()
        input_names = [inp.get_any_name() for inp in self.decoder.inputs]
        has_cache_position = "cache_position" in input_names

        decoder_req.set_tensor("encoder_hidden_states", ov.Tensor(hidden_state))

        # Prefill with SOT tokens
        input_ids = np.array([sot_tokens], dtype=np.int64)
        beam_idx = np.array([0], dtype=np.int32)

        decoder_req.set_tensor("input_ids", ov.Tensor(input_ids))
        if "beam_idx" in input_names:
            decoder_req.set_tensor("beam_idx", ov.Tensor(beam_idx))
        if has_cache_position:
            cache_pos = np.arange(len(sot_tokens), dtype=np.int64).reshape(1, -1)
            decoder_req.set_tensor("cache_position", ov.Tensor(cache_pos))

        decoder_req.infer()
        logits = decoder_req.get_tensor("logits").data
        next_token = int(np.argmax(logits[0, -1, :]))

        generated = [next_token]
        position = len(sot_tokens)

        for _ in range(max_tokens - 1):
            if next_token == eos_token:
                break

            decoder_req.set_tensor("input_ids",
                                   ov.Tensor(np.array([[next_token]], dtype=np.int64)))
            if "beam_idx" in input_names:
                decoder_req.set_tensor("beam_idx", ov.Tensor(beam_idx))
            if has_cache_position:
                decoder_req.set_tensor("cache_position",
                                       ov.Tensor(np.array([[position]], dtype=np.int64)))

            decoder_req.infer()
            logits = decoder_req.get_tensor("logits").data
            next_token = int(np.argmax(logits[0, -1, :]))
            generated.append(next_token)
            position += 1

        return generated


# ---------- Frame parser ----------

def parse_frame(buf):
    """Parse one complete mel frame from a bytearray buffer.

    Frame layout: [magic(4B), frame_number(4B), reserved(4B), energy(4B),
                   noise_energy(4B), vad_flag(4B), mel[0..79](320B)] = 344 bytes

    In compress PCM mode, each read delivers complete frames with no zero
    padding, so we search for magic and extract.

    Mutates buf in-place (deletes consumed bytes).
    Returns: (frame_number, vad_flag, mel_ints) or (None, None, None)
    """
    idx = buf.find(SOF_MAGIC_BYTES)
    if idx < 0:
        if len(buf) > 3:
            del buf[:-3]
        return None, None, None
    end = idx + SOF_FRAME_BYTES
    if end > len(buf):
        del buf[:idx]
        return None, None, None

    # Parse header fields
    frame_number = struct.unpack_from('<I', buf, idx + 4)[0]
    vad_flag = struct.unpack_from('<i', buf, idx + 20)[0]

    # Parse 80 mel coefficients (after 24-byte header)
    mel_bytes = bytes(buf[idx + SOF_NUM_HEADER * 4:end])
    mel_ints = np.frombuffer(mel_bytes, dtype=np.int32)
    del buf[:end]
    return frame_number, vad_flag, mel_ints


# ---------- Main capture + transcription loop ----------

def try_transcribe(transcriber, speech_buffer, t, on_transcription):
    """Attempt to send speech buffer to Whisper. Returns True if sent or discarded."""
    n = len(speech_buffer)
    duration = n * SOF_HOP_SEC

    if n < MIN_SPEECH_FRAMES:
        print(f"  [{t:7.2f}s] Too short ({duration:.1f}s), "
              f"discarding {n} frames", flush=True)
        return True

    if not transcriber.is_busy():
        print(f"  [{t:7.2f}s] Transcribing {n} frames "
              f"({duration:.1f}s)...", flush=True)
        frames_copy = list(speech_buffer)
        transcriber.transcribe_async(frames_copy, on_transcription)
        return True

    print(f"  [{t:7.2f}s] (Whisper busy, queuing {n} frames)", flush=True)
    return False


def run_capture(card, device, model_path, encoder_device, decoder_device,
                patience=SILENCE_PATIENCE_S):
    """Main capture loop: crecord compress PCM → DSP VAD → buffer speech → Whisper.

    With DTX, the FW sends:
    - All VAD=1 (speech) frames
    - Trailing VAD=0 silence frames (e.g. 20 = 200ms) after speech ends

    A wall-clock patience timer triggers transcription after silence.
    If speech resumes within the patience window, buffering continues.
    """

    transcriber = WhisperTranscriber(model_path, encoder_device=encoder_device,
                                     decoder_device=decoder_device)

    crecord_cmd = [
        'crecord', '-v',
        '-c', str(card),
        '-d', str(device),
        '-I', 'BESPOKE',
        '-R', '16000',
        '-C', '2',
        '-F', 'S32_LE',
        '-b', '6144',
        '-f', '3',
    ]

    # Wrap with stdbuf to disable crecord's stdio buffering. When stdout
    # is a pipe, C stdio uses full buffering (~4-8KB). A single DTX
    # silence frame (344 bytes) would sit in crecord's buffer until enough
    # data accumulates, delaying the patience timer by many seconds.
    cmd = ['stdbuf', '-o0'] + crecord_cmd

    print(f"Starting compress capture: {' '.join(crecord_cmd)}")
    print(f"VAD source: DSP (embedded in stream, DTX mode)")
    print(f"Silence patience: {patience}s")
    print(f"Whisper model: {model_path} (encoder: {encoder_device}, decoder: {decoder_device})")
    print()

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            bufsize=0)

    # Reader thread feeds parsed frames into a queue to decouple pipe I/O
    # from the patience timer in the main thread.
    frame_q = queue.Queue()

    def reader_thread():
        buf = bytearray()
        raw_fd = proc.stdout.fileno()
        try:
            while True:
                # Use os.read() for unbuffered reads — returns immediately
                # when any data is available. Python's read(n) waits for
                # exactly n bytes, which delays SILENCE frames until the
                # next speech burst fills the buffer.
                data = os.read(raw_fd, SOF_FRAME_BYTES * 4)
                if not data:
                    break
                buf.extend(data)
                while True:
                    frame_number, vad_flag, frame_ints = parse_frame(buf)
                    if frame_ints is None:
                        break
                    frame_q.put((frame_number, vad_flag, frame_ints))
        except (OSError, ValueError):
            pass
        frame_q.put(None)  # sentinel

    reader = threading.Thread(target=reader_thread, daemon=True)
    reader.start()

    recv_frames = 0
    prev_speech = None
    last_hop = 0

    # Speech buffering state
    speech_buffer = []         # list of mel frames during speech
    silence_time = None        # wall-clock time when first VAD=0 arrived
    pending_queue = None       # queued frames waiting for Whisper to become free
    pending_t = 0.0            # timestamp for queued frames

    def on_transcription(text):
        if text:
            print(f"\n  >> \"{text}\"\n", flush=True)
        else:
            print("  [Whisper] empty result", flush=True)

    def flush_speech(t_now):
        """Flush speech buffer to Whisper."""
        nonlocal speech_buffer, silence_time, pending_queue, pending_t
        if not speech_buffer:
            silence_time = None
            return
        if not try_transcribe(transcriber, speech_buffer, t_now,
                              on_transcription):
            pending_queue = list(speech_buffer)
            pending_t = t_now
        speech_buffer.clear()
        silence_time = None

    try:
        while True:
            # Calculate queue timeout based on patience timer
            get_timeout = 0.1  # default polling interval
            if silence_time is not None:
                remaining = patience - (time.monotonic() - silence_time)
                get_timeout = max(remaining, 0.01)

            try:
                item = frame_q.get(timeout=get_timeout)
            except queue.Empty:
                # Patience expired — flush speech to Whisper
                if silence_time is not None:
                    elapsed = time.monotonic() - silence_time
                    if elapsed >= patience:
                        t = last_hop * SOF_HOP_SEC
                        flush_speech(t)

                # Drain pending queue when Whisper becomes free
                if pending_queue is not None and not transcriber.is_busy():
                    print(f"  [{pending_t:7.2f}s] Whisper free, sending "
                          f"{len(pending_queue)} queued frames", flush=True)
                    transcriber.transcribe_async(pending_queue, on_transcription)
                    pending_queue = None
                continue

            if item is None:
                # Reader thread ended (crecord exited)
                stderr_out = proc.stderr.read().decode(errors='replace')
                rc = proc.wait()
                print(f"\ncrecord exited with code {rc}")
                if stderr_out:
                    print(f"stderr: {stderr_out}")
                break

            frame_number, vad_flag, frame_ints = item
            recv_frames += 1
            last_hop = frame_number
            mel = decode_mel_frame(frame_ints)
            speech = vad_flag != 0
            t = frame_number * SOF_HOP_SEC

            # Print VAD transitions
            if speech != prev_speech:
                tag = "SPEECH" if speech else "SILENCE"
                print(f"  [{t:7.2f}s] {tag} (hop {frame_number}, "
                      f"received {recv_frames})", flush=True)
            prev_speech = speech

            # Drain pending queue when Whisper becomes free
            if pending_queue is not None and not transcriber.is_busy():
                print(f"  [{pending_t:7.2f}s] Whisper free, sending "
                      f"{len(pending_queue)} queued frames", flush=True)
                transcriber.transcribe_async(pending_queue, on_transcription)
                pending_queue = None

            # --- Speech buffering logic ---
            if speech:
                if len(speech_buffer) >= MAX_SPEECH_FRAMES:
                    n = len(speech_buffer)
                    duration = n * SOF_HOP_SEC
                    print(f"  [{t:7.2f}s] Buffer full ({duration:.1f}s), "
                          f"forcing transcription", flush=True)
                    flush_speech(t)

                speech_buffer.append(mel.copy())
                silence_time = None  # speech resumed, cancel patience timer

            else:
                # VAD=0: start patience timer if we have buffered speech.
                # Don't refresh if already running so trailing silence
                # frames don't extend the wait.
                if speech_buffer and silence_time is None:
                    silence_time = time.monotonic()

    except (KeyboardInterrupt, BrokenPipeError):
        pass
    finally:
        # Flush remaining speech
        if speech_buffer:
            t = last_hop * SOF_HOP_SEC
            flush_speech(t)
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        print(f"\n\nCapture stopped. Received {recv_frames} frames.")


def main():
    parser = argparse.ArgumentParser(
        description="Live SOF mel capture from compress PCM with DTX-aware "
                    "Whisper transcription")
    parser.add_argument('--card', '-c', type=int, default=0,
                        help='ALSA card number (default: 0)')
    parser.add_argument('--device', '-d', type=int, default=54,
                        help='ALSA compress device number (default: 54)')
    parser.add_argument('--model', '-m', default='whisper-medium-int4-ov',
                        help='Path to Whisper OpenVINO model directory')
    parser.add_argument('--encoder-device', default='NPU',
                        help='OpenVINO device for encoder (default: NPU)')
    parser.add_argument('--decoder-device', default='CPU',
                        help='OpenVINO device for decoder (default: CPU)')
    parser.add_argument('--patience', type=float, default=SILENCE_PATIENCE_S,
                        help=f'Seconds of silence patience before triggering '
                             f'transcription (default: {SILENCE_PATIENCE_S})')
    args = parser.parse_args()

    model_id = "OpenVINO/" + os.path.basename(args.model)
    if not os.path.isdir(args.model):
        print(f"Downloading model {model_id} ...")
        hf_hub.snapshot_download(model_id, local_dir=args.model)

    print("=== Live SOF Mel → Whisper Transcription (Compress PCM, DTX) ===\n")
    run_capture(args.card, args.device, args.model, args.encoder_device,
                args.decoder_device, patience=args.patience)


if __name__ == '__main__':
    main()
