#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Intel Corporation. All rights reserved.
"""
Convert the float32 streaming state of a microWakeWord TFLite model to int8.

The TFLite converter keeps the six streaming ring buffers of the models from
sof_mww_train_pcan.py in float32 resource variables. Every inference then runs
DEQUANTIZE -> CONCATENATION -> QUANTIZE for each layer and DEQUANTIZE ->
ASSIGN_VARIABLE for the state update. The int8 tensors on both sides of these
float paths have the same scale and zero point, so the state can be kept in
int8 without changing any value. This script rewrites the flatbuffer
accordingly and emits the firmware header and the sof-ctl blob with the
struct sof_mww_model_hdr model header. A model without float32 state, e.g.
a model in the old format without the header, is emitted unchanged with the
header. The header settings come from the input header if it has one, and
the command line options override them.
sof_mww_train_pcan.py applies the same conversion to the models it exports.

Example:
    python3 sof_mww_int8_state.py --name banana \\
        ../mww_model_data_banana.h --out-dir /tmp/banana_int8
"""

import argparse
import copy
import re
import sys
from pathlib import Path

import numpy as np
from tensorflow.lite.python import schema_py_generated as schema
from tensorflow.lite.tools import flatbuffer_utils as fu

import sof_mww_train_pcan as mww

OP = schema.BuiltinOperator
TT = schema.TensorType


def read_model_bytes(path: Path) -> bytes:
    if path.suffix == ".h":
        text = path.read_text()
        body = text[text.index("{") + 1 : text.index("}")]
        return bytes(int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]{2}", body))
    return path.read_bytes()


def opcode(model, op) -> int:
    oc = model.operatorCodes[op.opcodeIndex]
    return max(oc.builtinCode, oc.deprecatedBuiltinCode)


def quant_eq(a, b) -> bool:
    return (a is not None and b is not None and
            np.array_equal(a.scale, b.scale) and np.array_equal(a.zeroPoint, b.zeroPoint))


def var_name(model, sg, tensor_idx) -> bytes:
    """Shared name of the VAR_HANDLE that produces the resource tensor."""
    for op in sg.operators:
        if opcode(model, op) == OP.VAR_HANDLE and op.outputs[0] == tensor_idx:
            return op.builtinOptions.sharedName
    raise ValueError(f"no VAR_HANDLE for tensor {tensor_idx}")


def compact(sg, sig_defs, sg_index) -> None:
    """Drop tensors no longer referenced and remap indices."""
    used = set(sg.inputs) | set(sg.outputs)
    for op in sg.operators:
        used |= {i for i in op.inputs if i >= 0} | set(op.outputs)
        if op.intermediates is not None:
            used |= set(op.intermediates)
    keep = sorted(used)
    remap = {old: new for new, old in enumerate(keep)}
    sg.tensors = [sg.tensors[i] for i in keep]
    sg.inputs = [remap[i] for i in sg.inputs]
    sg.outputs = [remap[i] for i in sg.outputs]
    for op in sg.operators:
        op.inputs = [remap[i] if i >= 0 else i for i in op.inputs]
        op.outputs = [remap[i] for i in op.outputs]
        if op.intermediates is not None:
            op.intermediates = [remap[i] for i in op.intermediates]
    for sd in sig_defs or []:
        if sd.subgraphIndex == sg_index:
            for t in list(sd.inputs or []) + list(sd.outputs or []):
                t.tensorIndex = remap[t.tensorIndex]


