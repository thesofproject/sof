"""Live SOF mel capture with DSP VAD-triggered Whisper transcription.

Captures mel frames from ALSA with embedded VAD flag from the DSP.
Frame format: [magic(int32), frame_number(uint32), reserved(int32), energy(int32), noise_energy(int32), vad_flag(int32), mel[0..79](int32)]
When silence of 100ms is detected after speech, sends the buffered mel
features to Whisper (OpenVINO encoder+decoder) for transcription.
Capture continues running during Whisper inference.

Usage:
    python sof_mel_to_text_live_dsp_vad.py [--device hw:0,47] [--model whisper-medium-int4-ov]
    python sof_mel_to_text_live_dsp_vad.py --plot  # with live spectrogram
"""

import argparse
import os
import struct
import subprocess
import threading
import time
import numpy as np
import openvino as ov
import huggingface_hub as hf_hub
from pathlib import Path

# Graphics imports deferred until --plot is used
matplotlib = None
plt = None

# SOF mel_s32.raw format constants (with DSP data header)
SOF_MAGIC_BYTES = struct.pack('<i', 0x6D666363)  # ASCII 'mfcc' as int32
SOF_NUM_HEADER = 6            # magic, frame_number, reserved, energy, noise_energy, vad_flag
SOF_Q_FORMAT = 23            # Q9.23 fixed-point
SOF_NUM_MEL = 80
SOF_FRAME_INTS = SOF_NUM_HEADER + SOF_NUM_MEL  # 86 int32 per frame
SOF_FRAME_BYTES = SOF_FRAME_INTS * 4  # 344 bytes per frame

# Speech buffering
SILENCE_TRIGGER_MS = 100     # ms of silence after speech to trigger transcription
SILENCE_TRIGGER_FRAMES = SILENCE_TRIGGER_MS // 10  # 10 frames at 10ms/frame
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


# ---------- Optional scrolling plot ----------

SPECTROGRAM_WIDTH = 100


class MelPlotter:
    """Real-time scrolling mel spectrogram + VAD strip."""

    def __init__(self, num_mel=SOF_NUM_MEL, width=SPECTROGRAM_WIDTH):
        global matplotlib, plt
        import matplotlib as _mpl
        _mpl.use('TkAgg')
        import matplotlib.pyplot as _plt
        matplotlib = _mpl
        plt = _plt

        self.num_mel = num_mel
        self.width = width

        self.mel_buf = np.zeros((num_mel, width), dtype=np.float32)
        self.vad_buf = np.zeros(width, dtype=np.float32)
        self.x = np.arange(width)

        self.fig, (self.ax_mel, self.ax_vad) = plt.subplots(
            2, 1, figsize=(10, 5),
            gridspec_kw={'height_ratios': [5, 1]},
            sharex=True
        )
        self.fig.tight_layout(pad=2.0)

        self.im_mel = self.ax_mel.imshow(
            self.mel_buf, aspect='auto', origin='lower',
            interpolation='nearest', cmap='turbo',
            vmin=-2.0, vmax=2.0
        )
        self.ax_mel.set_ylabel('Mel bin')
        self.ax_mel.set_title('Mel Spectrogram (scrolling) — DSP VAD')

        self.line_vad, = self.ax_vad.plot(
            self.x, self.vad_buf, color='green', linewidth=1.5,
            drawstyle='steps-post')
        self.ax_vad.set_ylabel('VAD')
        self.ax_vad.set_xlabel('Frame')
        self.ax_vad.set_ylim(-0.1, 1.1)
        self.ax_vad.set_yticks([0, 1])
        self.ax_vad.set_yticklabels(['Silent', 'Speech'])

        plt.ion()
        plt.show(block=False)
        self.fig.canvas.draw()
        self.fig.canvas.flush_events()

    def update(self, mel_frame, is_speech):
        self.mel_buf[:, :-1] = self.mel_buf[:, 1:]
        self.mel_buf[:, -1] = mel_frame
        self.vad_buf[:-1] = self.vad_buf[1:]
        self.vad_buf[-1] = 1.0 if is_speech else 0.0

        self.im_mel.set_data(self.mel_buf)
        self.line_vad.set_ydata(self.vad_buf)

        self.fig.canvas.draw_idle()
        self.fig.canvas.flush_events()


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

def find_frame_in_buffer(buf):
    """Find the first complete mel frame with data header in a bytearray.

    Frame layout: [magic(4B), frame_number(4B), reserved(4B), energy(4B),
                   noise_energy(4B), vad_flag(4B), mel[0..79](320B)] = 344 bytes
    Mutates buf in-place (deletes consumed bytes).
    Returns: (vad_flag, mel_ints) or (None, None)
    """
    idx = buf.find(SOF_MAGIC_BYTES)
    if idx < 0:
        if len(buf) > 3:
            del buf[:-3]
        return None, None
    end = idx + SOF_FRAME_BYTES
    if end > len(buf):
        del buf[:idx]
        return None, None
    # Parse vad_flag at offset 20 (after magic + frame_number + reserved + energy + noise_energy)
    vad_flag = struct.unpack_from('<i', buf, idx + 20)[0]
    # Parse 80 mel coefficients (after 24-byte header)
    mel_bytes = bytes(buf[idx + SOF_NUM_HEADER * 4 : end])
    mel_ints = np.frombuffer(mel_bytes, dtype=np.int32)
    del buf[:end]
    return vad_flag, mel_ints


# ---------- Main capture + transcription loop ----------

