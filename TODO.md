# Nice-to-haves
[x] Implement version banner with art.
[x] Embed build info in code.
[x] Rename project (espy is very specific to esp32 boards).

# Phase 1: Infrastructure & Stability
[x] Implement lm::config (Centralize priorities/stack sizes).
[x] Implement Logging Task (RingBuffer + Worker Task).
    [ ] Fix HID + CDC logging problems.
[~] Implement Healthmon - RAM Health Monitor (Heap tracking) / CPU Load Monitor (FreeRTOS Run Time Stats) / Task Monitor.
[x] Implement Event Bus.
[x] Implement Task lifetime and monitoring via statuses and commands from the Event Bus
    [ ] Finish abstracting this into lm::board.
[x] Implement Busmon.
[x] Implement lm::board.
[~] Implement lm::sysman (The State Machine "Brain"). ~Working, need to implement the rest of the states.
[ ] Implement lm::bus::input -> Publish to Event Bus and react accordingly.
[ ] Implement a minimal lm::board::mock (testing code directly on pc).

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
[ ] Fix spotty enumeration when descriptor is very big (UAC+HID+MIDI*12)
