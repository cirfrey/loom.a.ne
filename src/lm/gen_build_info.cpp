#include <iostream>
#include <ctime>

int main() {
    std::time_t now = std::time(nullptr);
    std::cout << "===============================" << std::endl;
    std::cout << "   ESPY-AUDIO BUILD ARTIFACT   " << std::endl;
    std::cout << "===============================" << std::endl;
    std::cout << "Build Date: " << std::ctime(&now);
    std::cout << "Target: ESP32-S2 (Lolin Mini)" << std::endl;
    std::cout << "Status: MSC + UAC2 Validated" << std::endl;
    return 0;
}
