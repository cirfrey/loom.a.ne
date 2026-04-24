
#include "lm/hooks.hpp"

#include "lm/fabric/strand.hpp"
#include "lm/port.hpp"

#include "lm/arch/x86_64/program_args.hpp"

#if LM_PORT_HOST_WINDOWS
    // Native Windows (MSVC, MinGW, Clang-cl)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <atomic>

    std::atomic<bool> quit = false;
    BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
        if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_CLOSE_EVENT) {
            quit = true;
            // Returning TRUE tells Windows we handled it
            return TRUE;
        }
        return FALSE;
    }
    auto main(int argc, char* argv[]) -> int
    {
        lm::arch::x86_64::program_args = {argv, (lm::st)argc};

        SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
        lm::hook::launcher();
        // Check for ctrl+c every second.
        while(!quit.load()) lm::fabric::strand::sleep_ms(1000);
        return 0;
    }

#else

    auto main(int argc, char* argv[]) -> int
    {
        lm::arch::x86_64::program_args = {argv, (lm::st)argc};

        lm::hook::launcher();
        // Linux doesnt need all that fancy ctrl+c every second thing apparently.
        using ulong = unsigned long;
        while(1) lm::fabric::strand::sleep_ms(~ulong{0});
        return 0;
    }

#endif
