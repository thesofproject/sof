import os
import argparse
import curses
import subprocess

os.chdir(os.path.dirname(os.path.abspath(__file__)))

class TopologySelectorUI:
    
    def __init__(self, tplg_path):
        self.selected_tplg_file = None
        self.tplg_path = tplg_path
        self.tplg_files = self.scan_for_files('', '.tplg', extra_paths=[tplg_path])
        self.tplg_files.sort()
        if self.tplg_files:
            self.selected_tplg_file = self.tplg_files[0]
        self.run_ui()

    def scan_for_files(self, directory_name: str, file_extension: str, extra_paths: list = None):
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

    def run_ui(self):
        curses.wrapper(self.main_menu)

    def main_menu(self, stdscr):
        curses.start_color()
        self.initialize_colors()
        stdscr.bkgd(' ', curses.color_pair(2))

        current_row = 0
        max_row = 3

        while True:
            stdscr.clear()
            self.display_menu(stdscr, current_row)
            key = stdscr.getch()

            if key == curses.KEY_UP:
                current_row = (current_row - 1) % (max_row + 1)
            elif key == curses.KEY_DOWN:
                current_row = (current_row + 1) % (max_row + 1)

            actions = {
                0: lambda: self.select_tplg_file(stdscr),
                1: lambda: self.switch_topology(self.selected_tplg_file),
                2: lambda: exit()
            }

            if key == 10:
                if current_row in actions:
                    actions[current_row]()
                elif current_row == 3:
                    break

    def initialize_colors(self):
        curses.init_pair(1, curses.COLOR_BLACK, 7)
        curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLUE)

    def display_menu(self, stdscr, current_row):
        stdscr.addstr(0, 0, "---------------Topology Selector----------------", curses.A_BOLD)
        stdscr.addstr(1, 0, "================================================")

        menu = [
            f"| Select Topology (Currently: {self.selected_tplg_file})",
            "| Apply Selected Topology",
            "| Quit "
        ]

        for idx, item in enumerate(menu):
            color_pair = curses.color_pair(1) if idx == current_row else curses.color_pair(2)
            stdscr.addstr(idx + 3, 0, item, color_pair)

        stdscr.refresh()

    def select_tplg_file(self, stdscr):
        self.select_file_from_list(stdscr, self.tplg_files, "-----Select Topology-----")


    def select_file_from_list(self, stdscr, file_list, title):
        current_row = 0
        offset = 0
        max_visible_rows = curses.LINES - 2

        while True:
            stdscr.clear()
            stdscr.addstr(0, 0, title, curses.A_BOLD)
            
            visible_rows = min(max_visible_rows, len(file_list))

            for idx in range(visible_rows):
                file_idx = idx + offset
                if file_idx >= len(file_list):
                    break
                file_name = file_list[file_idx]
                color_pair = curses.color_pair(1) if file_idx == current_row else curses.color_pair(2)
                stdscr.addstr(idx + 1, 0, file_name, color_pair)

            stdscr.refresh()
            key = stdscr.getch()

            if key == curses.KEY_UP:
                if current_row > 0:
                    current_row -= 1
                if current_row < offset:
                    offset -= 1
            elif key == curses.KEY_DOWN:
                if current_row < len(file_list) - 1:
                    current_row += 1
                if current_row >= offset + visible_rows:
                    offset += 1
            elif key == curses.KEY_ENTER or key in [10, 13]:
                self.selected_tplg_file = file_list[current_row]
                self.switch_topology(self.selected_tplg_file)
                break

    def switch_topology(self, tplg_file):
        tplg_full_path = os.path.join(self.tplg_path, tplg_file)
        subprocess.run(['./switch_tplg.sh', tplg_full_path], check=True)
        print(f"Switched to topology: {tplg_full_path}")

def main():
    parser = argparse.ArgumentParser(description="Topology Selector")
    parser.add_argument('--tplg-path', default='/lib/firmware/imx/sof-tplg', help="Path to search for topology files (.tplg)")
    args = parser.parse_args()

    TopologySelectorUI(tplg_path=args.tplg_path)

if __name__ == "__main__":
    main()
