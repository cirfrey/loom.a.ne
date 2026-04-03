# Nice-to-haves
[x] Implement version banner with art.
[x] Embed build info in code.
[x] Rename project (espy is very specific to esp32 boards).

# Phase 1: Infrastructure & Stability
[x] Implement lm::config (Centralize priorities/stack sizes).
[x] Implement Logging Task (RingBuffer + Worker Task).
    [ ] Fix HID + CDC logging problems.
    [ ] Reimplement HID logs.
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
[ ] Implement basic ::chip, ::board && ::fabric... for desktop mocking.
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
[ ] Fix spotty enumeration when descriptor is very big (UAC+HID+MIDI*12)

---

src/lm/tasks/usbd.cpp: In function 'void lm::usbd::task(const lm::fabric::task_constants&)':
src/lm/tasks/usbd.cpp:67:20: error: 'task' in namespace 'lm' does not name a type
   67 |     using tc = lm::task::event::task_command;
      |                    ^~~~
src/lm/tasks/usbd.cpp:68:19: error: 'tc' has not been declared
   68 |     auto tc_bus = tc::make_bus();
      |                   ^~
src/lm/tasks/usbd.cpp:69:5: error: 'tc' has not been declared
   69 |     tc::wait_for_start(tc_bus, cfg.id);
      |     ^~
src/lm/tasks/usbd.cpp:69:36: error: 'const struct lm::fabric::task_constants' has no member named 'id'
   69 |     tc::wait_for_start(tc_bus, cfg.id);
      |                                    ^~
src/lm/tasks/usbd.cpp:72:12: error: 'tc' has not been declared
   72 |     while(!tc::should_stop(tc_bus, cfg.id)) {
      |            ^~
src/lm/tasks/usbd.cpp:72:40: error: 'const struct lm::fabric::task_constants' has no member named 'id'
   72 |     while(!tc::should_stop(tc_bus, cfg.id)) {
      |                                        ^~
src/lm/tasks/usbd.cpp: In function 'void lm::usbd::init(configuration_t&, std::span<endpoint_info_t>, descriptors_t)':
src/lm/tasks/usbd.cpp:140:21: error: 'lm::bus' has not been declared
  140 |     if(cfg.cdc) lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::cdc_enabled});
      |                     ^~~
src/lm/tasks/usbd.cpp:140:48: error: 'lm::bus' has not been declared
  140 |     if(cfg.cdc) lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::cdc_enabled});
      |                                                ^~~
src/lm/tasks/usbd.cpp:141:21: error: 'lm::bus' has not been declared
  141 |     else        lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::cdc_disabled});
      |                     ^~~
src/lm/tasks/usbd.cpp:141:48: error: 'lm::bus' has not been declared
  141 |     else        lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::cdc_disabled});
      |                                                ^~~
src/lm/tasks/usbd.cpp:142:21: error: 'lm::bus' has not been declared
  142 |     if(cfg.hid) lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::hid_enabled});
      |                     ^~~
src/lm/tasks/usbd.cpp:142:48: error: 'lm::bus' has not been declared
  142 |     if(cfg.hid) lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::hid_enabled});
      |                                                ^~~
src/lm/tasks/usbd.cpp:143:21: error: 'lm::bus' has not been declared
  143 |     else        lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::hid_disabled});
      |                     ^~~
src/lm/tasks/usbd.cpp:143:48: error: 'lm::bus' has not been declared
  143 |     else        lm::bus::publish({.topic = lm::bus::usbd, .type = (u8)lm::usbd::event::hid_disabled});
      |                                                ^~~
src/lm/tasks/usbd.cpp:145:9: error: 'lm::task' has not been declared
  145 |     lm::task::create(lm::config::task::usbd, lm::usbd::task);
      |         ^~~~
Compiling .pio\build\lolin_s2_mini\bootloader_support\src\bootloader_clock_init.c.o
*** [.pio\build\lolin_s2_mini\src\lm\tasks\usbd.cpp.o] Error 1
src/lm/tasks/usbdlogging.cpp:70:19: error: 'lm::task' has not been declared
   70 |     auto task(lm::task::config const&) -> void;
      |                   ^~~~
src/lm/tasks/usbdlogging.cpp:70:40: error: expected ',' or ';' before '->' token
   70 |     auto task(lm::task::config const&) -> void;
      |                                        ^~
src/lm/tasks/usbdlogging.cpp: In function 'void lm::logging::init()':
src/lm/tasks/usbdlogging.cpp:202:9: error: 'lm::task' has not been declared
  202 |     lm::task::create(lm::config::task::logging, lm::logging::task);
      |         ^~~~
src/lm/tasks/usbdlogging.cpp: At global scope:
src/lm/tasks/usbdlogging.cpp:205:28: error: 'lm::task' has not been declared
  205 | auto lm::logging::task(lm::task::config const& cfg) -> void
      |                            ^~~~
*** [.pio\build\lolin_s2_mini\src\lm\tasks\usbdlogging.cpp.o] Error 1