def run_capture(device, rate, model_path, encoder_device, decoder_device,
                enable_plot=False):
    """Main capture loop: ALSA → DSP VAD → buffer speech → Whisper."""

    plotter = MelPlotter() if enable_plot else None
    transcriber = WhisperTranscriber(model_path, encoder_device=encoder_device,
                                     decoder_device=decoder_device)

    cmd = [
        'arecord', '-D', device, '-f', 'S32_LE', '-c', '2',
        '-r', str(rate), '-t', 'raw', '--buffer-size', '8192',
    ]

    print(f"Starting capture: {' '.join(cmd)}")
    print(f"VAD source: DSP (embedded in stream)")
    print(f"Silence trigger: {SILENCE_TRIGGER_MS}ms ({SILENCE_TRIGGER_FRAMES} frames)")
    print(f"Whisper model: {model_path} (encoder: {encoder_device}, decoder: {decoder_device})")
    print()

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    buf = bytearray()
    read_chunk = SOF_FRAME_BYTES * 4
    frame_num = 0
    prev_speech = None

    # Speech buffering state
    speech_buffer = []         # list of mel frames during speech
    silence_counter = 0        # consecutive silence frames after speech
    was_speaking = False       # True if we have buffered speech frames

    def on_transcription(text):
        if text:
            print(f"\n  >> \"{text}\"\n", flush=True)
        else:
            print("  [Whisper] empty result", flush=True)

    try:
        while True:
            data = proc.stdout.read(read_chunk)
            if not data:
                rc = proc.poll()
                if rc is not None:
                    stderr_out = proc.stderr.read().decode(errors='replace')
                    print(f"\narecord exited with code {rc}")
                    if stderr_out:
                        print(f"stderr: {stderr_out}")
                    break
                continue

            buf.extend(data)

            while True:
                vad_flag, frame_ints = find_frame_in_buffer(buf)
                if frame_ints is None:
                    break

                frame_num += 1
                mel = decode_mel_frame(frame_ints)
                speech = vad_flag != 0

                # Print VAD transitions when not plotting
                if plotter is None and speech != prev_speech:
                    t = frame_num * 0.01
                    tag = "SPEECH" if speech else "SILENCE"
                    print(f"  [{t:7.2f}s] {tag}", flush=True)
                prev_speech = speech

                # Update plot
                if plotter is not None:
                    plotter.update(mel, speech)

                # --- Speech buffering logic ---
                if speech:
                    if len(speech_buffer) >= MAX_SPEECH_FRAMES:
                        n = len(speech_buffer)
                        duration = n * 0.01
                        t = frame_num * 0.01
                        print(f"  [{t:7.2f}s] Buffer full ({duration:.1f}s), "
                              f"forcing transcription of {n} frames",
                              flush=True)
                        if not transcriber.is_busy():
                            frames_copy = list(speech_buffer)
                            transcriber.transcribe_async(
                                frames_copy, on_transcription)
                        else:
                            print(f"  [{t:7.2f}s] (Whisper busy, "
                                  f"dropping {n} frames)", flush=True)
                        speech_buffer.clear()
                    speech_buffer.append(mel.copy())
                    silence_counter = 0
                    was_speaking = True
                else:
                    if was_speaking:
                        silence_counter += 1
                        if silence_counter >= SILENCE_TRIGGER_FRAMES:
                            n = len(speech_buffer)
                            duration = n * 0.01
                            t = frame_num * 0.01

                            if n < MIN_SPEECH_FRAMES:
                                # Too short — discard
                                speech_buffer.clear()
                                silence_counter = 0
                                was_speaking = False
                                continue

                            # Silence threshold reached — send to Whisper
                            print(f"  [{t:7.2f}s] Transcribing {n} frames "
                                  f"({duration:.1f}s)...", flush=True)

                            if not transcriber.is_busy():
                                frames_copy = list(speech_buffer)
                                transcriber.transcribe_async(
                                    frames_copy, on_transcription)
                            else:
                                print(f"  [{t:7.2f}s] (Whisper busy, "
                                      f"dropping {n} frames)", flush=True)

                            speech_buffer.clear()
                            silence_counter = 0
                            was_speaking = False

    except (KeyboardInterrupt, BrokenPipeError):
        pass
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        if plotter is not None:
            try:
                plt.close(plotter.fig)
            except Exception:
                pass
        print("\n\nCapture stopped.")


def main():
    parser = argparse.ArgumentParser(
        description="Live SOF mel capture with DSP VAD-triggered Whisper transcription")
    parser.add_argument('--device', '-D', default='hw:0,47',
                        help='ALSA capture device (default: hw:0,47)')
    parser.add_argument('--rate', '-r', type=int, default=16000,
                        help='Sample rate for arecord (default: 16000)')
    parser.add_argument('--model', '-m', default='whisper-medium-int4-ov',
                        help='Path to Whisper OpenVINO model directory')
    parser.add_argument('--encoder-device', default='NPU',
                        help='OpenVINO device for encoder (default: NPU)')
    parser.add_argument('--decoder-device', default='CPU',
                        help='OpenVINO device for decoder (default: CPU)')
    parser.add_argument('--plot', action='store_true',
                        help='Show live scrolling mel spectrogram and VAD plot')
    args = parser.parse_args()
    model_id = "OpenVINO/" + os.path.basename(args.model)
    if not os.path.isdir(args.model):
        print(f"Downloading model {model_id} ...")
        hf_hub.snapshot_download(model_id, local_dir=args.model)

    print("=== Live SOF Mel → Whisper Transcription (DSP VAD) ===\n")
    run_capture(args.device, args.rate, args.model, args.encoder_device,
                args.decoder_device, enable_plot=args.plot)


if __name__ == '__main__':
    main()
