#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause


import json
import pathlib
import shutil


"""Headers for needs of lmdk are defined in
           lmdk/include/headers_list.json"""


_SOF_TOP = pathlib.Path(__file__).parents[2].resolve()
LMDK_HEADERS = _SOF_TOP / "lmdk" / "include" / "headers_manifest.json"


def str_path_from_json(record):
    """parsing json record to string"""
    src = ''
    for i in record:
        src += i
        src += "/"
    return src[:-1]


def create_separate_headers():
    f = open(LMDK_HEADERS)
    data = json.load(f)

    for i in data:
        src = str_path_from_json(i)
        p = pathlib.Path(_SOF_TOP, "lmdk", "include", "sof", src)
        p.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(_SOF_TOP / src, _SOF_TOP / "lmdk" /"include" / "sof" / src)
    f.close()

""" -> to do
def validate_separate_headers():
    return 0"""


def create_headers_pack():
    """Creates pack of lmdk headers"""
    create_separate_headers()
    shutil.make_archive(_SOF_TOP / "lmdk" /"include" / "header_pack", "zip", _SOF_TOP / "lmdk" /"include" / "sof")
    shutil.rmtree(_SOF_TOP / "lmdk" /"include" / "sof", ignore_errors=True)
    return 0


def main():
    create_headers_pack()


if __name__ == "__main__":
    main()