/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#include <new>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <zephyr/kernel.h>
#include "absl/strings/string_view.h"
#include "rtc_base/experiments/field_trial_parser.h"

extern "C" {
void *rmalloc_align(uint32_t flags, size_t bytes, uint32_t alignment);
void *rmalloc(uint32_t flags, size_t bytes);
void rfree(void *ptr);
void *webrtc_malloc(size_t size);
void webrtc_free(void *ptr);
}

#define SOF_MEM_FLAG_USER (1 << 8)

void* operator new(std::size_t size) {
    void* p = webrtc_malloc(size);
    if (!p)
        k_panic();
    return p;
}

void* operator new[](std::size_t size) {
    void* p = webrtc_malloc(size);
    if (!p)
        k_panic();
    return p;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return webrtc_malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return webrtc_malloc(size);
}

void operator delete(void* ptr) noexcept {
    webrtc_free(ptr);
}

void operator delete[](void* ptr) noexcept {
    webrtc_free(ptr);
}

void operator delete(void* ptr, std::size_t size) noexcept {
    (void)size;
    webrtc_free(ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept {
    (void)size;
    webrtc_free(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    webrtc_free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    webrtc_free(ptr);
}

extern "C" {
void* __dso_handle = nullptr;
int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void __cxa_pure_virtual() { k_panic(); }
}

namespace std {
void __throw_bad_alloc() { k_panic(); }
void __throw_bad_array_new_length() { k_panic(); }
void __throw_length_error(char const*) { k_panic(); }
void __throw_logic_error(char const*) { k_panic(); }
void __throw_out_of_range(char const*) { k_panic(); }
void __throw_out_of_range_fmt(char const*, ...) { k_panic(); }
void __throw_bad_function_call() { k_panic(); }
}

namespace webrtc {

void* GetRightAlign(const void* pointer, size_t alignment) {
    if (!pointer) return nullptr;
    uintptr_t start_pos = reinterpret_cast<uintptr_t>(pointer);
    return reinterpret_cast<void*>((start_pos + alignment - 1) & ~(alignment - 1));
}

void* AlignedMalloc(size_t size, size_t alignment) {
    if (size == 0) return nullptr;
    if (alignment < sizeof(void*)) alignment = sizeof(void*);
    void* ptr = rmalloc_align(SOF_MEM_FLAG_USER, size, alignment);
    if (!ptr)
        k_panic();
    return ptr;
}

void AlignedFree(void* mem_block) {
    rfree(mem_block);
}

namespace field_trial {
std::string FindFullName(absl::string_view name) {
    (void)name;
    return std::string();
}
bool IsEnabled(absl::string_view name) {
    (void)name;
    return false;
}
bool IsDisabled(absl::string_view name) {
    (void)name;
    return false;
}
void InitFieldTrialsFromString(const char* trials_string) { (void)trials_string; }
const char* GetFieldTrialString() { return ""; }
bool FieldTrialsStringIsValid(absl::string_view trials_string) { (void)trials_string; return true; }
std::string MergeFieldTrialsStrings(absl::string_view first, absl::string_view second) {
    (void)first; (void)second;
    return std::string();
}
} // namespace field_trial

FieldTrialParameterInterface::FieldTrialParameterInterface(absl::string_view key) {
    (void)key;
}
FieldTrialParameterInterface::~FieldTrialParameterInterface() = default;

void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> fields,
    absl::string_view trial_string) {
    (void)fields;
    (void)trial_string;
}

template <>
std::optional<double> ParseTypedParameter<double>(absl::string_view) { return std::nullopt; }
template <>
std::optional<int> ParseTypedParameter<int>(absl::string_view) { return std::nullopt; }

template class FieldTrialParameter<double>;
template class FieldTrialParameter<int>;

} // namespace webrtc
