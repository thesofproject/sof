# SPDX-License-Identifier: BSD-3-Clause

import subprocess
import os
import signal
import re
import math

# Global variables to store the aplay/arecord process, paused state, current file, detected device, volume control, and EQ numid
aplay_process = None
arecord_process = None
paused = None
current_file = None
device_string = None
volume_control = None
eq_numid = None

extra_audio_paths = []

def initialize_device():
    global device_string, volume_control, eq_numid

    try:
        output = subprocess.check_output(["aplay", "-l"], text=True, stderr=subprocess.DEVNULL)

        match = re.search(r"card (\d+):.*\[.*sof.*\]", output, re.IGNORECASE)
        if match:
            card_number = match.group(1)
            device_string = f"hw:{card_number}"
            print(f"Detected SOF card: {device_string}")
        else:
            print("No SOF card found.")
            raise RuntimeError("SOF card not found. Ensure the device is connected and recognized by the system.")

        controls_output = subprocess.check_output(["amixer", f"-D{device_string}", "controls"], text=True, stderr=subprocess.DEVNULL)

        volume_match = re.search(r"numid=(\d+),iface=MIXER,name='(.*Master Playback Volume.*)'", controls_output)
        if volume_match:
            volume_control = volume_match.group(2)
            print(f"Detected Volume Control: {volume_control}")
        else:
            print("Master GUI Playback Volume control not found.")
            raise RuntimeError("Volume control not found.")

        eq_match = re.search(r"numid=(\d+),iface=MIXER,name='EQIIR1\.0 eqiir_coef_1'", controls_output)
        if eq_match:
            eq_numid = eq_match.group(1)
            print(f"Detected EQ numid: {eq_numid}")
        else:
            print("EQ control not found.")
            raise RuntimeError("EQ control not found.")

    except subprocess.CalledProcessError as e:
        print(f"Failed to run device detection commands: {e}")
        raise

def scale_volume(user_volume):
    normalized_volume = user_volume / 100.0
    scaled_volume = 31 * (math.sqrt(normalized_volume))
    return int(round(scaled_volume))

def scan_for_files(directory_name: str, file_extension: str, extra_paths: list = None):
    found_files = []
    dir_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), directory_name)

    if os.path.exists(dir_path):
        found_files.extend([f for f in os.listdir(dir_path) if f.endswith(file_extension)])
    else:
        print(f"Error: The '{directory_name}' directory is missing. It should be located in the same folder as this script.")

    if extra_paths:
        for path in extra_paths:
            if os.path.exists(path):
                found_files.extend([f for f in os.listdir(path) if f.endswith(file_extension)])
            else:
                print(f"Warning: The directory '{path}' does not exist.")

    return found_files

def execute_command(command: str, data: int = 0, file_name: str = None):
    if device_string is None or volume_control is None or eq_numid is None:
        raise RuntimeError("Device not initialized. Call initialize_device() first.")

    command_switch = {
        'volume': lambda x: handle_volume(data),
        'eq': lambda x: handle_eq(file_name),
        'play': lambda x: handle_play(file_name),
        'pause': lambda x: handle_pause(),
        'record': lambda x: handle_record(start=data, filename=file_name)
    }

    command_function = command_switch.get(command, lambda x: handle_unknown_command(data))
    command_function(data)

def handle_volume(data: int):
    amixer_command = f"amixer -D{device_string} cset name='{volume_control}' {data}"
    try:
        subprocess.run(amixer_command, shell=True, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError as e:
        print(f"Failed to set volume: {e}")

def handle_eq(eq_file_name: str):
    ctl_command = f"./sof-ctl -D{device_string} -n {eq_numid} -s {eq_file_name}"
    try:
        subprocess.run(ctl_command, shell=True, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError as e:
        print(f"Failed to apply EQ settings: {e}")

def handle_play(play_file_name: str):
    global aplay_process, paused, current_file

    if paused and paused is not None and current_file == play_file_name:
        os.kill(aplay_process.pid, signal.SIGCONT)
        print("Playback resumed.")
        paused = False
        return

    if aplay_process is not None:
        if aplay_process.poll() is None:
            if current_file == play_file_name:
                print("Playback is already in progress.")
                return
            else:
                os.kill(aplay_process.pid, signal.SIGKILL)
                print("Stopping current playback to play a new file.")
        else:
            print("Previous process is not running, starting new playback.")

    default_audio_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'audios')
    file_path = next((os.path.join(path, play_file_name) for path in [default_audio_dir] + extra_audio_paths if os.path.exists(os.path.join(path, play_file_name))), None)

    if file_path is None:
        print(f"Error: File '{play_file_name}' not found in the default 'audios' directory or any provided paths.")
        return

    aplay_command = f"aplay -D{device_string} '{file_path}'"

    try:
        aplay_process = subprocess.Popen(aplay_command, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        current_file = play_file_name
        print(f"Playing file: {play_file_name}.")
        paused = False
    except subprocess.CalledProcessError as e:
        print(f"Failed to play file: {e}")

def handle_pause():
    global aplay_process, paused

    if aplay_process is None:
        print("No playback process to pause.")
        return

    if aplay_process.poll() is not None:
        print("Playback process has already finished.")
        return

    try:
        os.kill(aplay_process.pid, signal.SIGSTOP)
        paused = True
        print("Playback paused.")
    except Exception as e:
        print(f"Failed to pause playback: {e}")

def handle_record(start: bool, filename: str):
    global arecord_process

    if start:
        if arecord_process is not None and arecord_process.poll() is None:
            print("Recording is already in progress.")
            return

        if not filename:
            print("No filename provided for recording.")
            return

        record_command = f"arecord -D{device_string} -f cd -t wav {filename}"

        try:
            arecord_process = subprocess.Popen(record_command, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print(f"Started recording: {filename}")
        except subprocess.CalledProcessError as e:
            print(f"Failed to start recording: {e}")

    else:
        if arecord_process is None or arecord_process.poll() is not None:
            print("No recording process to stop.")
            return

        try:
            os.kill(arecord_process.pid, signal.SIGINT)
            arecord_process = None
            print(f"Stopped recording.")
        except Exception as e:
            print(f"Failed to stop recording: {e}")

def handle_unknown_command(data: int):
    print(f"Unknown command: {data}")
