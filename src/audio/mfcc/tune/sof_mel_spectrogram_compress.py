#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2026, Intel Corporation.

"""Live scrolling mel spectrogram viewer for SOF compress PCM capture.

GTK 4 application that displays a live scrolling mel spectrogram with
VAD status from a SOF compress audio-features PCM. Rendering uses
``Gtk.Snapshot`` with a ``Gdk.MemoryTexture`` for the spectrogram so
that the per-frame upload is handed to the GSK GPU renderer; the small
VAD / level strips use Cairo snapshot nodes.

Dependencies (Debian/Ubuntu):
    sudo apt install python3-gi gir1.2-gtk-4.0
    pip install numpy

Frame format: [magic(int32), frame_number(uint32), reserved(int32),
               energy(int32), noise_energy(int32), vad_flag(int32),
               mel[0..79](int32)]

Usage:
    python sof_mel_spectrogram_compress.py [--card 0] [--device 54]
    python sof_mel_spectrogram_compress.py --width 400
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
from gi.repository import Gdk, GLib, Gsk, Gtk, Graphene, Pango  # noqa: E402

# SOF compress mel frame format constants (with DSP data header)
SOF_MAGIC_BYTES = struct.pack('<i', 0x6D666363)  # ASCII 'mfcc' as int32
SOF_NUM_HEADER = 6            # magic, frame_number, reserved, energy, noise_energy, vad_flag
SOF_Q_FORMAT = 23             # Q9.23 fixed-point
SOF_NUM_MEL = 80
SOF_FRAME_INTS = SOF_NUM_HEADER + SOF_NUM_MEL  # 86 int32 per frame
SOF_FRAME_BYTES = SOF_FRAME_INTS * 4           # 344 bytes per frame
SOF_HOP_SEC = 0.01            # 10 ms per STFT hop

SPECTROGRAM_WIDTH = 200       # default number of frames visible

# Color range for the mel image (float coefficients)
MEL_VMIN = -2.0
MEL_VMAX = 2.0

# Fallback redraw period when no frame clock is available (ms).
GUI_REFRESH_MS = 16


def decode_mel_frame(raw_ints):
    """Convert 80 int32 Q9.23 values to float32 mel coefficients."""
    return raw_ints.astype(np.float32) / (2 ** SOF_Q_FORMAT)


def parse_frame(buf):
    """Parse one complete mel frame from a bytearray buffer.

    Mutates buf in-place (deletes consumed bytes).
    Returns: (frame_number, vad_flag, energy, noise, mel_ints) or
             (None, None, None, None, None) when no frame is ready.
    """
    idx = buf.find(SOF_MAGIC_BYTES)
    if idx < 0:
        if len(buf) > 3:
            del buf[:-3]
        return None, None, None, None, None
    end = idx + SOF_FRAME_BYTES
    if end > len(buf):
        del buf[:idx]
        return None, None, None, None, None

    frame_number = struct.unpack_from('<I', buf, idx + 4)[0]
    energy_int = struct.unpack_from('<i', buf, idx + 12)[0]
    noise_int = struct.unpack_from('<i', buf, idx + 16)[0]
    vad_flag = struct.unpack_from('<i', buf, idx + 20)[0]

    mel_bytes = bytes(buf[idx + SOF_NUM_HEADER * 4:end])
    mel_ints = np.frombuffer(mel_bytes, dtype=np.int32)
    del buf[:end]
    energy = energy_int / (2 ** SOF_Q_FORMAT)
    noise = noise_int / (2 ** SOF_Q_FORMAT)
    return frame_number, vad_flag, energy, noise, mel_ints


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


def _build_turbo_lut(n=256):
    """Return an Nx4 uint8 RGBA lookup table for the 'turbo' colormap.

    Polynomial approximation by A. Mikhailov (Google, public-domain
    snippet) — fast and dependency-free.
    """
    t = np.linspace(0.0, 1.0, n, dtype=np.float32)
    r = (0.13572138 + 4.61539260 * t - 42.66032258 * t**2
         + 132.13108234 * t**3 - 152.94239396 * t**4
         + 59.28637943 * t**5)
    g = (0.09140261 + 2.19418839 * t + 4.84296658 * t**2
         - 14.18503333 * t**3 + 4.27729857 * t**4
         + 2.82956604 * t**5)
    b = (0.10667330 + 12.64194608 * t - 60.58204836 * t**2
         + 110.36276771 * t**3 - 89.90310912 * t**4
         + 27.34824973 * t**5)
    rgb = np.clip(np.stack([r, g, b], axis=1), 0.0, 1.0)
    rgba = np.concatenate(
        [rgb, np.ones((n, 1), dtype=np.float32)], axis=1)
    return (rgba * 255.0).astype(np.uint8)


TURBO_LUT = _build_turbo_lut(256)


class MelSpectrogramArea(Gtk.Widget):
    """GTK 4 widget that renders the mel ring buffer as a GPU texture.

    The numpy buffer is colour-mapped to RGBA8 once per redraw, wrapped
    in a ``Gdk.MemoryTexture`` and pushed to the snapshot. The GSK
    renderer uploads it to the GPU and scales it to the widget size.

    Supports sub-pixel scrolling via :meth:`set_sub_offset`: the texture
    is built with one extra column on the right (a hold of the latest
    frame) and translated left by ``sub * column_width`` so the newest
    column slides smoothly in between data arrivals.
    """

    def __init__(self, mel_buf, vmin, vmax):
        super().__init__()
        self._mel_buf = mel_buf            # shape (frames, bins)
        self._vmin = vmin
        self._vmax = vmax
        self._lut = TURBO_LUT
        self._sub = 0.0
        self.set_hexpand(True)
        self.set_vexpand(True)

    def set_sub_offset(self, sub):
        if sub < 0.0:
            sub = 0.0
        elif sub > 1.0:
            sub = 1.0
        if sub != self._sub:
            self._sub = sub
            self.queue_draw()

    def _build_texture(self):
        bins = self._mel_buf.shape[1]
        frames = self._mel_buf.shape[0]
        norm = (self._mel_buf - self._vmin) / (self._vmax - self._vmin)
        np.clip(norm, 0.0, 1.0, out=norm)
        idx = (norm * 255.0).astype(np.uint8)              # (frames, bins)
        # Extend on the right by holding the latest column constant so the
        # sub-pixel translate has something to reveal at the right edge.
        idx_ext = np.empty((frames + 1, bins), dtype=np.uint8)
        idx_ext[:frames] = idx
        idx_ext[frames] = idx[-1]
        idx_img = idx_ext.T[::-1, :]                       # (bins, frames+1)
        rgba = self._lut[idx_img]                          # (bins, frames+1, 4)
        data = rgba.tobytes()
        gbytes = GLib.Bytes.new(data)
        return Gdk.MemoryTexture.new(
            frames + 1, bins,
            Gdk.MemoryFormat.R8G8B8A8,
            gbytes,
            (frames + 1) * 4,
        )

    def do_snapshot(self, snapshot):
        w = self.get_width()
        h = self.get_height()
        if w <= 0 or h <= 0:
            return
        frames = self._mel_buf.shape[0]
        col_w = w / frames
        offset_x = -self._sub * col_w
        ext_w = w + col_w
        clip_rect = Graphene.Rect().init(0.0, 0.0, float(w), float(h))
        draw_rect = Graphene.Rect().init(offset_x, 0.0, float(ext_w), float(h))
        texture = self._build_texture()
        snapshot.push_clip(clip_rect)
        if hasattr(snapshot, 'append_scaled_texture'):
            snapshot.append_scaled_texture(
                texture, Gsk.ScalingFilter.LINEAR, draw_rect)
        else:
            snapshot.append_texture(texture, draw_rect)
        snapshot.pop()


class CurveStripArea(Gtk.Widget):
    """Strip plot rendered with GSK render nodes (GPU path).

    All drawing goes through native ``Gsk`` nodes so the GL/Vulkan
    renderer handles compositing on the GPU:

    * background & grid -> ``snapshot.append_color()``
    * curves            -> ``snapshot.append_stroke()`` with ``Gsk.Path``
    * tick / title text -> ``snapshot.append_layout()``

    Requires GTK 4.14+ (for ``Gsk.PathBuilder`` and
    ``Gtk.Snapshot.append_stroke``).
    """

    def __init__(self, get_data, *, ymin=None, ymax=None, autoscale=False,
                 lines=(), background=(0.10, 0.10, 0.12),
                 yticks=None, label=None, height_request=80):
        super().__init__()
        self._get_data = get_data
        self._ymin = ymin
        self._ymax = ymax
        self._autoscale = autoscale
        self._lines = lines
        self._bg = self._mk_rgba((background[0], background[1], background[2], 1.0))
        self._grid_color = self._mk_rgba((0.4, 0.4, 0.45, 0.6))
        self._text_color = self._mk_rgba((0.85, 0.85, 0.85, 0.9))
        self._line_colors = [self._mk_rgba(rgba) for _, rgba in lines]
        self._stroke = Gsk.Stroke.new(1.5)
        self._stroke.set_line_cap(Gsk.LineCap.ROUND)
        self._stroke.set_line_join(Gsk.LineJoin.ROUND)
        self._yticks = yticks
        self._label = label
        self._tick_font = Pango.FontDescription.from_string("sans-serif 9")
        self._title_font = Pango.FontDescription.from_string("sans-serif 10")
        self._tick_layouts = None  # built lazily once we have a Pango context
        self._title_layout = None
        self._sub = 0.0
        self.set_hexpand(True)
        self.set_size_request(-1, height_request)

    def set_sub_offset(self, sub):
        if sub < 0.0:
            sub = 0.0
        elif sub > 1.0:
            sub = 1.0
        if sub != self._sub:
            self._sub = sub
            self.queue_draw()

    @staticmethod
    def _mk_rgba(t):
        c = Gdk.RGBA()
        c.red, c.green, c.blue, c.alpha = (
            float(t[0]), float(t[1]), float(t[2]), float(t[3]))
        return c

    def _ensure_layouts(self):
        if self._tick_layouts is not None:
            return
        pctx = self.get_pango_context()
        self._tick_layouts = []
        if self._yticks:
            for yv, txt in self._yticks:
                layout = Pango.Layout(pctx)
                layout.set_font_description(self._tick_font)
                layout.set_text(str(txt), -1)
                self._tick_layouts.append((yv, layout))
        if self._label:
            self._title_layout = Pango.Layout(pctx)
            self._title_layout.set_font_description(self._title_font)
            self._title_layout.set_text(self._label, -1)

    def _yrange(self, arrays):
        if not self._autoscale:
            return self._ymin, self._ymax
        vmin = min(float(a.min()) for a in arrays if a.size)
        vmax = max(float(a.max()) for a in arrays if a.size)
        if vmax - vmin < 1e-6:
            vmax = vmin + 1.0
        margin = 0.05 * (vmax - vmin)
        return vmin - margin, vmax + margin

    @staticmethod
    def _build_path(arr, x_scale, h, ymin, yspan):
        """Build a stroke path covering [0, n*x_scale] (one extra sample held)."""
        builder = Gsk.PathBuilder.new()
        n = arr.shape[0]
        ys = h - (arr - ymin) / yspan * h
        builder.move_to(0.0, float(ys[0]))
        for i in range(1, n):
            builder.line_to(float(i * x_scale), float(ys[i]))
        # Hold last value for one extra column so the slide-in area is filled.
        builder.line_to(float(n * x_scale), float(ys[-1]))
        return builder.to_path()

    def do_snapshot(self, snapshot):
        w = self.get_width()
        h = self.get_height()
        if w <= 0 or h <= 0:
            return

        bg_rect = Graphene.Rect().init(0.0, 0.0, float(w), float(h))
        snapshot.append_color(self._bg, bg_rect)

        data = self._get_data()
        arrays = [data[k] for k, _ in self._lines]
        if not arrays or arrays[0].size == 0:
            return
        n = arrays[0].size
        ymin, ymax = self._yrange(arrays)
        yspan = (ymax - ymin) or 1.0
        x_scale = w / max(n - 1, 1)
        offset_x = -self._sub * x_scale

        # Y grid lines as 1px coloured rectangles (GSK color nodes).
        if self._yticks:
            for yv, _ in self._yticks:
                py = h - (yv - ymin) / yspan * h
                grid_rect = Graphene.Rect().init(0.0, py - 0.5, float(w), 1.0)
                snapshot.append_color(self._grid_color, grid_rect)

        # Curves -> GSK stroke nodes (GPU rasterised), translated for smooth scroll.
        clip_rect = Graphene.Rect().init(0.0, 0.0, float(w), float(h))
        snapshot.push_clip(clip_rect)
        snapshot.save()
        snapshot.translate(Graphene.Point().init(offset_x, 0.0))
        for (key, _), color in zip(self._lines, self._line_colors):
            path = self._build_path(data[key], x_scale, h, ymin, yspan)
            snapshot.append_stroke(path, self._stroke, color)
        snapshot.restore()
        snapshot.pop()

        # Text -> Pango layouts via append_layout (no scroll offset).
        self._ensure_layouts()
        if self._tick_layouts:
            for yv, layout in self._tick_layouts:
                py = h - (yv - ymin) / yspan * h
                _, ext = layout.get_pixel_extents()
                ty = max(0.0, min(float(h) - ext.height, py - ext.height - 1.0))
                snapshot.save()
                snapshot.translate(Graphene.Point().init(4.0, ty))
                snapshot.append_layout(layout, self._text_color)
                snapshot.restore()

        if self._title_layout is not None:
            _, ext = self._title_layout.get_pixel_extents()
            snapshot.save()
            snapshot.translate(
                Graphene.Point().init(float(w) - ext.width - 6.0, 2.0))
            snapshot.append_layout(self._title_layout, self._text_color)
            snapshot.restore()


class MelViewer:
    """Top-level controller: capture process + GTK window."""

    def __init__(self, app, card, device, width):
        self.app = app
        self.width_frames = width

        # Ring buffer stored as (frames, mel_bins) for cheap shifts along axis 0.
        self.mel_buf = np.zeros((width, SOF_NUM_MEL), dtype=np.float32)
        self.vad_buf = np.zeros(width, dtype=np.float32)
        self.energy_buf = np.zeros(width, dtype=np.float32)
        self.noise_buf = np.zeros(width, dtype=np.float32)

        self.recv_frames = 0
        self.prev_speech = None
        self._closing = False
        self._last_frame_mono = None       # timestamp of most recent ingest
        self._frame_period = SOF_HOP_SEC   # nominal 10 ms per hop

        self._build_ui()

        # Start capture
        self.proc = start_crecord(card, device)
        self.frame_q = queue.Queue()
        self.reader = threading.Thread(target=self._reader_thread, daemon=True)
        self.reader.start()

        # Vsync-aligned redraws via the frame clock — fires once per monitor
        # refresh while the widget is mapped. Falls back to a periodic timer
        # if add_tick_callback isn't available for some reason.
        self._tick_id = 0
        self._timer_id = 0
        if hasattr(self.mel_area, 'add_tick_callback'):
            self._tick_id = self.mel_area.add_tick_callback(self._on_tick_clock)
        else:
            self._timer_id = GLib.timeout_add(GUI_REFRESH_MS, self._on_timer)

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
        self.window.set_title("SOF Mel Spectrogram (compress PCM) — DSP VAD")
        self.window.set_default_size(1100, 600)
        self.window.connect('close-request', self._on_close)

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        box.set_margin_start(4)
        box.set_margin_end(4)
        box.set_margin_top(4)
        box.set_margin_bottom(4)
        self.window.set_child(box)

        self.mel_area = MelSpectrogramArea(self.mel_buf, MEL_VMIN, MEL_VMAX)
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

        # Vertical stretch: spectrogram dominates.
        self.mel_area.set_vexpand(True)
        box.append(self.mel_area)
        box.append(self.vad_area)
        box.append(self.level_area)

    # --- I/O thread ---------------------------------------------------
    def _reader_thread(self):
        buf = bytearray()
        raw_fd = self.proc.stdout.fileno()
        try:
            while True:
                data = os.read(raw_fd, SOF_FRAME_BYTES * 4)
                if not data:
                    break
                buf.extend(data)
                while True:
                    fn, vf, en, no, mi = parse_frame(buf)
                    if mi is None:
                        break
                    self.frame_q.put((fn, vf, en, no, mi))
        except OSError:
            pass
        self.frame_q.put(None)

    # --- GUI tick -----------------------------------------------------
    def _drain_and_redraw(self):
        """Drain queued frames, update sub-pixel offset, queue redraws.

        Returns True while running, False when EOF.
        """
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
            fn, vf, en, no, mi = item
            self._ingest(fn, vf, en, no, mi)
            drained += 1
            if drained >= 64:
                break

        # Sub-pixel scroll position: time since last ingest as a fraction of
        # the nominal frame period, clamped to [0, 1] so we never overshoot.
        if self._last_frame_mono is None:
            sub = 0.0
        else:
            sub = (time.monotonic() - self._last_frame_mono) / self._frame_period
            if sub < 0.0:
                sub = 0.0
            elif sub > 1.0:
                sub = 1.0
        self.mel_area.set_sub_offset(sub)
        self.vad_area.set_sub_offset(sub)
        self.level_area.set_sub_offset(sub)

        # If no new data arrived, still queue a redraw so the sub-pixel
        # translate advances each vsync. set_sub_offset only does it on
        # a change; force it when sub is at 1.0 (no-op) and frames were 0.
        if drained == 0 and sub >= 1.0:
            # We've slid the maximum distance; nothing to redraw.
            pass

        if eof:
            self._on_eof()
            return False
        return True

    def _on_tick_clock(self, _widget, _frame_clock):
        return True if self._drain_and_redraw() else False

    def _on_timer(self):
        return GLib.SOURCE_CONTINUE if self._drain_and_redraw() \
            else GLib.SOURCE_REMOVE

    def _ingest(self, frame_number, vad_flag, energy, noise, mel_ints):
        self.recv_frames += 1
        self._last_frame_mono = time.monotonic()
        mel = decode_mel_frame(mel_ints)
        speech = vad_flag != 0

        if speech != self.prev_speech:
            t = frame_number * SOF_HOP_SEC
            tag = "SPEECH" if speech else "SILENCE"
            print(f"  [{t:7.2f}s] {tag} (hop {frame_number})", flush=True)
        self.prev_speech = speech

        # Scroll left and append new frame on the right.
        self.mel_buf[:-1] = self.mel_buf[1:]
        self.mel_buf[-1] = mel
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
            self.mel_area.remove_tick_callback(self._tick_id)
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
        return False  # allow the window to close


def main():
    parser = argparse.ArgumentParser(
        description="Live scrolling mel spectrogram viewer "
                    "from SOF compress PCM capture (GTK 4 / GSK)")
    parser.add_argument('--card', '-c', type=int, default=0,
                        help='ALSA card number (default: 0)')
    parser.add_argument('--device', '-d', type=int, default=54,
                        help='ALSA compress device number (default: 54)')
    parser.add_argument('--width', '-w', type=int, default=SPECTROGRAM_WIDTH,
                        help=f'Spectrogram width in frames (default: {SPECTROGRAM_WIDTH})')
    args = parser.parse_args()

    print("=== SOF Mel Spectrogram Viewer (Compress PCM, GTK 4) ===\n")

    # Let Python receive Ctrl-C while the GLib loop is running.
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = Gtk.Application(application_id='org.sof.mel_spectrogram')
    holder = {}

    def on_activate(application):
        viewer = MelViewer(application, args.card, args.device, args.width)
        viewer.window.present()
        holder['viewer'] = viewer

    app.connect('activate', on_activate)
    sys.exit(app.run(None))


if __name__ == '__main__':
    main()
