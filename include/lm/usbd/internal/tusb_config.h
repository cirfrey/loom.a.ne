#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define TUP_DCD_ENPOINT_MAX 7

#define CFG_TUD_DWC2_DMA_ENABLE 1

#include "tusb_option.h"

// --- Platform & OS Abstraction ---
#ifndef LOOMANE_NATIVE
    #include "sdkconfig.h"
    /// NOTE: CFG_TUSB_MCU and CFG_TUSB_OS should be defined in platformio.ini.
#endif
#define CFG_TUD_ENDPOINT0_SIZE      64

#define CFG_TUD_ENABLED 1
#define CFG_TUSB_RHPORT0_MODE    (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
// Explicitly disable VBUS sensing if your board doesn't have it.
// If TinyUSB thinks VBUS is low, it will NEVER pull D+ high.
#define CFG_TUD_VBUS_SENSE_PIN 0

// --- Enabled Classes (Set to 1 for all potential classes) ---
// We force all enabled and override the FIFO addresses based on our dynamic configuration later.
#define CFG_TUD_CDC                 1
#define CFG_TUD_MSC                 1
#define CFG_TUD_HID                 1
#define CFG_TUD_MIDI                1
#define CFG_TUD_AUDIO               0
#define CFG_TUD_VENDOR              0 // We use HID for logging, not Vendor class

// --- CDC Settings (Logging/Debug) ---
#define CFG_TUD_CDC_EP_BUFSIZE      32
#define CFG_TUD_CDC_RX_BUFSIZE      32
#define CFG_TUD_CDC_TX_BUFSIZE      32

// --- MSC Settings ---
#define CFG_TUD_MSC_EP_BUFSIZE      64 /// TODO: is this number ok?

// --- HID Settings ---
// This buffer must be large enough for your biggest report (Gamepad/Keyboard)
#define CFG_TUD_HID_EP_BUFSIZE      64

// --- MIDI Settings ---
// Should allow for 128/4(packet size) = 32 notes at once. Or 2 notes per cable at once for 16 cables.
#define CFG_TUD_MIDI_EP_BUFSIZE     64
#define CFG_TUD_MIDI_RX_BUFSIZE     CFG_TUD_MIDI_EP_BUFSIZE
#define CFG_TUD_MIDI_TX_BUFSIZE     CFG_TUD_MIDI_EP_BUFSIZE

// --- Audio (UAC2) Settings ---
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE                              48000

#define CFG_TUD_AUDIO_ENABLE_EP_IN                                    1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX                    2                                       // Driver gets this info from the descriptors - we define it here to use it to setup the descriptors and to do calculations with it below
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                            1                                       // Driver gets this info from the descriptors - we define it here to use it to setup the descriptors and to do calculations with it below - be aware: for different number of channels you need another descriptor!
#define CFG_TUD_AUDIO_EP_SZ_IN                                        TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX                             CFG_TUD_AUDIO_EP_SZ_IN
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ                          (TUD_OPT_HIGH_SPEED ? 32 : 4) * CFG_TUD_AUDIO_EP_SZ_IN // Example write FIFO every 1ms, so it should be 8 times larger for HS device

// --- FIFO Buffer Settings (ESP32 Specific) ---
// Since we are pushing 6 endpoints, we need to ensure the hardware FIFO
// is distributed correctly. TinyUSB handles most of this on the ESP32,
// but keeping buffers small (64-256) helps fit everything.

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
