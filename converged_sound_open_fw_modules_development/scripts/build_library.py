# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2021 Intel Corporation. All rights reserved.

from shutil import copyfile
from pathlib import Path
import argparse
import os
import xml.etree.ElementTree as ET
import sys
import errno

# usage example
# python3 build_library.py lx7hifi4_plat aca_lib 1.0 aca_module

# Global paths
REPO_PATH = Path(__file__).absolute().parents[1]
COMMON_BINMAP_PATH = REPO_PATH / Path("FW/intel_common/module_binmaps")
INTEL_ADSP_DIR = REPO_PATH / Path("FW/src/intel_adsp/")
OUT_DIR = Path("out").absolute()
ATTR_FILE_PATH = REPO_PATH / Path("artifacts/dsp_fw_example_release.xml")
LIB_CONFIG_FILE_PATH = REPO_PATH / Path("scripts/lib_config.conf")
MOD_DEF_FILE = REPO_PATH / Path("FW/portable/include/modules.h")

# Global variables
PAGE_SIZE = 4098
MANIFEST_SIZE = 8 * PAGE_SIZE
ERROR_CODE = 1

BUILD_CONFIG = {
        "SIGNING_KEYS": "",
        "BINMAP_DIR": "",
        "FW_BUILDER_PATH": "",
        "MEU_PATH": "",
        "SIGNING_TOOL": "",
        "OVERLAY_TOOLS_DIR": "",
        "XTENSA_TOOLS_DIR": "",
}

def parse_aguments():
        parser = argparse.ArgumentParser()
        parser.add_argument("platform_name", help="name of target platform")
        parser.add_argument("library_name", help="name of output library")
        parser.add_argument("library_version", help="version of output library")
        parser.add_argument("modules_list", help="modules to be included in \
                output library (e.g. \"module_1_name, module_2_name\")")
        parser.add_argument("-o", "--output_path", default = OUT_DIR, \
                help="path to output build folder (default: \
                {})".format(OUT_DIR))
        parser.add_argument("-c", "--config", choices=["release", "debug"], \
                default="release", help="type of config (default: release)")
        parser.add_argument("-a", "--attr_file_path", default=ATTR_FILE_PATH, \
                help="path to xml attribute file (default: \
                {})".format(ATTR_FILE_PATH))
        parser.add_argument("-t", "--toolschain", \
                choices=["xtensa", "xtensa-lx7hifi4-elf"], default="xtensa-lx7hifi4-elf", \
                help="default: xtensa-lx7hifi4-elf")
        
        args = parser.parse_args()
        args.modules_list = args.modules_list.split(' ')

        # TODO: remove, for debug only
        print("platform_name: ", args.platform_name)
        print("library_name: ", args.library_name)
        print("library_version: ", args.library_version)
        print("config: ", args.config)
        print("modules_list: ", args.modules_list)
        print("toolschain: ", args.modules_list)

        return args

def create_library_makefile(args, repo_path, out_dir):
        src_makefile_path = repo_path / Path("FW/src/intel_adsp/template/loadable_library/makefile")
        out_makefile_path = out_dir / "makefile"

        copyfile(str(src_makefile_path), str(out_makefile_path))

        repo_dir = repo_path

        # create modules_properties section - it is requires in outputmakefile
        module_properties = ""
        for module in args.modules_list:
                module_properties += "define {}_PROPERTIES\n".format(module)
                module_properties += "  {}_INSTANCES_COUNT := 1\n".format(module)
                module_properties += "  {}_FILE := {}/modules/{}/build/out/{}/{}/{}.dlm.a\n".format(module, repo_dir, module, args.platform_name, args.config, module)
                module_properties += "endef\n\n"

        # open out makefile and read the content
        with open(str(out_makefile_path), "r") as out_file:
                make_content = out_file.read()

        # replace template defines
        make_content = make_content.replace("___TEMPLATE_FIELD_LIBRARY_NAME_LOWER_SNAKE_CASE___",\
                 args.library_name)
        make_content = make_content.replace("___TEMPLATE_FIELD_MODULES_LIST___", \
                " ".join(args.modules_list))
        make_content = make_content.replace("___TEMPLATE_FIELD_MODULES_MAKEFILE_SECTION___", \
                module_properties)

        # save replaced content in output makefile 
        with open(str(out_makefile_path), "w") as out_file:
                out_file.write(make_content)


def create_binamp_file(args, common_binamp_path):
        switcher = {
                "lx7hifi4_plat": "hifi4_plat",
        }

        dsp_fw_common_binmap_path = common_binamp_path / "dsp_fw_common.binmap"

        binmap_content = ""
        binmap_content += "include {}\n\n".format(dsp_fw_common_binmap_path)
        binmap_content += "binary_name {}\n".format(args.library_name)
        binmap_content += "binary_ver {}\n".format(args.library_version)
        binmap_content += "max_data 0\n\n"
        binmap_content += "define PLATFORM {}\n\n".format(switcher.get(args.platform_name))
        for module in args.modules_list:
                module_binmap_path = common_binamp_path / "{}.binmap".format(module)
                binmap_content += "include {}".format(module_binmap_path)

        output_binmap_file_path = OUT_DIR / "{}.binmap".format(args.library_name)
        
        with open(str(output_binmap_file_path), 'w') as binmap_file:
                binmap_file.write(binmap_content)

        return output_binmap_file_path

