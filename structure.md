
| Type | Filename | Function |
|------|----------|----------|
| HAL | /lm/chip/* anc lm/chip.hpp | Abstracts the physical parts/functions of the chip (ESP32, STM32, AtMega, Desktop, etc)
| BSP | /lm/boards/* and lm/board.hpp | Abstracts how the chip is broken-out into a physical board with pins and stuff. (Lolin S2 Mini, Seeduino XIAO, etc)
| OSAL | /lm/fabric/primitives.hpp | The "Mechanical Parts." . It wraps the RTOS data primitives (Queue, Ringbuf, etc).
| OSAL | /lm/fabric/task.hpp | The worker. Wraps RTOS execution primitives (task).
| Generic | /lm/fabric/bus.hpp | The router/transport system for communicating safely between tasks
| Generic | /lm/fabric/types.hpp | The "Data Dictionary"/common language of the bus.
| Generic | /lm/core | Stuff you need to get this show on the road |
| Generic | /lm/usbd | Mostly stubs for setting up configuration descriptors for specific usbd functions (HID, MIDI, etc).
| Generic | /lm/tasks | A bunch of tasks that do different things, you probably want to look at lm::tman, it is responsible for orchestrating those tasks.
| Generic | /lm/version | Branding (banner) and library info.
| Generic | /lm/config.hpp | Centralized configuration header for many different aspects of this framework (task stack sizes, mount points, buffer sizes, etc)
