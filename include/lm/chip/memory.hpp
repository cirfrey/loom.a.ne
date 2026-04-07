#pragma once

#include "lm/core/types.hpp"

#include <span>

namespace lm::chip::memory
{
    // How much RAM there is in total in bytes.
    auto total() -> st;
    // Available RAM (heap) in bytes.
    auto free() -> st;
    // Get the maxiumum amount of RAM that's been used (high water mark) in bytes.
    auto peak_used() -> st;
    // Get the size of the largest free block in RAM in bytes.
    auto largest_free_block() -> st;


    enum class storage_op_status {
        ok,
        out_of_bounds,
        unaligned_access,
        hardware_failure,
        unauthorized_access,
    };
    struct storage
    {
        text const label = text{nullptr, 0};
        u8 const type    = 0;
        u8 const subtype = 0;
        st const size    = 0;
        st const sector_size = 0;
        st const absolute_base = 0;
        bool const readonly = 0;

        auto read(st offset, mut_buf out) const -> storage_op_status;
        auto write(st offset, buf data) -> storage_op_status;
        auto erase(st offset, st length) -> storage_op_status;
        // static auto raw_read(st global_offset, mut_buf out) -> storage_op_status;

        void* const impl;
    };
    auto get_storages() -> std::span<storage>;
}

/// Gemini:
/// (Future) You will likely need to distinguish between internal RAM and
/// external PSRAM for audio buffers: auto free_internal() -> st; and auto free_psram() -> st;
