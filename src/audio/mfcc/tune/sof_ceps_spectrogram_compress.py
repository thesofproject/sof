#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2026, Intel Corporation.

"""Live scrolling cepstral coefficient viewer for SOF compress PCM capture.

GTK 4 application that displays a live scrolling MFCC cepstral
spectrogram with VAD status from a SOF compress audio-features PCM.
Rendering goes through ``Gtk.Snapshot`` with ``Gdk.MemoryTexture`` for
the spectrogram and native GSK render nodes (``Gsk.Path`` strokes,
``append_color``, ``append_layout``) for the strip plots, so the
GL/Vulkan renderer handles compositing on the GPU. Vsync-aligned
redraws (``add_tick_callback``) and sub-pixel scrolling keep the 100 Hz
data stream looking smooth on a 60+ Hz display.

Dependencies (Debian/Ubuntu):
    sudo apt install python3-gi gir1.2-gtk-4.0
    pip install numpy

Frame format: [magic(int32), frame_number(uint32), reserved(int32),
               energy(int32), noise_energy(int32), vad_flag(int32),
               ceps[0..N-1](int32)]

Cepstral coefficients are in Q9.23 fixed-point format.

Usage:
    python sof_ceps_spectrogram_compress.py [--card 0] [--device 54]
    python sof_ceps_spectrogram_compress.py --num-ceps 13 --width 300
"""

import argparse
import os
import queue
import signal
import struct
import subprocess
import sys
import threading
import time

import numpy as np

import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Gdk', '4.0')
gi.require_version('Gsk', '4.0')
gi.require_version('Graphene', '1.0')
gi.require_version('Pango', '1.0')
from gi.repository import GLib, Gtk  # noqa: E402

# Reuse the GPU-accelerated widgets from the mel viewer.
from sof_mel_spectrogram_compress import (  # noqa: E402
    MelSpectrogramArea as SpectrogramArea,
    CurveStripArea,
    GUI_REFRESH_MS,
)

# SOF compress frame format constants (with DSP data header)
SOF_MAGIC_BYTES = struct.pack('<i', 0x6D666363)  # ASCII 'mfcc' as int32
SOF_NUM_HEADER = 6            # magic, frame_number, reserved, energy, noise_energy, vad_flag
SOF_HEADER_BYTES = SOF_NUM_HEADER * 4  # 24 bytes
SOF_HOP_SEC = 0.01            # 10 ms per STFT hop

SPECTROGRAM_WIDTH = 200       # default number of frames visible
DEFAULT_NUM_CEPS = 13         # default cepstral coefficients
Q_FORMAT = 23                 # Q9.23 fixed-point

# Color range for the image (cepstral coefficients in float)
CEPS_VMIN = -50.0
CEPS_VMAX = 50.0


def decode_ceps_frame(raw_ints):
    """Convert int32 Q9.23 cepstral coefficients to float32."""
    return raw_ints.astype(np.float32) / (2 ** Q_FORMAT)


def parse_frame(buf, num_ceps):
    """Parse one complete ceps frame from a bytearray buffer.

    Mutates buf in-place (deletes consumed bytes).
    Returns: (frame_number, vad_flag, energy, noise, ceps_ints) or
             (None, None, None, None, None) when no frame is ready.
    """
    frame_bytes = SOF_HEADER_BYTES + num_ceps * 4
    idx = buf.find(SOF_MAGIC_BYTES)
    if idx < 0:
        if len(buf) > 3:
            del buf[:-3]
        return None, None, None, None, None
    end = idx + frame_bytes
    if end > len(buf):
        del buf[:idx]
        return None, None, None, None, None

    frame_number = struct.unpack_from('<I', buf, idx + 4)[0]
    energy_int = struct.unpack_from('<i', buf, idx + 12)[0]
    noise_int = struct.unpack_from('<i', buf, idx + 16)[0]
    vad_flag = struct.unpack_from('<i', buf, idx + 20)[0]

    ceps_bytes = bytes(buf[idx + SOF_HEADER_BYTES:end])
    ceps_ints = np.frombuffer(ceps_bytes, dtype=np.int32)
    del buf[:end]
    energy = energy_int / (2 ** Q_FORMAT)
    noise = noise_int / (2 ** Q_FORMAT)
    return frame_number, vad_flag, energy, noise, ceps_ints


