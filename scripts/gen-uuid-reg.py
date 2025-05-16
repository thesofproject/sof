#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
import re
import sys

# Very simple UUID registry validator and C header generator.  Parses
# uuid-registry.txt (passed as the first command line argument) and
# writes a C header (named in the second argument) containing
# definitions to be used at build time.  Fails via assertion if the
# any element in the registry is invalid.

header = """/*
 * GENERATED CODE.  DO NOT EDIT.
 *
 * Generated UUID records (initializers for struct sof_uuid)
 * See scripts/gen-uuid-reg.py
 */
#ifndef _UUID_REGISTRY_H
#define _UUID_REGISTRY_H
"""

all_syms = set()
all_uuids = set()
out_recs = []

def emit_uuid_rec(uu, sym):
    recs = uu.split('-')
    brec = recs[3]
    wrecs = [ "0x" + r for r in recs[0:3] ]
    byts = [ "0x" + brec[ 2*i : 2*i+2 ] for i in range(int(len(brec) / 2)) ]
    uuidinit = "{ " + ", ".join(wrecs) + ", { " + ", ".join(byts) + " } }"
    out_recs.append(f"#define _UUIDREG_{sym} {uuidinit}")

    uuidstr = uu[:23] + '-' + uu[23:]
    out_recs.append(f'#define UUIDREG_STR_{sym.upper()} "{uuidstr}"')

def main():
    with open(sys.argv[1]) as f:
        for line in f.readlines():
            line = re.sub(r'\s*#.*', '', line) # trim comments
            line = re.sub(r'^\s*', '', line)   # trim leading ws
            line = re.sub(r'\s*$', '', line)   # trim trailing ws
            if line == "":
                continue
            m = re.match(r'(.*)\s+(.*)', line)
            assert m
            (uu, sym) = (m.group(1).lower(), m.group(2))
            assert re.match(r'[0-9a-f]{8}(?:-[0-9a-f]{4}){2}-[0-9a-f]{16}', uu)
            assert re.match(r'[a-zA-Z_][a-zA-Z0-9_]*', sym)
            assert len(sym) < 32
            assert uu not in all_uuids
            assert sym not in all_syms
            all_uuids.add(uu)
            all_syms.add(sym)
            emit_uuid_rec(uu, sym)

    with open(sys.argv[2], "w") as f:
        f.write(header)
        for l in out_recs:
            f.write(l + "\n")
        f.write("#endif /* _UUID_REGISTRY_H */\n")

if __name__ == "__main__":
	main()
