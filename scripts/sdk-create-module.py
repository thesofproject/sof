#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Creates new module based on template module.

import os
import sys
import shutil
import uuid
import re

def process_directory(directory_path, old_text, new_text):
    """
    Recursively walks through a directory to rename files and replace content.
    This function processes files first, then directories, to allow for renaming
    of the directories themselves after their contents are handled.
    """
    # Walk through the directory tree
    for dirpath, dirnames, filenames in os.walk(directory_path, topdown=False):
        # --- Process Files ---
        for filename in filenames:
            original_filepath = os.path.join(dirpath, filename)

            # a) Replace content within files
            # Only process C/H files for content replacement
            if filename.endswith(('.c', '.h', '.cpp', '.hpp', '.txt', '.toml', 'Kconfig')):
                replace_text_in_file(original_filepath, old_text, new_text)
            if filename.endswith(('.c', '.h', '.cpp', '.hpp', '.txt', '.toml', 'Kconfig')):
                replace_text_in_file(original_filepath, old_text.upper(), new_text.upper())
            if filename.endswith(('.c', '.h', '.cpp', '.hpp', '.txt', '.toml', 'Kconfig')):
                replace_text_in_file(original_filepath, "template", new_text)
            if filename.endswith(('.c', '.h', '.cpp', '.hpp', '.txt', '.toml', 'Kconfig')):
                replace_text_in_file(original_filepath, "TEMPLATE", new_text.upper())

            # b) Rename the file if its name contains the template text
            if "template" in filename:
                new_filename = filename.replace("template", new_text)
                new_filepath = os.path.join(dirpath, new_filename)
                print(f"  -> Renaming file: '{original_filepath}' to '{new_filepath}'")
                os.rename(original_filepath, new_filepath)

def replace_text_in_file(filepath, old_text, new_text):
    """
    Replaces all occurrences of old_text with new_text in a given file.
    """
    try:
        # Read the file content
        with open(filepath, 'r', encoding='utf-8') as file:
            content = file.read()

        # Perform the replacement
        new_content = content.replace(old_text, new_text)

        # If content has changed, write it back
        if new_content != content:
            print(f"  -> Updating content in: '{filepath}'")
            with open(filepath, 'w', encoding='utf-8') as file:
                file.write(new_content)

    except Exception as e:
        print(f"  -> [WARNING] Could not process file '{filepath}'. Reason: {e}")

def insert_uuid_name(filepath: str, new_name: str):
    """
    Inserts a newly generated UUID and a given name into a file, maintaining
    alphabetical order by name. Ignores and preserves lines starting with '#'.

    Args:
        filepath (str): The path to the file to be updated.
        new_name (str): The name to associate with the new UUID.
    """
    data_entries = []
    other_lines = []  # To store comments and blank lines

    # --- 1. Read existing entries and comments from the file ---
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            for line in f:
                stripped_line = line.strip()
                # Check for comments or blank lines
                if not stripped_line or stripped_line.startswith('#'):
                    other_lines.append(line)  # Preserve the original line with newline
                    continue

                # It's a data line, so process it
                # Split only on the first space to handle names that might contain spaces
                parts = stripped_line.split(' ', 1)
                if len(parts) == 2:
                    data_entries.append((parts[0], parts[1]))
    except FileNotFoundError:
        print(f"File '{filepath}' not found. A new file will be created.")
    except Exception as e:
        print(f"An error occurred while reading the file: {e}")
        return

    # --- 2. Check if the name already exists in data entries ---
    if any(name == new_name for _, name in data_entries):
        print(f"Name '{new_name}' already exists in the file. No changes made.")
        return

    # --- 3. Add the new entry to the data list ---
    new_uuid = str(uuid.uuid4())

    # We split the string from the right at the last hyphen and then join the parts.
    parts = new_uuid.rsplit('-', 1)
    custom_format_uuid = ''.join(parts)

    data_entries.append((custom_format_uuid, new_name))
    print(f"Generated new entry: {new_uuid} {new_name}")

    # --- 4. Sort the list of data entries by name (the second element of the tuple) ---
    data_entries.sort(key=lambda item: item[1])

    # --- 5. Write the comments and then the sorted data back to the file ---
    try:
        with open(filepath, 'w', encoding='utf-8') as f:
            # Write all the comments and other non-data lines first
            for line in other_lines:
                f.write(line)

            # Write the sorted data entries
            for entry_uuid, entry_name in data_entries:
                f.write(f"{entry_uuid} {entry_name}\n")
        print(f"Successfully updated '{filepath}'.")
    except Exception as e:
        print(f"An error occurred while writing to the file: {e}")

