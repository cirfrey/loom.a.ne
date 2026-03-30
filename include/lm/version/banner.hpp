#pragma once

#include "lm/core/types.hpp"
#include "lm/chip/types.hpp"

namespace lm::version
{
    /* Writes the banner to console. Heres an example banner:

> ---------------- Loom Audio Nexus (loom.a.ne) v0.1 - Resonant Horizon
> .    . ___  .         ___   .        ___      .     ___
>       /\  \     .    /\  \       .  /\  \          /\  \         .
>    . /::\  \ .      /::\  \        /::\  \   .    /::\  \
>     /:/\:\  \   .  /:/\:\  \      /:/\:\  \      /:/\:\  \
>    /:/  \:\  \    /:/  \:\  \ .  /:/  \:\  \    /:/  \:\  \
>   /:/  / \:\  \  /:/  / \:\  \  /:/  / \:\  \  /:/  / \:\  \    .
>   \:\  \ /:/  / .\:\  \ /:/  /  \:\  \ /:/  /  \:\  \ /:/  /
>   .\:\  /:/  /    \:\  /:/  /    \:\  /:/  /    \:\  /:/  / .
>     \:\/:/  /  .   \:\/:/  /      \:\/:/  /  .   \:\/:/  /         .
>    . \::/  / .    . \::/  /   .    \::/  /        \::/  /
> .     \/__/   .     .\/__/          \/__/          \/__/
> ------------------------------------------------- [48:27:E2:57:1A:DE]
>  [CHIP  ] ESP32-S2 XTensa LX7 32bit 1c@240MHz  [RAM ] 11kb / 246kb
>  [UPTIME] 69 days, 4 hours, 20 minutes         [TEMP] 30.9°C
> --------------------------------------- [89762c5:2026-03-28T23:21:13]

    */
    auto write_banner(
        chip::uart_port port, // Where to write this.
        u8 major, u8 minor,
        text git_hash, text build_date,
        text prefix = text::from("\n"), // Appended to the start of the banner. Useful if you want
                                        // a little space or some extra info/branding before printing
                                        // the chip banner and specs.
        u8 interval = 1 // How much to sleep for between characters. 0 disables this feature.
                        // Cmon, if we're printing a banner we might as well make it look cool.
                        // Also, if we're on a boot loop for whatever reason this helps to not
                        // absolutely flood the terminal every time we restart (assuming you)
                        // are printing the banner during the boot sequence.
    ) -> void;

    constexpr const char* adjectives[32] = {
        "Astral", "Resonant", "Kinetic", "Spectral", "Nebulous", "Harmonic", "Orbital", "Granular",
        "Velocity", "Infinite", "Void", "Stellar", "Tactile", "Pulsar", "Chrome", "Cosmic",
        "Latent", "Async", "Bit-Wise", "Static", "Virtual", "Buffered", "Quantized", "Radiant",
        "Jittery", "Sonic", "Atomic", "Analog", "Synced", "Flux", "Modular", "Parallel"
    };
    auto get_version_adjective(u8 major, u8 minor) -> char const*;

    constexpr const char* subjects[32] = {
        "Singularity", "Horizon", "Gravity", "Vacuum", "Gate", "Pulsar", "Quasar", "Void",
        "Zenith", "Nadir", "Monolith", "Supernova", "Ion-Stream", "Dark-Matter", "Wormhole", "Nebula",
        "Kernel", "Bus", "Jack", "Loom", "Engine", "Matrix", "Fabric", "Warp",
        "Frame", "Trace", "Signal", "Relay", "Bridge", "Driver", "Hub", "Array"
    };
    auto get_version_subject(u8 major, u8 minor)   -> char const*;
}
