# Nice-to-haves
[x] Implement version banner with art.
[x] Embed build info in code.
[x] Rename project (espy is very specific to esp32 boards).
[~] Native builds
    [ ] Embed build info in /static via native builds
    [ ] Emulate USB via USB/IP?

# Phase 1: Infrastructure & Stability
[x] Implement lm::config (Centralize priorities/stack sizes).
[x] Implement Logging Task (RingBuffer + Worker Task).
    [x] Fix HID + CDC logging problems.
    [x] Reimplement HID logs.
[~] Implement Healthmon - RAM Health Monitor (Heap tracking) / CPU Load Monitor / Task Monitor. ~Only ram for now.
[x] Implement Event Bus.
[x] Implement Task lifetime and monitoring via statuses and commands from the Event Bus
[x] Implement Busmon.
[x] Implement Hardware/OS abstractions.
    [x] Implement HAL lm::chip.
    [x] Implement BSP lm::board.
    [x] Implement OSAL lm::fabric::task && lm::fabric::primitives.
        [x] Implement RingBuf for ESP32.
        [x] Implement semaphore.
[~] Implement lm::sysman (The State Machine "Brain").
[ ] Implement lm::bus::input -> Publish to Event Bus and react accordingly.
[ ] Have the code stable enough where uart_bridge.ino can be implemented in terms of loom.a.ne.

# Phase 2: Audio & Storage
[~] Fix UAC2. ~Enumerates and shows up on windows but I didn't test it yet. Code needs cleanup.
[ ] Mount /static (Initialize filesystem on boot).
[~] Add MSC support (/static) for debugging (readonly). ~Compiles, not working
[ ] Implement WavLoader (Helper to read .wav header/data from storage).
[ ] Integration Test: Play test.wav from static -> USB UAC2.
[ ] Add MSC support (/storage)
[ ] Hardware Setup: Wire SD Card Slot to S2 Mini (SPI pins).
[ ] Integration Test: Play test.wav from SD Card -> USB UAC2.

# Phase 3: Wireless Audio Bridge
[ ] Implement ESP-NOW Packet Structure (Sequence IDs).
[ ] Implement Protocol Structs: Define the Header, AudioPacket, MidiPacket.
[ ] Implement ESP-NOW Audio Transmission.
[ ] Implement Jitter Buffer.

# Phase 4: User Experience (Portal)
[ ] Build System: Gzip compression script for /static (necessary?).
[ ] Web Server: API-driven design.

# Phase 5: Nice-to-haves
[x] Fix spotty enumeration when descriptor is very big (UAC+HID+MIDI*12) - Fixed with dedicated TUD task.