# Define the markers for the managed block in the CMakeLists.txt file.
CMAKE_START_MARKER = "	# directories and files included conditionally (alphabetical order)"
CMAKE_END_MARKER = "	# end of directories and files included conditionally (alphabetical order)"

def insert_cmake_rule(filepath: str, kconfig_option: str, subdir_name: str):
    """
    Reads a CMakeLists.txt file, adds a new rule, and writes it back.

    Args:
        filepath (str): Path to the CMakeLists.txt file.
        kconfig_option (str): The Kconfig flag to check.
        subdir_name (str): The subdirectory to add.
    """
    if not os.path.exists(filepath):
        print(f"[ERROR] File not found at: '{filepath}'")
        return

    # --- 1. Read the entire file content ---
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # --- 2. Find the start and end of the managed block ---
    try:
        start_index = lines.index(CMAKE_START_MARKER + '\n')
        end_index = lines.index(CMAKE_END_MARKER + '\n')
    except ValueError:
        print(f"[ERROR] Could not find the required marker blocks in '{filepath}'.")
        print(f"Please ensure the file contains both '{CMAKE_START_MARKER}' and '{CMAKE_END_MARKER}'.")
        return

    # --- 3. Extract the lines before, during, and after the block ---
    lines_before = lines[:start_index + 1]
    block_lines = lines[start_index + 1 : end_index]
    lines_after = lines[end_index:]

    # --- 4. Parse the existing rules within the block ---
    rules = []
    # Regex to find: if(CONFIG_NAME) ... add_subdirectory(subdir) ... endif()
    # This is robust against extra whitespace and blank lines.
    block_content = "".join(block_lines)
    pattern = re.compile(
        r"if\s*\((?P<kconfig>CONFIG_[A-Z0-9_]+)\)\s*"
        r"add_subdirectory\s*\((?P<subdir>[a-zA-Z0-9_]+)\)\s*"
        r"endif\(\)",
        re.DOTALL
    )

    for match in pattern.finditer(block_content):
        rules.append(match.groupdict())

    # --- 5. Check if the rule already exists ---
    if any(rule['kconfig'] == kconfig_option for rule in rules):
        print(f"[INFO] Rule for '{kconfig_option}' already exists. No changes made.")
        return

    # --- 6. Add the new rule and sort alphabetically ---
    rules.append({'kconfig': kconfig_option, 'subdir': subdir_name})
    rules.sort(key=lambda r: r['kconfig'])
    print(f"Adding rule for '{kconfig_option}' -> '{subdir_name}' and re-sorting.")

    # --- 7. Rebuild the block content from the sorted rules ---
    new_block_lines = []
    for i, rule in enumerate(rules):
        new_block_lines.append(f"\tif({rule['kconfig']})\n")
        new_block_lines.append(f"\t\tadd_subdirectory({rule['subdir']})\n")
        new_block_lines.append("\tendif()\n")

    # --- 8. Assemble the new file content and write it back ---
    new_content = "".join(lines_before + new_block_lines + lines_after)
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"Successfully updated '{filepath}'.")

# Define the markers for the managed block in the Kconfig file.
KCONFIG_START_MARKER = "# --- Kconfig Sources (alphabetical order) ---"
KCONFIG_END_MARKER = "# --- End Kconfig Sources (alphabetical order) ---"

