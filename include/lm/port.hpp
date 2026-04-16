// Loomane portability header.
// We could cheat and ask platformio to sneak this info to us
// ... but whats the fun in that?
// Also, we want this to be somewhat portable, why rely on platformio?
#pragma once

// Having all the macros be defaulted to 0 is handy since the
// consumer(you) has to write less code:
//     #if LM_PORT_COMPILER_CLANG && !LM_...
// Instead of:
//     #if defined(LM_PORT_COMPILER_CLANG) && !defined(LM_...)
// Also helps with autocompletion on IDEs.

// --- Compiler ---
#define LM_PORT_COMPILER_CLANG   0
#define LM_PORT_COMPILER_GCC     0
#define LM_PORT_COMPILER_MSVC    0
#define LM_PORT_COMPILER_UNKNOWN 0
#if defined(__clang__)
    #undef  LM_PORT_COMPILER_CLANG
    #define LM_PORT_COMPILER_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
    #undef  LM_PORT_COMPILER_GCC
    #define LM_PORT_COMPILER_GCC 1
#elif defined(_MSC_VER)
    #undef  LM_PORT_COMPILER_MSVC
    #define LM_PORT_COMPILER_MSVC 1
#else
    #undef  LM_PORT_COMPILER_UNKNOWN
    #define LM_PORT_COMPILER_UNKNOWN 1
#endif

// --- C++ Standard ---
#if defined(_MSVC_LANG)
    #define LM_PORT_CPP _MSVC_LANG
#else
    #define LM_PORT_CPP __cplusplus
#endif

// --- Environment Layer ---
#define LM_PORT_ENV_FREESTANDING 0
#define LM_PORT_ENV_CYGWIN       0
#define LM_PORT_ENV_MINGW        0
#define LM_PORT_ENV_NATIVE       0
#define LM_PORT_ENV_UNKNOWN      0
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0
    #undef  LM_PORT_ENV_FREESTANDING
    #define LM_PORT_ENV_FREESTANDING 1
#elif defined(__CYGWIN__)
    #undef  LM_PORT_ENV_CYGWIN
    #define LM_PORT_ENV_CYGWIN 1
#elif defined(__MINGW32__) || defined(__MINGW64__)
    #undef  LM_PORT_ENV_MINGW
    #define LM_PORT_ENV_MINGW 1
#else // TODO: defaults to native, is this right?
    #undef  LM_PORT_ENV_NATIVE
    #define LM_PORT_ENV_NATIVE 1
#endif

// --- Host OS / Hardware Platform ---
#define LM_PORT_HOST_ESP32   0
#define LM_PORT_HOST_ESP8266 0
#define LM_PORT_HOST_IOS     0
#define LM_PORT_HOST_MACOS   0
#define LM_PORT_HOST_WINDOWS 0
#define LM_PORT_HOST_LINUX   0
#define LM_PORT_HOST_STM32   0
#define LM_PORT_HOST_NORDIC  0
#define LM_PORT_HOST_PICO    0
#define LM_PORT_HOST_TEENSY  0
#define LM_PORT_HOST_AVR     0
#define LM_PORT_HOST_ANDROID 0
#define LM_PORT_HOST_UNKNOWN 0
#if defined(ESP_PLATFORM)
    #undef  LM_PORT_HOST_ESP32
    #define LM_PORT_HOST_ESP32 1
#elif defined(ESP8266)
    #undef  LM_PORT_HOST_ESP8266
    #define LM_PORT_HOST_ESP8266 1
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #undef  LM_PORT_HOST_IOS
        #define LM_PORT_HOST_IOS 1
    #else
        #undef  LM_PORT_HOST_MACOS
        #define LM_PORT_HOST_MACOS 1
    #endif
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #undef  LM_PORT_HOST_WINDOWS
    #define LM_PORT_HOST_WINDOWS 1
#elif defined(__linux__)
    #undef  LM_PORT_HOST_LINUX
    #define LM_PORT_HOST_LINUX 1
#elif defined(STM32) || defined(USE_HAL_DRIVER)
    #undef  LM_PORT_HOST_STM32
    #define LM_PORT_HOST_STM32 1
#elif defined(NRF51) || defined(NRF52) || defined(NRF52840_XXAA)
    #undef  LM_PORT_HOST_NORDIC
    #define LM_PORT_HOST_NORDIC 1
#elif defined(PICO_BOARD) || defined(RASPBERRYPI_PICO)
    #undef  LM_PORT_HOST_PICO
    #define LM_PORT_HOST_PICO 1