def calculate_library_address(xml_file):
       
        max_hpsram_offset = 0
        max_imr_offset = 0

        if not os.path.isfile(str(xml_file)):
                print("Provided file does not exist.")
                sys.exit(ERROR_CODE)

        f_extension = os.path.splitext(str(xml_file))[-1]
        if not f_extension == '.xml':
                print("Provided file is not a *.xml file.")
                sys.exit(ERROR_CODE)

        tree = ET.parse(str(xml_file))
        root = tree.getroot()

        for mem_class in root.iter('memory_class'):
                mem_class_name = mem_class.get('name')
                offset = int(mem_class.find('offset').text, 16)
                
                if mem_class_name == 'hp_sram':
                        if offset >= max_hpsram_offset:
                                max_hpsram_offset = offset
                elif mem_class_name == 'imr':
                        if offset >= max_imr_offset:
                                max_imr_offset = offset

        lib_base_address = max_hpsram_offset + MANIFEST_SIZE + PAGE_SIZE
        lib_base_address_lma = max_imr_offset + MANIFEST_SIZE + PAGE_SIZE

        return [lib_base_address, lib_base_address_lma]

def create_make_cmd(args, load_addr, load_addr_lma, build_config, repo_path, \
        out_dir, intel_adsp_dir, binmap_file_path, mod_def_file):
        # fetch binmap directory
        binmap_dir = Path(binmap_file_path).parent
        makefile_path = out_dir / "makefile"

        make_cmd = "make "
        make_cmd += "-f {} ".format(makefile_path)
        make_cmd += "{} ".format(args.platform_name)
        # add "/" at the end in order to fulfill make convention
        make_cmd += "INTEL_ADSP_DIR={}/ ".format(intel_adsp_dir)
        make_cmd += "SIGNING_KEYS={} ".format(build_config["SIGNING_KEYS"])
        make_cmd += "MOD_DEF_FILE={} ".format(mod_def_file)
        make_cmd += "SIGNING_TOOL={} ".format(build_config["SIGNING_TOOL"])
        # add "/" at the end in order to fulfill make convention
        make_cmd += "BINMAP_DIR={}/ ".format(binmap_dir)
        make_cmd += "MEU_PATH={} ".format(build_config["MEU_PATH"])
        make_cmd += "FW_BUILDER_PATH={} ".format(build_config["FW_BUILDER_PATH"])
        make_cmd += "LOADABLE_LIBRARY_BASE_ADDRESS={} ".format(str(hex(load_addr)))
        make_cmd += "LOADABLE_LIBRARY_BASE_ADDRESS_LMA={} ".format(str(hex(load_addr_lma)))
        make_cmd += "TOOLSCHAIN={} ".format(args.toolschain)
        if (args.toolschain == "xtensa-lx7hifi4-elf"):
                make_cmd += "OVERLAY_TOOLS_DIR={} ".format(build_config["OVERLAY_TOOLS_DIR"])
        else:
                make_cmd += "XTENSA_TOOLS_DIR={} ".format(build_config["XTENSA_TOOLS_DIR"])

        return make_cmd

def parse_config_file(config_file_path, build_config):
        with open(str(config_file_path), "r") as config_file:
                config_content = config_file.read()
        
        for line in config_content.split("\n"):
                # remove white spaces in lines
                line = line.replace(" ", "")

                # ommit comments
                if (len(line) > 0) and (line[0] == "#"):
                        continue

                # split variable and value by "="
                splitted_line = line.split("=")
                if len(splitted_line) == 2:
                        [var_name , var_value] = splitted_line

                        #set build_config value
                        for key in list(build_config.keys()):
                                if (key == var_name):
                                        build_config[key] = var_value
        
        for key in list(build_config.keys()):
                if not build_config[key]:
                        print("{} variable is not set in {} file".format(key, \
                                config_file_path))
                        sys.exit(ERROR_CODE)
        
        return build_config



def main():
        args = parse_aguments()
        print("OUT_DIR: {}".format(OUT_DIR))
        print(type(OUT_DIR))
        # create output directory
        try:
                os.mkdir(str(OUT_DIR))
        except OSError as error:
                if error.errno != errno.EEXIST:
                        raise             

        # create library makefile
        create_library_makefile(args, REPO_PATH, OUT_DIR)

        # create library binmap
        binmap_file_path = create_binamp_file(args, COMMON_BINMAP_PATH)

        # calculte library address
        [load_addr, load_addr_lma] = calculate_library_address(ATTR_FILE_PATH)

        # parse library config file - fetch tool variables
        global BUILD_CONFIG
        BUILD_CONFIG = parse_config_file(LIB_CONFIG_FILE_PATH, BUILD_CONFIG)

        make_cmd = create_make_cmd(args, load_addr, load_addr_lma, \
                BUILD_CONFIG, REPO_PATH, OUT_DIR, INTEL_ADSP_DIR, \
                binmap_file_path, MOD_DEF_FILE)
        
        # invoke make
        print(make_cmd)
        os.system(make_cmd)

if __name__ == '__main__':
        main()