def insert_kconfig_source(filepath: str, source_path: str):
    """
    Reads a Kconfig file, adds a new rsource rule, and writes it back.

    Args:
        filepath (str): Path to the Kconfig file.
        source_path (str): The path to the Kconfig file to be sourced.
    """
    if not os.path.exists(filepath):
        print(f"[ERROR] File not found at: '{filepath}'")
        return

    # --- 1. Read the entire file content ---
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"[ERROR] Could not read file: {e}")
        return

    # --- 2. Find the start and end of the managed block ---
    try:
        start_index = lines.index(KCONFIG_START_MARKER + '\n')
        end_index = lines.index(KCONFIG_END_MARKER + '\n')
    except ValueError:
        print(f"[ERROR] Could not find the required marker blocks in '{filepath}'.")
        print(f"Please ensure the file contains both '{KCONFIG_START_MARKER}' and '{KCONFIG_END_MARKER}'.")
        return

    # --- 3. Extract the lines before, during, and after the block ---
    lines_before = lines[:start_index + 1]
    block_lines = lines[start_index + 1 : end_index]
    lines_after = lines[end_index:]

    # --- 4. Parse the existing rsource rules within the block ---
    source_paths = []
    # Regex to find: rsource "path/to/file"
    pattern = re.compile(r'rsource\s+"(?P<path>.*?)"')

    for line in block_lines:
        match = pattern.search(line)
        if match:
            source_paths.append(match.group('path'))

    # --- 5. Check if the source path already exists ---
    if source_path in source_paths:
        print(f"[INFO] Source path '{source_path}' already exists. No changes made.")
        return

    # --- 6. Add the new path and sort alphabetically ---
    source_paths.append(source_path)
    source_paths.sort()
    print(f"Adding source '{source_path}' and re-sorting.")

    # --- 7. Rebuild the block content from the sorted paths ---
    new_block_lines = []
    for path in source_paths:
        new_block_lines.append(f'rsource "{path}"\n')

    # --- 8. Assemble the new file content and write it back ---
    new_content = "".join(lines_before + new_block_lines + lines_after)
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"Successfully updated '{filepath}'.")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Define the markers for the managed block in the header file.
HEADER_START_MARKER = "/* Start of modules in alphabetical order */"
HEADER_END_MARKER = "/* End of modules in alphabetical order */"

