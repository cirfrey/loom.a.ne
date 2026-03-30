#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define TUP_DCD_ENPOINT_MAX 6

#include "tusb_option.h"
#include "sdkconfig.h"

// --- Common Configuration ---
/// TODO: Add support to ESP32S3
#define CFG_TUSB_MCU                OPT_MCU_ESP32S2 // Or S3 depending on your board
#define CFG_TUD_ENDPOINT0_SIZE      64
#define CFG_TUSB_OS                 OPT_OS_FREERTOS

#define CFG_TUD_ENABLED 1
#define CFG_TUSB_RHPORT0_MODE    (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
// Explicitly disable VBUS sensing if your board doesn't have it.
// If TinyUSB thinks VBUS is low, it will NEVER pull D+ high.
#define CFG_TUD_VBUS_SENSE_PIN 0

// --- Enabled Classes (Set to 1 for all potential classes) ---
#define CFG_TUD_CDC                 1
#define CFG_TUD_MSC                 1
#define CFG_TUD_HID                 1
#define CFG_TUD_MIDI                1
#define CFG_TUD_AUDIO               1
#define CFG_TUD_VENDOR              0 // We use HID for logging, not Vendor class

// --- CDC Settings (Logging/Debug) ---
#define CFG_TUD_CDC_EP_BUFSIZE      64
#define CFG_TUD_CDC_RX_BUFSIZE      128
#define CFG_TUD_CDC_TX_BUFSIZE      256

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
#define CFG_TUD_AUDIO_FUNC_COUNT    1
// Max channels you'll support (Stereo Out = 2)
#define CFG_TUD_AUDIO_MAX_CHANNELS  2
// Allocation for Isochronous packets (1ms @ 48kHz Stereo 16-bit = 192 bytes)
//#define CFG_TUD_AUDIO_EP_SZ_OUT     256

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                                 TUD_AUDIO_MIC_ONE_CH_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ                              64                                      // Size of control request buffer
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE                              48000

#define CFG_TUD_AUDIO_ENABLE_EP_IN                                    1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX                    2                                       // Driver gets this info from the descriptors - we define it here to use it to setup the descriptors and to do calculations with it below
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                            1                                       // Driver gets this info from the descriptors - we define it here to use it to setup the descriptors and to do calculations with it below - be aware: for different number of channels you need another descriptor!
#define CFG_TUD_AUDIO_EP_SZ_IN                                        TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
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
