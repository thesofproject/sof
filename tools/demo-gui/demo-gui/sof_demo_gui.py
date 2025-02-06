# SPDX-License-Identifier: BSD-3-Clause

import gi
import os
import argparse
import sof_controller_engine as sof_ctl
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk

class MyWindow(Gtk.Window):
    def __init__(self, audio_paths, config_paths):
        super().__init__(title="SOF Demo Gui")
        self.set_resizable(False)
        self.set_border_width(10)
        self.apply_css()
        self.init_ui(audio_paths, config_paths)
        sof_ctl.initialize_device()
        self.is_muted = False
        self.previous_volume = 50  # Store the previous volume level

    def apply_css(self):
        css_provider = Gtk.CssProvider()
        css = """
        * {
            font-family: "Segoe UI", "Arial", sans-serif;
            font-size: 14px;
            color: #FFFFFF;
        }
        window {
            background-color: #2C3E50;
        }
        frame {
            border: 1px solid #34495E;
            border-radius: 5px;
            padding: 10px;
            margin: 5px;
            background-color: #34495E;
        }
        button, togglebutton {
            background-color: #3b3b3b;
            background: #3b3b3b;
            border: none;
            padding: 10px;
            color: #3b3b3b;
        }
        button:hover, togglebutton:hover {
            background-color: #A9A9A9;
            color: #A9A9A9;
        }
        togglebutton:checked {
            background-color: #A9A9A9;
            color: #A9A9A9;
        }
        scale {
            background-color: #34495E;
            border-radius: 5px;
        }
        label {
            color: #A9A9A9;
        }
        headerbar {
            background-color: #2C3E50;
            color: #2C3E50;
        }
        headerbar.titlebar {
            background: #2C3E50;
        }
        """
        css_provider.load_from_data(css.encode('utf-8'))
        screen = Gdk.Screen.get_default()
        Gtk.StyleContext.add_provider_for_screen(screen, css_provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

    def init_ui(self, audio_paths, config_paths):
        main_grid = Gtk.Grid()
        main_grid.set_row_spacing(10)
        main_grid.set_column_spacing(10)
        self.add(main_grid)

        control_frame = self.create_control_frame()
        main_grid.attach(control_frame, 0, 0, 1, 1)

        file_frame = self.create_file_frame()
        main_grid.attach(file_frame, 0, 1, 1, 1)

        record_frame = self.create_record_frame()
        main_grid.attach(record_frame, 0, 2, 1, 1)

        self.scan_and_populate_dropdowns(audio_paths, config_paths)

    def create_control_frame(self):
        control_frame = Gtk.Frame(label="Playback and Volume Control")
        control_grid = Gtk.Grid()
        control_grid.set_row_spacing(10)
        control_grid.set_column_spacing(10)
        control_frame.add(control_grid)

        volume_adjustment = Gtk.Adjustment(value=100, lower=0, upper=100, step_increment=1, page_increment=5, page_size=0)
        self.volume_button = Gtk.Scale(name="volume", orientation=Gtk.Orientation.HORIZONTAL, adjustment=volume_adjustment)
        self.volume_button.set_digits(0)
        self.volume_button.set_hexpand(True)
        self.volume_button.connect("value-changed", self.on_volume_changed)
        control_grid.attach(self.volume_button, 0, 0, 2, 1)

        self.play_pause_button = Gtk.ToggleButton(label="Play")
        self.play_pause_button.connect("toggled", self.on_play_pause_toggled)
        control_grid.attach(self.play_pause_button, 0, 1, 1, 1)

        self.stop_button = Gtk.Button(label="Stop")
        self.stop_button.connect("clicked", self.on_stop_clicked)
        control_grid.attach(self.stop_button, 1, 1, 1, 1)

        self.mute_button = Gtk.Button(label="Mute")
        self.mute_button.connect("clicked", self.on_mute_clicked)
        control_grid.attach(self.mute_button, 0, 2, 2, 1)

        return control_frame

    def create_file_frame(self):
        file_frame = Gtk.Frame(label="File Selection")
        file_grid = Gtk.Grid()
        file_grid.set_row_spacing(10)
        file_grid.set_column_spacing(10)
        file_frame.add(file_grid)

        self.wav_dropdown = Gtk.ComboBoxText()
        self.wav_dropdown.connect("changed", self.on_wav_file_selected)
        wav_label = Gtk.Label(label="Select WAV File")
        wav_label.set_margin_top(5)
        wav_label.set_margin_bottom(5)
        file_grid.attach(wav_label, 0, 0, 1, 1)
        file_grid.attach(self.wav_dropdown, 1, 0, 1, 1)

        self.eq_dropdown = Gtk.ComboBoxText()
        eq_label = Gtk.Label(label="Select EQ Config")
        eq_label.set_margin_top(5)
        eq_label.set_margin_bottom(5)
        file_grid.attach(eq_label, 0, 1, 1, 1)
        file_grid.attach(self.eq_dropdown, 1, 1, 1, 1)

        self.apply_eq_button = Gtk.Button(label="Apply EQ Config")
        self.apply_eq_button.connect("clicked", self.on_apply_eq_clicked)
        file_grid.attach(self.apply_eq_button, 1, 2, 1, 1)

        return file_frame

    def create_record_frame(self):
        record_frame = Gtk.Frame(label="Recording Control")
        record_grid = Gtk.Grid()
        record_grid.set_row_spacing(10)
        record_grid.set_column_spacing(10)
        record_grid.set_hexpand(True)
        record_grid.set_vexpand(True)
        record_frame.add(record_grid)

        self.record_button = Gtk.ToggleButton(label="Record")
        self.record_button.connect("toggled", self.on_record_toggled)
        self.record_button.set_hexpand(True)
        self.record_button.set_vexpand(True)
        record_grid.attach(self.record_button, 0, 0, 1, 1)

        self.record_index = 1

        return record_frame

    def scan_and_populate_dropdowns(self, audio_paths, config_paths):
        wav_files = sof_ctl.scan_for_files('audios', '.wav', extra_paths=audio_paths)

        self.wav_dropdown.remove_all()
        for wav_file in wav_files:
            self.wav_dropdown.append_text(wav_file)

        if wav_files:
            self.wav_dropdown.set_active(0)
            self.selected_wav_file = wav_files[0]

        eq_files = sof_ctl.scan_for_files('eq_configs', '.txt', extra_paths=config_paths)

        self.eq_dropdown.remove_all()
        for eq_file in eq_files:
            self.eq_dropdown.append_text(eq_file)

        if eq_files:
            self.eq_dropdown.set_active(0)
            self.selected_eq_file = eq_files[0]

    def on_volume_changed(self, widget):
        user_volume = widget.get_value()
        scaled_volume = sof_ctl.scale_volume(user_volume)
        sof_ctl.execute_command(command="volume", data=scaled_volume)

    def on_play_pause_toggled(self, widget):
        if widget.get_active():
            widget.set_label("Pause")
            sof_ctl.execute_command(command="play", file_name=self.selected_wav_file)
        else:
            widget.set_label("Play")
            sof_ctl.execute_command(command="pause")

    def on_stop_clicked(self, widget):
        self.play_pause_button.set_active(False)
        sof_ctl.execute_command(command="stop")

    def on_mute_clicked(self, widget):
        if not self.is_muted:
            self.previous_volume = self.volume_button.get_value()
            self.volume_button.set_value(0)
            self.is_muted = True
            widget.set_label("Unmute")
        else:
            self.volume_button.set_value(self.previous_volume)
            self.is_muted = False
            widget.set_label("Mute")

    def on_record_toggled(self, widget):
        if widget.get_active():
            widget.set_label("Stop Recording")
            filename = self.generate_sequential_filename()
            sof_ctl.execute_command(command="record", data=True, file_name=filename)
        else:
            widget.set_label("Record")
            sof_ctl.execute_command(command="record", data=False)

    def generate_sequential_filename(self):
        recordings_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'recordings')

        if not os.path.exists(recordings_dir):
            os.makedirs(recordings_dir)

        filename = os.path.join(recordings_dir, f"recording_{self.record_index}.wav")
        self.record_index += 1

        return filename

    def on_wav_file_selected(self, widget):
        self.selected_wav_file = widget.get_active_text()

    def on_apply_eq_clicked(self, widget):
        self.selected_eq_file = self.eq_dropdown.get_active_text()
        if self.selected_eq_file:
            eq_file_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'eq_configs', self.selected_eq_file)
            sof_ctl.execute_command(command="eq", file_name=eq_file_path)

def main():
    parser = argparse.ArgumentParser(description="SOF GUI Application")
    parser.add_argument('--audio-path', action='append', help="Additional path to search for audio files (.wav)")
    parser.add_argument('--config-path', action='append', help="Additional path to search for EQ config files (.txt)")
    args = parser.parse_args()

    sof_ctl.extra_audio_paths = args.audio_path if args.audio_path else []

    win = MyWindow(audio_paths=args.audio_path, config_paths=args.config_path)
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()

if __name__ == "__main__":
    main()