def convert(model) -> int:
    sg = model.subgraphs[0]
    producer = {o: op for op in sg.operators for o in op.outputs}
    consumers = {}
    for op in sg.operators:
        for i in op.inputs:
            consumers.setdefault(i, []).append(op)

    remove = []
    state_quant = {}
    for rd in [op for op in sg.operators if opcode(model, op) == OP.READ_VARIABLE]:
        res, state = rd.inputs[0], rd.outputs[0]
        if sg.tensors[state].type != TT.FLOAT32:
            continue
        (cat,) = consumers[state]
        assert opcode(model, cat) == OP.CONCATENATION, "state not consumed by concat"
        (q,) = consumers[cat.outputs[0]]
        assert opcode(model, q) == OP.QUANTIZE, "concat output not quantized"
        cat_q = sg.tensors[q.outputs[0]].quantization

        new_inputs = []
        for i in cat.inputs:
            if i == state:
                new_inputs.append(i)
                continue
            dq = producer[i]
            assert opcode(model, dq) == OP.DEQUANTIZE, "concat input not dequantized"
            assert quant_eq(sg.tensors[dq.inputs[0]].quantization, cat_q), "scale mismatch"
            new_inputs.append(dq.inputs[0])
            if len(consumers[i]) == 1:
                remove.append(dq)
        cat.inputs = new_inputs
        cat.outputs = [q.outputs[0]]
        remove.append(q)

        (asg,) = [op for op in consumers[res] if opcode(model, op) == OP.ASSIGN_VARIABLE]
        dq = producer[asg.inputs[1]]
        assert opcode(model, dq) == OP.DEQUANTIZE, "state update not dequantized"
        assert quant_eq(sg.tensors[dq.inputs[0]].quantization, cat_q), "state scale mismatch"
        asg.inputs = [res, dq.inputs[0]]
        remove.append(dq)

        sg.tensors[state].type = TT.INT8
        sg.tensors[state].quantization = copy.deepcopy(cat_q)
        state_quant[var_name(model, sg, res)] = cat_q

    if not state_quant:
        return 0

    sg.operators = [op for op in sg.operators if all(op is not r for r in remove)]

    # CALL_ONCE init subgraphs assign float zero constants to the variables
    for sgi in model.subgraphs[1:]:
        for asg in [op for op in sgi.operators if opcode(model, op) == OP.ASSIGN_VARIABLE]:
            name = var_name(model, sgi, asg.inputs[0])
            q = state_quant[name]
            t = sgi.tensors[asg.inputs[1]]
            buf = model.buffers[t.buffer]
            vals = np.frombuffer(bytes(buf.data), dtype="<f4")
            qv = np.clip(np.round(vals / q.scale[0]) + q.zeroPoint[0], -128, 127).astype(np.int8)
            t.type = TT.INT8
            t.quantization = copy.deepcopy(q)
            buf.data = np.frombuffer(qv.tobytes(), dtype=np.uint8)

    compact(sg, model.signatureDefs, 0)
    return len(state_quant)


def convert_bytes(src: bytes) -> tuple[bytes, int]:
    """Convert a .tflite flatbuffer, return the new flatbuffer and the converted state count."""
    model = fu.convert_bytearray_to_object(bytearray(src))
    n = convert(model)
    if n == 0:
        return src, 0
    return bytes(fu.convert_object_to_bytearray(model)), n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("model", type=Path, help="Input .tflite or mww_model_data_<name>.h")
    ap.add_argument("--name", required=True, help="Model name for the outputs, e.g. banana")
    ap.add_argument("--out-dir", type=Path, default=Path("."), help="Output directory")
    ap.add_argument("--features", choices=tuple(mww.SOF_MWW_FEATURE_TYPES), help="Feature type (default pcan)")
    ap.add_argument("--threshold", type=float, help="Trigger threshold (default 0.5)")
    ap.add_argument("--avg-len", type=int, help="Trigger averaging length (default 4)")
    args = ap.parse_args()

    src, info = mww.unpack_model_blob(read_model_bytes(args.model))
    info = info or {}
    out, n = convert_bytes(src)
    blob = mww.pack_model_blob(out, args.name,
                               args.features or info.get("features", "pcan"),
                               args.threshold if args.threshold is not None else info.get("threshold", 0.5),
                               args.avg_len or info.get("avg_len", 4))

    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / f"{args.name}_int8_state.tflite").write_bytes(out)
    mww.emit_model_header(blob, args.out_dir / f"mww_model_data_{args.name}.h", args.name)
    mww.emit_sofctl_ipc4_txt(blob, args.out_dir / f"{args.name}.txt")
    if n == 0:
        print(f"{args.model}: no float32 streaming state, emitted unchanged model with header")
    else:
        print(f"{args.model}: {n} state variables converted to int8, {len(src)} -> {len(out)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