def insert_c_declaration(header_path: str, name: str) -> bool:
    """
    Inserts a C function declaration alphabetically into a header file between
    start and end markers.

    The function declaration format is:
    "void sys_comp_module_${name}_interface_init(void);"

    Args:
        header_path (str): The full path to the C header file.
        name (str): The component name to be inserted into the function declaration.
        HEADER_START_MARKER (str): The string marking the beginning of the declaration block.
        HEADER_END_MARKER (str): The string marking the end of the declaration block.

    Returns:
        bool: True if the file was modified or already up-to-date, False on error.
    """
    new_declaration = f"void sys_comp_module_{name}_interface_init(void);"
    line_to_insert = new_declaration + "\n"

    # --- 1. Read existing lines from the header file ---
    try:
        with open(header_path, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: Header file not found at '{header_path}'", file=sys.stderr)
        return False
    except IOError as e:
        print(f"Error: Could not read file '{header_path}': {e}", file=sys.stderr)
        return False

    # --- 2. Find start and end markers ---
    start_index = -1
    end_index = -1
    for i, line in enumerate(lines):
        if HEADER_START_MARKER in line:
            start_index = i
        elif HEADER_END_MARKER in line:
            end_index = i
            break  # Assume end marker always comes after the start marker

    if start_index == -1 or end_index == -1:
        print(f"Error: Start ('{HEADER_START_MARKER}') or end ('{HEADER_END_MARKER}') marker not found.", file=sys.stderr)
        return False
    if end_index <= start_index:
        print(f"Error: End marker appears before start marker.", file=sys.stderr)
        return False

    # --- 3. Check if the declaration already exists within the block ---
    # Isolate the block of lines where declarations are allowed
    declaration_block = lines[start_index + 1 : end_index]
    if line_to_insert in declaration_block:
        print(f"Declaration for '{name}' already exists within the block. No changes made.")
        return True

    # --- 4. Find the correct alphabetical insertion point within the block ---
    relative_insert_index = len(declaration_block)
    for i, line in enumerate(declaration_block):
        # Compare with the stripped content of each line in the block
        if line.strip() > new_declaration:
            relative_insert_index = i
            break

    # Calculate the final insertion index relative to the whole file
    final_insert_index = start_index + 1 + relative_insert_index

    # --- 5. Insert the new line into the main list of lines ---
    lines.insert(final_insert_index, line_to_insert)
    print(f"Inserting declaration for '{name}' at line {final_insert_index + 1}.")

    # --- 6. Write the modified list back to the file ---
    try:
        with open(header_path, 'w') as f:
            f.writelines(lines)
    except IOError as e:
        print(f"Error: Could not write to file '{header_path}': {e}", file=sys.stderr)
        return False

    return True

def main():
    """
    Main function to drive the script logic.
    """
    print("--- SOF SDK New Module Creator ---")

    # Argument Validation ---
    if len(sys.argv) != 2:
        print("\n[ERROR] Invalid number of arguments.")
        print("Usage: sdk_create_module.py <new_module_name>")
        sys.exit(1)

    # Configuration --- paths are with respect to script dir
    modules_root =  SCRIPT_DIR + "/../src/audio"
    template_name = "template"
    new_module_name = sys.argv[1]
    uuid_file = SCRIPT_DIR + "/../uuid-registry.txt"
    cmake_file = SCRIPT_DIR + "/../src/audio/CMakeLists.txt"
    kconfig_file = SCRIPT_DIR + "/../src/audio/Kconfig"
    component_file = SCRIPT_DIR + "/../src/include/sof/audio/component.h"

    template_dir = os.path.join(modules_root, template_name)
    new_module_dir = os.path.join(modules_root, new_module_name)

    print(f"\nConfiguration:")
    print(f"  - Modules Root:   '{modules_root}'")
    print(f"  - Template Name:  '{template_name}'")
    print(f"  - New Module Name:'{new_module_name}'")
    print(f"  - UUID file:      '{uuid_file}'")
    print(f"  - Cmake file:    '{cmake_file}'")
    print(f"  - Kconfig file:  '{kconfig_file}'")
    print(f"  - Component file: '{component_file}'")

    # Check for Pre-existing Directories ---
    if not os.path.isdir(template_dir):
        print(f"\n[ERROR] Template directory not found at: '{template_dir}'")
        sys.exit(1)

    if os.path.exists(new_module_dir):
        print(f"\n[ERROR] A directory with the new module name already exists: '{new_module_dir}'")
        sys.exit(1)

    # Copy Template Directory ---
    try:
        print(f"\n[1/6] Copying template directory...")
        shutil.copytree(template_dir, new_module_dir)
        print(f"  -> Successfully copied to '{new_module_dir}'")
    except OSError as e:
        print(f"\n[ERROR] Could not copy directory. Reason: {e}")
        sys.exit(1)

    # Rename Files and Replace Content ---
    # We walk through the newly created directory.
    print(f"\n[2/6] Renaming files and replacing content...")
    process_directory(new_module_dir, template_name, new_module_name)
    insert_c_declaration(component_file, new_module_name)

    # Generate UUID for the new module ---
    print("\n[3/6] Generating UUID for module...")
    insert_uuid_name(uuid_file, new_module_name)

    # Add CMake rule for new module ---
    print("\n[4/6] Module creation process finished successfully!")
    kconfig_option = f"CONFIG_COMP_{new_module_name.upper()}"
    print(f"  -> Adding CMake rule for '{kconfig_option}'")
    insert_cmake_rule(cmake_file, kconfig_option, new_module_name)

    # Add Kconfig rsource for new module
    print("\n[5/6] Module creation process finished successfully!")
    insert_kconfig_source(kconfig_file, f"{new_module_name}/Kconfig")

    print("\n[6/6] Module creation process finished successfully!")
    print("--- Done ---")

if __name__ == "__main__":
    main()