#elif defined(__MK64FX512__) || defined(__MK66FX1M0__) || defined(TEENSYDUINO)
    #undef  LM_PORT_HOST_TEENSY
    #define LM_PORT_HOST_TEENSY 1
#elif defined(__AVR__)
    #undef  LM_PORT_HOST_AVR
    #define LM_PORT_HOST_AVR 1
#elif defined(__ANDROID__)
    #undef  LM_PORT_HOST_ANDROID
    #define LM_PORT_HOST_ANDROID 1
#else
    #undef  LM_PORT_HOST_UNKNOWN
    #define LM_PORT_HOST_UNKNOWN 1
#endif

// --- CPU Architecture ---
#define LM_PORT_CPU_X64     0
#define LM_PORT_CPU_X86     0
#define LM_PORT_CPU_ARM64   0
#define LM_PORT_CPU_ARM32   0
#define LM_PORT_CPU_XTENSA  0
#define LM_PORT_CPU_RISCV   0
#define LM_PORT_CPU_AVR     0
#define LM_PORT_CPU_UNKNOWN 0
#if defined(__x86_64__) || defined(_M_X64)
    #undef  LM_PORT_CPU_X64
    #define LM_PORT_CPU_X64 1
#elif defined(__i386) || defined(_M_IX86)
    #undef  LM_PORT_CPU_X86
    #define LM_PORT_CPU_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #undef  LM_PORT_CPU_ARM64
    #define LM_PORT_CPU_ARM64 1
#elif defined(__arm__) || defined(_M_ARM)
    #undef  LM_PORT_CPU_ARM32
    #define LM_PORT_CPU_ARM32 1
#elif defined(__XTENSA__)
    #undef  LM_PORT_CPU_XTENSA
    #define LM_PORT_CPU_XTENSA 1
#elif defined(__riscv)
    #undef  LM_PORT_CPU_RISCV
    #define LM_PORT_CPU_RISCV 1
#elif defined(__AVR__)
    #undef  LM_PORT_CPU_AVR
    #define LM_PORT_CPU_AVR 1
#else
    #undef  LM_PORT_CPU_UNKNOWN
    #define LM_PORT_CPU_UNKNOWN 1
#endif

// --- Pointer Width ---
#if defined(__SIZEOF_POINTER__)
    #define LM_PORT_PTR_SIZE __SIZEOF_POINTER__
#elif defined(_WIN64)
    #define LM_PORT_PTR_SIZE 8
#else
    #define LM_PORT_PTR_SIZE 4
#endif

// --- Endianness ---
#define LM_PORT_ENDIAN_LITTLE  0
#define LM_PORT_ENDIAN_BIG     0
#define LM_PORT_ENDIAN_UNKNOWN 0
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #undef  LM_PORT_ENDIAN_LITTLE
        #define LM_PORT_ENDIAN_LITTLE 1
    #else
        #undef  LM_PORT_ENDIAN_BIG
        #define LM_PORT_ENDIAN_BIG 1
    #endif
#elif LM_PORT_CPU_X64 || LM_PORT_CPU_X86 || LM_PORT_CPU_XTENSA || \
      LM_PORT_CPU_RISCV || LM_PORT_CPU_AVR || LM_PORT_CPU_ARM32 || \
      LM_PORT_CPU_ARM64
    #undef  LM_PORT_ENDIAN_LITTLE
    #define LM_PORT_ENDIAN_LITTLE 1
#else
    #undef  LM_PORT_ENDIAN_UNKNOWN
    #define LM_PORT_ENDIAN_UNKNOWN 1
#endif

// --- Utility ---
#define LM_PORT_IS_64_BIT (LM_PORT_PTR_SIZE == 8)
#define LM_PORT_IS_32_BIT (LM_PORT_PTR_SIZE == 4)
#define LM_PORT_IS_POSIX  \
    (LM_PORT_HOST_LINUX || \
    LM_PORT_HOST_MACOS || \
    LM_PORT_HOST_IOS ||   \
    LM_PORT_ENV_CYGWIN)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    #define LM_PORT_HAS_EXCEPTIONS 1
#else
    #define LM_PORT_HAS_EXCEPTIONS 0
#endif

#if defined(__cpp_rtti) || defined(__GXX_RTTI) || defined(_CPPRTTI)
    #define LM_PORT_HAS_RTTI 1
#else
    #define LM_PORT_HAS_RTTI 0
#endif