def start_crecord(card, device):
    """Spawn crecord as subprocess; return Popen."""
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
    cmd = ['stdbuf', '-o0'] + crecord_cmd
    print(f"Starting compress capture: {' '.join(crecord_cmd)}")
    return subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            bufsize=0)


class CepsViewer:
    """Top-level controller: capture process + GTK window."""

    def __init__(self, app, card, device, width, num_ceps):
        self.app = app
        self.num_ceps = num_ceps
        self.width_frames = width
        self.frame_bytes = SOF_HEADER_BYTES + num_ceps * 4

        # Ring buffers stored as (frames, ceps) for cheap shifts along axis 0.
        self.ceps_buf = np.zeros((width, num_ceps), dtype=np.float32)
        self.vad_buf = np.zeros(width, dtype=np.float32)
        self.energy_buf = np.zeros(width, dtype=np.float32)
        self.noise_buf = np.zeros(width, dtype=np.float32)

        self.recv_frames = 0
        self.prev_speech = None
        self._closing = False
        self._last_frame_mono = None
        self._frame_period = SOF_HOP_SEC

        self._build_ui()

        # Start capture
        self.proc = start_crecord(card, device)
        self.frame_q = queue.Queue()
        self.reader = threading.Thread(target=self._reader_thread, daemon=True)
        self.reader.start()

        # Vsync-aligned redraws via the frame clock; fall back to a periodic
        # timer if add_tick_callback is unavailable.
        self._tick_id = 0
        self._timer_id = 0
        if hasattr(self.ceps_area, 'add_tick_callback'):
            self._tick_id = self.ceps_area.add_tick_callback(self._on_tick_clock)
        else:
            self._timer_id = GLib.timeout_add(GUI_REFRESH_MS, self._on_timer)

        print(f"Cepstral coefficients: {num_ceps} (frame size: {self.frame_bytes} bytes)")
        print(f"Spectrogram width: {width} frames ({width * SOF_HOP_SEC:.1f}s)")
        if self._tick_id:
            print("GUI refresh: vsync-aligned via frame clock, "
                  "sub-pixel scroll enabled")
        else:
            print(f"GUI refresh: fallback timer at {GUI_REFRESH_MS} ms "
                  f"({1000.0 / GUI_REFRESH_MS:.0f} FPS)")
        print()

    def _build_ui(self):
        self.window = Gtk.ApplicationWindow(application=self.app)
        self.window.set_title(
            f"SOF MFCC Cepstral Coefficients ({self.num_ceps} ceps) — DSP VAD"
        )
        self.window.set_default_size(1100, 600)
        self.window.connect('close-request', self._on_close)

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        box.set_margin_start(4)
        box.set_margin_end(4)
        box.set_margin_top(4)
        box.set_margin_bottom(4)
        self.window.set_child(box)

        # Spectrogram-style cepstral image (re-uses the GPU texture widget).
        self.ceps_area = SpectrogramArea(self.ceps_buf, CEPS_VMIN, CEPS_VMAX)
        self.vad_area = CurveStripArea(
            lambda: {'vad': self.vad_buf},
            ymin=-0.1, ymax=1.1,
            lines=[('vad', (0.2, 0.9, 0.2, 1.0))],
            yticks=[(0, 'Silent'), (1, 'Speech')],
            label='VAD',
            height_request=70,
        )
        self.level_area = CurveStripArea(
            lambda: {'energy': self.energy_buf, 'noise': self.noise_buf},
            autoscale=True,
            lines=[
                ('energy', (1.0, 0.9, 0.2, 1.0)),
                ('noise', (0.2, 0.9, 1.0, 1.0)),
            ],
            label='Level (Y=speech, C=noise)',
            height_request=110,
        )

        self.ceps_area.set_vexpand(True)
        box.append(self.ceps_area)
        box.append(self.vad_area)
        box.append(self.level_area)

    # --- I/O thread ---------------------------------------------------
    def _reader_thread(self):
        buf = bytearray()
        raw_fd = self.proc.stdout.fileno()
        try:
            while True:
                data = os.read(raw_fd, self.frame_bytes * 4)
                if not data:
                    break
                buf.extend(data)
                while True:
                    fn, vf, en, no, ci = parse_frame(buf, self.num_ceps)
                    if ci is None:
                        break
                    self.frame_q.put((fn, vf, en, no, ci))
        except OSError:
            pass
        self.frame_q.put(None)

    # --- GUI tick -----------------------------------------------------
    def _drain_and_redraw(self):
        drained = 0
        eof = False
        while True:
            try:
                item = self.frame_q.get_nowait()
            except queue.Empty:
                break
            if item is None:
                eof = True
                break
            fn, vf, en, no, ci = item
            self._ingest(fn, vf, en, no, ci)
            drained += 1
            if drained >= 64:
                break

        if self._last_frame_mono is None:
            sub = 0.0
        else:
            sub = (time.monotonic() - self._last_frame_mono) / self._frame_period
            if sub < 0.0:
                sub = 0.0
            elif sub > 1.0:
                sub = 1.0
        self.ceps_area.set_sub_offset(sub)
        self.vad_area.set_sub_offset(sub)
        self.level_area.set_sub_offset(sub)

        if eof:
            self._on_eof()
            return False
        return True

    def _on_tick_clock(self, _widget, _frame_clock):
        return True if self._drain_and_redraw() else False

    def _on_timer(self):
        return GLib.SOURCE_CONTINUE if self._drain_and_redraw() \
            else GLib.SOURCE_REMOVE

    def _ingest(self, frame_number, vad_flag, energy, noise, ceps_ints):
        self.recv_frames += 1
        self._last_frame_mono = time.monotonic()
        ceps = decode_ceps_frame(ceps_ints)
        speech = vad_flag != 0

        if speech != self.prev_speech:
            t = frame_number * SOF_HOP_SEC
            tag = "SPEECH" if speech else "SILENCE"
            print(f"  [{t:7.2f}s] {tag} (hop {frame_number})", flush=True)
        self.prev_speech = speech

        # Scroll left and append new frame on the right.
        self.ceps_buf[:-1] = self.ceps_buf[1:]
        self.ceps_buf[-1] = ceps
        self.vad_buf[:-1] = self.vad_buf[1:]
        self.vad_buf[-1] = 1.0 if speech else 0.0
        self.energy_buf[:-1] = self.energy_buf[1:]
        self.energy_buf[-1] = energy
        self.noise_buf[:-1] = self.noise_buf[1:]
        self.noise_buf[-1] = noise

    def _on_eof(self):
        if self._closing:
            return
        if self.proc.poll() is None:
            self.proc.terminate()
        try:
            rc = self.proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            rc = self.proc.wait()
        try:
            stderr_out = self.proc.stderr.read().decode(errors='replace')
        except Exception:
            stderr_out = ''
        print(f"\ncrecord exited with code {rc}")
        if stderr_out:
            print(f"stderr: {stderr_out}")
        self.window.close()

    # --- shutdown -----------------------------------------------------
    def _on_close(self, *_args):
        self._closing = True
        if self._tick_id:
            self.ceps_area.remove_tick_callback(self._tick_id)
            self._tick_id = 0
        if self._timer_id:
            GLib.source_remove(self._timer_id)
            self._timer_id = 0
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        print(f"\nCapture stopped. Received {self.recv_frames} frames.")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Live scrolling MFCC cepstral coefficient viewer "
                    "from SOF compress PCM capture (GTK 4 / GSK)")
    parser.add_argument('--card', '-c', type=int, default=0,
                        help='ALSA card number (default: 0)')
    parser.add_argument('--device', '-d', type=int, default=54,
                        help='ALSA compress device number (default: 54)')
    parser.add_argument('--width', '-w', type=int, default=SPECTROGRAM_WIDTH,
                        help=f'Spectrogram width in frames (default: {SPECTROGRAM_WIDTH})')
    parser.add_argument('--num-ceps', '-n', type=int, default=DEFAULT_NUM_CEPS,
                        help=f'Number of cepstral coefficients (default: {DEFAULT_NUM_CEPS})')
    args = parser.parse_args()

    print("=== SOF MFCC Cepstral Coefficient Viewer (Compress PCM, GTK 4) ===\n")

    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = Gtk.Application(application_id='org.sof.ceps_spectrogram')
    holder = {}

    def on_activate(application):
        viewer = CepsViewer(application, args.card, args.device,
                            args.width, args.num_ceps)
        viewer.window.present()
        holder['viewer'] = viewer

    app.connect('activate', on_activate)
    sys.exit(app.run(None))


if __name__ == '__main__':
    main()
