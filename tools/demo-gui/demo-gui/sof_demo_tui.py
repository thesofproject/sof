# SPDX-License-Identifier: BSD-3-Clause

import os
import sof_controller_engine as sof_ctl
import argparse

os.chdir(os.path.dirname(os.path.abspath(__file__)))

class TextBasedUI:
    def __init__(self, audio_paths, config_paths):
        self.selected_wav_file = None
        self.selected_eq_file = None
        self.volume = 15
        self.is_playing = False
        self.is_recording = False
        self.scan_and_populate_files(audio_paths, config_paths)
        sof_ctl.initialize_device()
        self.run_ui()

    def scan_and_populate_files(self, audio_paths, config_paths):
        self.wav_files = sof_ctl.scan_for_files('audios', '.wav', extra_paths=audio_paths)

        if self.wav_files:
            self.selected_wav_file = self.wav_files[0]
            print(f"Default WAV file selected: {self.selected_wav_file}")

        self.eq_files = sof_ctl.scan_for_files('eq_configs', '.txt', extra_paths=config_paths)

        if self.eq_files:
            self.selected_eq_file = self.eq_files[0]
            print(f"Default EQ config selected: {self.selected_eq_file}")

    def display_menu(self):
        print("\nSOF Text-Based UI")
        print("==================")
        print("1. Select a WAV file")
        if self.selected_wav_file:
            print(f"   Currently selected: {self.selected_wav_file}")
        else:
            print("   No WAV file selected")

        print("2. Select an EQ Config")
        if self.selected_eq_file:
            print(f"   Currently selected: {self.selected_eq_file}")
        else:
            print("   No EQ file selected")

        print(f"3. Set Volume (Current: {self.volume})")
        print(f"4. Play/Pause (Current: {'Playing' if self.is_playing else 'Paused'})")
        print(f"5. Start/Stop Recording (Current: {'Recording' if self.is_recording else 'Not Recording'})")
        print("6. Apply EQ Config")
        print("7. Quit")
        print("==================")

    def run_ui(self):
        while True:
            self.display_menu()
            choice = input("Enter your choice (1-7): ").strip()

            if choice == '1':
                self.select_wav_file()
            elif choice == '2':
                self.select_eq_file()
            elif choice == '3':
                self.set_volume()
            elif choice == '4':
                self.toggle_play_pause()
            elif choice == '5':
                self.toggle_record()
            elif choice == '6':
                self.apply_eq_config()
            elif choice == '7':
                print("Exiting...")
                break
            else:
                print("Invalid choice, please try again.")

    def select_wav_file(self):
        if not self.wav_files:
            print("No .wav files found in the current directory.")
            return

        print("\nSelect a WAV file:")
        for idx, wav_file in enumerate(self.wav_files):
            print(f"{idx + 1}. {wav_file}")

        file_choice = input(f"Enter the number of the file (1-{len(self.wav_files)}): ").strip()

        try:
            file_index = int(file_choice) - 1
            if 0 <= file_index < len(self.wav_files):
                self.selected_wav_file = self.wav_files[file_index]
                print(f"Selected file: {self.selected_wav_file}")
            else:
                print("Invalid selection.")
        except ValueError:
            print("Invalid input, please enter a number.")

    def select_eq_file(self):
        if not self.eq_files:
            print("No EQ config files found in the eq_configs directory.")
            return

        print("\nSelect an EQ Config file:")
        for idx, eq_file in enumerate(self.eq_files):
            print(f"{idx + 1}. {eq_file}")

        file_choice = input(f"Enter the number of the file (1-{len(self.eq_files)}): ").strip()

        try:
            file_index = int(file_choice) - 1
            if 0 <= file_index < len(self.eq_files):
                self.selected_eq_file = self.eq_files[file_index]
                print(f"Selected EQ Config: {self.selected_eq_file}")
            else:
                print("Invalid selection.")
        except ValueError:
            print("Invalid input, please enter a number.")

    def set_volume(self):
        volume_input = input("Enter volume level (0-100): ").strip()

        try:
            user_volume = int(volume_input)
            if 0 <= user_volume <= 100:
                self.volume = user_volume
                scaled_volume = sof_ctl.scale_volume(user_volume)
                sof_ctl.execute_command(command="volume", data=scaled_volume)
                print(f"Volume set to {self.volume}.")
            else:
                print("Volume must be between 0 and 100.")
        except ValueError:
            print("Invalid input, please enter a number.")

    def toggle_play_pause(self):
        if not self.selected_wav_file:
            print("No WAV file selected.")
            return

        if not self.is_playing:
            sof_ctl.execute_command(command="play", file_name=self.selected_wav_file)
            self.is_playing = True
            print("Playing...")
        else:
            sof_ctl.execute_command(command="pause")
            self.is_playing = False
            print("Paused.")

    def toggle_record(self):
        recordings_dir = os.path.join(os.getcwd(), 'recordings')

        if not self.is_recording:
            filename = input(f"Enter filename to save the recording (default: {recordings_dir}/record.wav): ").strip()
            if not filename:
                filename = "record.wav"

            full_path = os.path.join(recordings_dir, filename)
            sof_ctl.execute_command(command="record", data=True, file_name=full_path)
            self.is_recording = True
        else:
            sof_ctl.execute_command(command="record", data=False)
            self.is_recording = False

    def apply_eq_config(self):
        if not self.selected_eq_file:
            print("No EQ config file selected.")
            return

        eq_file_path = os.path.join(os.getcwd(), 'eq_configs', self.selected_eq_file)
        sof_ctl.execute_command(command="eq", file_name=eq_file_path)
        print(f"Applied EQ config: {self.selected_eq_file}")

def main():
    parser = argparse.ArgumentParser(description="SOF TUI Application")
    parser.add_argument('--audio-path', action='append', help="Additional path to search for audio files (.wav)")
    parser.add_argument('--config-path', action='append', help="Additional path to search for EQ config files (.txt)")
    args = parser.parse_args()

    sof_ctl.extra_audio_paths = args.audio_path if args.audio_path else []

    TextBasedUI(audio_paths=args.audio_path, config_paths=args.config_path)

if __name__ == "__main__":
    main()