namespace lm::port
{
    enum class compiler_t { unknown, gcc, clang, msvc };
    enum class env_t {
        unknown,
        native,         // Standard OS layer
        cygwin,         // POSIX-on-Windows
        mingw,          // Minimalist GNU-on-Windows
        freestanding    // Baremetal / No standard OS utilities
    };
    enum class host_t {
        unknown, windows, linux, macos, ios, android,
        esp32, esp8266, stm32, nordic, pico, teensy, avr
    };
    enum class cpu_t { unknown, x86, x64, arm32, arm64, xtensa, riscv, avr };
    enum class endian_t { unknown, little, big };

    enum class cpp_std_t : long {
        cpp98 = 199711L,
        cpp11 = 201103L,
        cpp14 = 201402L,
        cpp17 = 201703L,
        cpp20 = 202002L,
        cpp23 = 202302L
    };


    inline constexpr compiler_t compiler =
        #if   LM_PORT_COMPILER_CLANG
            compiler_t::clang;
        #elif LM_PORT_COMPILER_GCC
            compiler_t::gcc;
        #elif LM_PORT_COMPILER_MSVC
            compiler_t::msvc;
        #else
            compiler_t::unknown;
        #endif

    inline constexpr env_t env =
        #if   LM_PORT_ENV_FREESTANDING
            env_t::freestanding;
        #elif LM_PORT_ENV_CYGWIN
            env_t::cygwin;
        #elif LM_PORT_ENV_MINGW
            env_t::mingw;
        #else
            env_t::native;
        #endif

    inline constexpr host_t host =
        #if   LM_PORT_HOST_ESP32
            host_t::esp32;
        #elif LM_PORT_HOST_ESP8266
            host_t::esp8266;
        #elif LM_PORT_HOST_STM32
            host_t::stm32;
        #elif LM_PORT_HOST_NORDIC
            host_t::nordic;
        #elif LM_PORT_HOST_PICO
            host_t::pico;
        #elif LM_PORT_HOST_TEENSY
            host_t::teensy;
        #elif LM_PORT_HOST_AVR
            host_t::avr;
        #elif LM_PORT_HOST_MACOS
            host_t::macos;
        #elif LM_PORT_HOST_IOS
            host_t::ios;
        #elif LM_PORT_HOST_LINUX
            host_t::linux;
        #elif LM_PORT_HOST_WINDOWS
            host_t::windows;
        #elif LM_PORT_HOST_ANDROID
            host_t::android;
        #else
            host_t::unknown;
        #endif

    inline constexpr cpu_t cpu =
        #if   LM_PORT_CPU_X64
            cpu_t::x64;
        #elif LM_PORT_CPU_ARM64
            cpu_t::arm64;
        #elif LM_PORT_CPU_XTENSA
            cpu_t::xtensa;
        #elif LM_PORT_CPU_RISCV
            cpu_t::riscv;
        #elif LM_PORT_CPU_X86
            cpu_t::x86;
        #elif LM_PORT_CPU_ARM32
            cpu_t::arm32;
        #elif LM_PORT_CPU_AVR
            cpu_t::avr;
        #else
            cpu_t::unknown;
        #endif

    inline constexpr endian_t endianness =
        #if   LM_PORT_ENDIAN_LITTLE
            endian_t::little;
        #elif LM_PORT_ENDIAN_BIG
            endian_t::big;
        #else
            endian_t::unknown;
        #endif

    inline constexpr cpp_std_t standard =
        LM_PORT_CPP >= (long)cpp_std_t::cpp23 ? cpp_std_t::cpp23 :
        LM_PORT_CPP >= (long)cpp_std_t::cpp20 ? cpp_std_t::cpp20 :
        LM_PORT_CPP >= (long)cpp_std_t::cpp17 ? cpp_std_t::cpp17 :
        LM_PORT_CPP >= (long)cpp_std_t::cpp14 ? cpp_std_t::cpp14 :
        LM_PORT_CPP >= (long)cpp_std_t::cpp11 ? cpp_std_t::cpp11 :
        cpp_std_t::cpp98;

    // --- Utility Flags ---
    inline constexpr bool is_little_endian = LM_PORT_ENDIAN_LITTLE;
    inline constexpr bool is_big_endian    = LM_PORT_ENDIAN_BIG;
    inline constexpr bool is_64_bit        = LM_PORT_IS_64_BIT;
    inline constexpr bool is_32_bit        = LM_PORT_IS_32_BIT;
    inline constexpr bool is_posix         = LM_PORT_IS_POSIX;
    inline constexpr bool has_exceptions   = LM_PORT_HAS_EXCEPTIONS;
    inline constexpr bool has_rtti         = LM_PORT_HAS_RTTI;
} // namespace lm::port
