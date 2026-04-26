#include "lm/chip.hpp"
#include "lm/core.hpp"

#include "lm/port.hpp"

#include "lm/arch/x86_64/program_args.hpp"

#if LM_PORT_IS_POSIX
    #include <unistd.h>
#endif

#if LM_PORT_HOST_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include <thread>

auto lm::chip::system::init() -> void
{
    // No-op for native OS
}

auto lm::chip::system::reboot(st code) -> void
{
    // POSIX / MinGW Soft Reboot (exec)
    if constexpr(lm::port::is_posix) {
        if (!lm::arch::x86_64::program_args.empty()) {
            // execvp uses the PATH to find the binary and replaces the process.
            // execvp requires the last element of the array (argv[argc]) to be nullptr.
            char* const* argv = lm::arch::x86_64::program_args.data();
            execvp(argv[0], argv);
        }
    }

    // Windows Native Soft Reboot
    // TODO: test outside mingw.
    if constexpr(lm::port::host == lm::port::host_t::windows)
    {
        LPWSTR lpCmdLine = GetCommandLineW();

        STARTUPINFOW si{};
        si.cb = sizeof(si);

        PROCESS_INFORMATION pi{};

        if (CreateProcessW(
                nullptr,    // lpApplicationName
                lpCmdLine,  // lpCommandLine
                nullptr,    // lpProcessAttributes
                nullptr,    // lpThreadAttributes
                FALSE,      // bInheritHandles
                0,          // dwCreationFlags
                nullptr,    // lpEnvironment
                nullptr,    // lpCurrentDirectory
                &si,        // lpStartupInfo
                &pi         // lpProcessInformation
            ))
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    // Fallback: If reboot fails or isn't supported, halt.
    halt(code);
}

auto lm::chip::system::panic(text msg, st code) -> void
{
    std::printf("%.*s", (int)msg.size, msg.data);
    halt(code);
}

auto lm::chip::system::halt(st code) -> void
{
    std::exit(code);
}

auto lm::chip::system::core_count() -> st
{
    st cores = std::thread::hardware_concurrency();
    return cores > 0 ? cores : 1;
}
