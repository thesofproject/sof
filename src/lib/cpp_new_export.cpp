// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#include <cstddef>
#include <stdlib.h>
#include <rtos/symbol.h>

/*
 * LLEXT modules linking C++ code (e.g. TFLite Micro) can end up with
 * undefined references to global operator new/delete even when built with
 * -fno-exceptions, because some support code (e.g. TFLM's arena allocators)
 * still emits a call to the sized deallocation form in its generated
 * destructors. Export wrappers here so such extensions can resolve them
 * against the base image, mirroring zephyr/lib/cpp/minimal/cpp_new.cpp
 * (which isn't itself referenced anywhere in the base image build, so its
 * definitions are never pulled into the link or exported).
 */

namespace {

[[maybe_unused]] void *sof_operator_new(size_t size)
{
	return malloc(size);
}

[[maybe_unused]] void sof_operator_delete(void *ptr, size_t size)
{
	(void)size;
	free(ptr);
}

/*
 * Same problem as operator new/delete above, one level lower: an abstract
 * class's vtable (e.g. TFLM's MicroOpResolver base) fills the slots for its
 * pure virtual methods with the address of __cxa_pure_virtual, so any
 * translation unit that references such a vtable needs this symbol
 * resolvable even though it should never actually be called at runtime.
 * zephyr/lib/cpp/minimal/cpp_virtual.c already defines a real
 * __cxa_pure_virtual, but -- exactly like cpp_new.cpp above -- nothing in
 * the base image build references it directly, so it is never pulled into
 * the link or exported for LLEXT modules to resolve against. Provide our
 * own and export it under the same name.
 */
[[maybe_unused]] void sof_cxa_pure_virtual(void)
{
	while (1) {
	}
}

} /* namespace */

EXPORT_SYMBOL_NAMED(sof_operator_new, _Znwj);
EXPORT_SYMBOL_NAMED(sof_operator_delete, _ZdlPvj);
EXPORT_SYMBOL_NAMED(sof_cxa_pure_virtual, __cxa_pure_virtual);
