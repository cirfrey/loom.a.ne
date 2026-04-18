#include "lm/strands/usbd.hpp"

#include "lm/core/veil.hpp"

#include "lm/chip/usb.hpp"

#include "lm/config.hpp"
#include "lm/log.hpp"

#include "lm/usb/debug.hpp"
#include "lm/usb/backend.hpp"

#include <tusb.h>

namespace lm::strands
{
    static lm::strands::usbd* usbd_instance = nullptr;
}

#ifdef TUP_USBIP_DWC2
#include "portable/synopsys/dwc2/dwc2_common.h"
// Helper to get size in WORDS (Bytes / 4, rounded up)
constexpr lm::u16 get_ep_word_size(lm::usbd::ept_t type) {
    lm::u16 bytes = 0;
    switch (type) {
        case lm::usbd::ept_t::control:           bytes = 64; break;
        case lm::usbd::ept_t::cdc_bulk_in:       bytes = CFG_TUD_CDC_EP_BUFSIZE; break;
        case lm::usbd::ept_t::cdc_interrupt_in:  bytes = 16; break;
        case lm::usbd::ept_t::hid_interrupt_in:  bytes = CFG_TUD_HID_EP_BUFSIZE; break;
        case lm::usbd::ept_t::midi_bulk_in:      bytes = CFG_TUD_MIDI_EP_BUFSIZE; break;
        case lm::usbd::ept_t::msc_bulk_in:       bytes = CFG_TUD_MSC_EP_BUFSIZE; break;
        case lm::usbd::ept_t::uac2_iso_in:       bytes = CFG_TUD_AUDIO_EP_SZ_IN; break;
        case lm::usbd::ept_t::uac2_feedback:     bytes = 8; break;
        default: return 0;
    }
    return (bytes + 3) / 4;
}

// Returns the last sucessful EP.
auto apply_dynamic_fifo_allocation(std::span<lm::usbd::ep_t> eps) -> lm::st {
    using namespace lm;

    // Access the registers via the driver's own abstraction
    // On ESP32-S3, rhport is usually 0.
    dwc2_regs_t* dwc2 = DWC2_REG(0);

    u16 current_word_offset = 0;
    const u16 MAX_FIFO_WORDS = 256;

    // 1. Global RX FIFO
    // Shared by all OUT endpoints. 48 words is a safe starting point.
    u16 rx_words = 128;
    dwc2->grxfsiz = rx_words;
    current_word_offset += rx_words;

    for (size_t i = 0; i < eps.size(); ++i) {
        const auto& ep = eps[i];

        if (ep.in == usbd::ept_t::unassigned ||
            ep.in == usbd::ept_t::unavailable) {
            continue;
        }

        u16 ep_words = get_ep_word_size(ep.in);

        if (current_word_offset + ep_words > MAX_FIFO_WORDS) {
            return 0;
        }

        // 2. EP0 (Control IN) uses the Non-periodic TX FIFO register
        if (i == 0) {
            dwc2->gnptxfsiz = (ep_words << 16) | current_word_offset;
        }
        // 3. EP1-EP6 use the dedicated Device IN Endpoint TX FIFO array
        else {
            // dieptxf[0] maps to EP1, dieptxf[1] to EP2, etc.
            dwc2->dieptxf[i - 1] = (ep_words << 16) | current_word_offset;
        }

        current_word_offset += ep_words;
    }

    while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) fabric::strand::sleep_ms(1);

    // 0x10 is the magic value for "Flush All TX FIFOs" in the DWC2 core.
    // This resets the pointers for both EP0 (gnptxfsiz) and EP1-6 (dieptxf).
    dfifo_flush_tx(dwc2, 0x10);
    while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) fabric::strand::sleep_ms(1);

    // Resets the pointer for the Global RX FIFO.
    dfifo_flush_rx(dwc2);
    while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) fabric::strand::sleep_ms(1);


    return current_word_offset;
}
#endif

lm::strands::usbd::usbd(fabric::strand_runtime_info& info)
{
    usbd_instance = this;

    auto lens = usb::backend::setup_descriptors(
        config_descriptor,
        string_descriptors,
        device_descriptor,
        config.usb,
        config.audio.backend.usb,
        config.cdc.backend.usb,
        config.hid.backend.usb,
        config.midi.backend.usb,
        config.msc.backend.usb
    );
    config_descriptor_size = lens.total;

    log::debug<128 * 3>(
        "Config descriptor len && endpoint map report.\n"
        "\t+--------+-----+-----+------+-----+-------+-------+\n"
        "\t| Header | CDC | HID | MIDI | MSC | AUDIO | Total |\n"
        "\t+--------+-----+-----+------+-----+-------+-------+\n"
        "\t| %-6u | %-3u | %-3u | %-4u | %-3u | %-5u | %-5u |\n"
        "\t+--------+-----+-----+------+-----+-------+-------+\n",
        lens.header, lens.cdc, lens.hid, lens.midi, lens.msc, lens.audio, lens.total
    );
    auto printer = [](auto fmt, auto... args){ log::debug<128>(
        log::fmt_t({ .fmt = fmt, .timestamp = log::no_timestamp, .filename = log::no_filename }),
        veil::forward<decltype(args)>(args)...
    ); };
    usb::debug::print_ep_table(config.usb.endpoints, printer, {"\t", 1});

    chip::usb::phy::power_up();
}

auto lm::strands::usbd::on_ready() -> fabric::managed_strand_status
{
    tud_strand_handle = fabric::strand::create(_config::strand::tud, {}, [](void* p){
        auto self = (usbd*)p;
        tusb_init();
        #ifdef TUP_USBIP_DWC2
            auto eps = apply_dynamic_fifo_allocation(self->endpoints);
            log::warn("apply_dynamic_fifo_allocation allocated %zu words\n", eps);
        #endif
        while(1) tud_task();
    }, this);

    return fabric::managed_strand_status::ok;
}

auto lm::strands::usbd::before_sleep() -> fabric::managed_strand_status
{ return fabric::managed_strand_status::ok; }

auto lm::strands::usbd::on_wake() -> fabric::managed_strand_status
{
    return fabric::managed_strand_status::ok;
}

lm::strands::usbd::~usbd() {
    if(tud_strand_handle) {
        fabric::strand::reap(tud_strand_handle);
        tusb_deinit(0);
    }
}

extern "C" {

// Device Descriptor.
auto tud_descriptor_device_cb() -> lm::u8 const*
{
    if(!lm::strands::usbd_instance) return nullptr;
    return (lm::u8 const*) &lm::strands::usbd_instance->device_descriptor;
}

// Configuration Descriptor.
auto tud_descriptor_configuration_cb(uint8_t index) -> lm::u8 const*
{
    if(!lm::strands::usbd_instance) return nullptr;
    return lm::strands::usbd_instance->config_descriptor;
}

// String Descriptors.
auto tud_descriptor_string_cb(lm::u8 index, lm::u16 langid) -> lm::u16 const*
{
    using namespace lm;

    if(!strands::usbd_instance) return nullptr;

    // Shared buffer for string descriptors
    static u16 _desc_str[config_t::usbcommon::string_descriptor_max_len];

    // --- Index 0 is NOT a string, it's the Language ID list ---
    if (index == (u8)usb::string_descriptor::lang)
    {
        static u16 const lang_id[] = { (u16)((TUSB_DESC_STRING << 8) | 0x04), 0x0409 };
        return lang_id;
    }

    // Helper to convert UTF-8 (char*) to UTF-16 (uint16_t)
    auto utf8_to_utf16 = [](const char* str) -> u16* {
        if (!str) str = "";

        u16 chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        // First byte is Length (Total bytes including header), Second is Type (0x03)
        // In TinyUSB, we return this as a uint16_t array where the first element
        // packs these two bytes: (0x03 << 8) | (Length)
        _desc_str[0] = (uint16_t)((0x03 << 8) | (2 * chr_count + 2));

        // Convert ASCII/UTF8 chars to UTF16 (simple cast for standard ASCII)
        for (uint8_t i = 0; i < chr_count; i++) { _desc_str[1 + i] = str[i]; }

        return _desc_str;
    };
    // Return the UTF-16 string for the given index
    return utf8_to_utf16( strands::usbd_instance->string_descriptors[index] );
}

void tud_mount_cb(void) {}
void tud_umount_cb(void) {}
void tud_suspend_cb(bool remote_wakeup_en) {}
void tud_resume_cb(void) {}

} // extern "C"

// --- CDC ---
// TODO: Fixme! (use static instance)
extern "C"
{
    void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
        (void) itf; (void) dtr; (void) rts;
    }
    void tud_cdc_rx_cb(uint8_t itf) { (void) itf; }
}

// --- HID ---
#include "lm/lib/hid.hpp"
// TODO: Fixme! (use static instance)
extern "C"
{
    uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf) {
        (void) itf;
        return lm::hid::report_descriptor;
    }

    uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
        (void) itf; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
        return 0;
    }

    void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
        (void) itf; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
    }

    bool tud_hid_set_idle_cb(uint8_t itf, uint8_t idle_rate) {
        (void) itf; (void) idle_rate;
        return true;
    }
}

// --- AUDIO ---
// TODO: Fixme! (use static instance)
// TODO: Also heavy refactor needed! AUDIO is not ready for user customization yet like the other capabilities.
#if CFG_TUD_AUDIO

// Audio controls
// Current states
bool mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1]; 				          // +1 for master channel 0
uint16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1]; 					// +1 for master channel 0
uint32_t sampFreq;
uint8_t clkValid;

// Range states
audio20_control_range_2_n_t(1) volumeRng[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX+1]; 			// Volume range state
audio20_control_range_4_n_t(1) sampleFreqRng; 						// Sample frequency range state

// Audio test data
uint16_t test_buffer_audio[(CFG_TUD_AUDIO_EP_SZ_IN - 2) / 2];
uint16_t startVal = 0;

// Call this function in your main setup BEFORE lm::usbd::init() / tud_init()
void audio_state_init() {
    // 1. Set the Current Frequency (Must match your descriptor math!)
    sampFreq = 48000;

    // 2. Mark Clock as Valid (Critical for Windows)
    clkValid = 1;

    // 3. Define Supported Range (Critical for Windows "Advanced" Tab)
    // Format: [Number of Ranges] (Lower) (Upper) (Resolution)
    sampleFreqRng.wNumSubRanges = 1;
    sampleFreqRng.subrange[0].bMin = 48000;
    sampleFreqRng.subrange[0].bMax = 48000;
    sampleFreqRng.subrange[0].bRes = 0; // 0 means "Discrete" or "No stepping"

    // 4. Initialize Volume (Optional, but good practice)
    // Set all channels to 0dB (0x0000) and Unmuted
    for(int i=0; i < CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1; i++) {
        volume[i] = 0x0000; // 0dB
        mute[i] = 0;        // Unmuted

        // Initialize Volume Ranges (e.g. -90dB to 0dB)
        volumeRng[i].wNumSubRanges = 1;
        volumeRng[i].subrange[0].bMin = -90 * 256; // -90 dB
        volumeRng[i].subrange[0].bMax = 0;         // 0 dB
        volumeRng[i].subrange[0].bRes = 1 * 256;   // 1 dB steps
    }
}

extern "C" {

bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff){
  (void) rhport;
  (void) pBuff;
  // We do not support any set range requests here, only current value requests
  TU_VERIFY(p_request->bRequest == AUDIO20_CS_REQ_CUR);
  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t ep = TU_U16_LOW(p_request->wIndex);
  (void) channelNum; (void) ctrlSel; (void) ep;
  return false; 	// Yet not implemented
}

// Invoked when audio class specific set request received for an interface
bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff){
  (void) rhport;
  (void) pBuff;
  // We do not support any set range requests here, only current value requests
  TU_VERIFY(p_request->bRequest == AUDIO20_CS_REQ_CUR);
  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t itf = TU_U16_LOW(p_request->wIndex);
  (void) channelNum; (void) ctrlSel; (void) itf;
  return false; 	// Yet not implemented
}

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff){
  (void) rhport;
  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t itf = TU_U16_LOW(p_request->wIndex);
  uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
  (void) itf;
  // We do not support any set range requests here, only current value requests
  TU_VERIFY(p_request->bRequest == AUDIO20_CS_REQ_CUR);
  // If request is for our feature unit
  if ( entityID == 2 ){
    switch ( ctrlSel ){
      case AUDIO20_FU_CTRL_MUTE:
        // Request uses format layout 1
        TU_VERIFY(p_request->wLength == sizeof(audio20_control_cur_1_t));
        mute[channelNum] = ((audio20_control_cur_1_t*) pBuff)->bCur;
        TU_LOG2("    Set Mute: %d of channel: %u\r\n", mute[channelNum], channelNum);
      return true;
      // --
      case AUDIO20_FU_CTRL_VOLUME:
        // Request uses format layout 2
        TU_VERIFY(p_request->wLength == sizeof(audio20_control_cur_2_t));
        volume[channelNum] = (uint16_t) ((audio20_control_cur_2_t*) pBuff)->bCur;
        TU_LOG2("    Set Volume: %d dB of channel: %u\r\n", volume[channelNum], channelNum);
      return true;
        // Unknown/Unsupported control
      default:
        TU_BREAKPOINT();
      return false;
    }
  }
  return false;    // Yet not implemented
}
// Invoked when audio class specific get request received for an EP
bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request){
  (void) rhport;
  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t ep = TU_U16_LOW(p_request->wIndex);
  (void) channelNum; (void) ctrlSel; (void) ep;
  //	return tud_control_xfer(rhport, p_request, &tmp, 1);
  return false; 	// Yet not implemented
}

// Invoked when audio class specific get request received for an interface
bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request){
  (void) rhport;
  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t itf = TU_U16_LOW(p_request->wIndex);
  (void) channelNum; (void) ctrlSel; (void) itf;
  return false; 	// Yet not implemented
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request){
  (void) rhport;
  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since we have only one audio function implemented, we do not need the itf value
  uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
  // Input terminal (Microphone input)
  if (entityID == 1){
    switch ( ctrlSel ){
      case AUDIO20_TE_CTRL_CONNECTOR:{
        // The terminal connector control only has a get request with only the CUR attribute.
        audio20_desc_channel_cluster_t ret;
        // Those are dummy values for now
        ret.bNrChannels = 1;
        ret.bmChannelConfig = (audio20_channel_config_t) 0;
        ret.iChannelNames = 0;
        TU_LOG2("    Get terminal connector\r\n");
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*) &ret, sizeof(ret));
      }
      break;
        // Unknown/Unsupported control selector
      default:
        TU_BREAKPOINT();
        return false;
    }
  }
  // Feature unit
  if (entityID == 2){
    switch ( ctrlSel ){
      case AUDIO20_FU_CTRL_MUTE:
        // Audio control mute cur parameter block consists of only one byte - we thus can send it right away
        // There does not exist a range parameter block for mute
        TU_LOG2("    Get Mute of channel: %u\r\n", channelNum);
        return tud_control_xfer(rhport, p_request, &mute[channelNum], 1);
      case AUDIO20_FU_CTRL_VOLUME:
        switch ( p_request->bRequest ){
          case AUDIO20_CS_REQ_CUR:
            TU_LOG2("    Get Volume of channel: %u\r\n", channelNum);
            return tud_control_xfer(rhport, p_request, &volume[channelNum], sizeof(volume[channelNum]));
          case AUDIO20_CS_REQ_RANGE:
            TU_LOG2("    Get Volume range of channel: %u\r\n", channelNum);
            // Copy values - only for testing - better is version below
            audio20_control_range_2_n_t(1)
            ret;
            ret.wNumSubRanges = 1;
            ret.subrange[0].bMin = -90;    // -90 dB
            ret.subrange[0].bMax = 90;		// +90 dB
            ret.subrange[0].bRes = 1; 		// 1 dB steps
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*) &ret, sizeof(ret));
            // Unknown/Unsupported control
          default:
            TU_BREAKPOINT();
            return false;
        }
      break;
        // Unknown/Unsupported control
      default:
        TU_BREAKPOINT();
        return false;
    }
  }
  // Clock Source unit
  if ( entityID == 4 ){
    switch ( ctrlSel ){
      case AUDIO20_CS_CTRL_SAM_FREQ:
        // channelNum is always zero in this case
        switch ( p_request->bRequest ){
          case AUDIO20_CS_REQ_CUR:
            TU_LOG2("    Get Sample Freq.\r\n");
            return tud_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));
          case AUDIO20_CS_REQ_RANGE:
            TU_LOG2("    Get Sample Freq. range\r\n");
            return tud_control_xfer(rhport, p_request, &sampleFreqRng, sizeof(sampleFreqRng));
          // Unknown/Unsupported control
          default:
            TU_BREAKPOINT();
            return false;
        }
      break;
      case AUDIO20_CS_CTRL_CLK_VALID:
        // Only cur attribute exists for this request
        TU_LOG2("    Get Sample Freq. valid\r\n");
        return tud_control_xfer(rhport, p_request, &clkValid, sizeof(clkValid));
      // Unknown/Unsupported control
      default:
        TU_BREAKPOINT();
        return false;
    }
  }
  TU_LOG2("  Unsupported entity: %d\r\n", entityID);
  return false; 	// Yet not implemented
}
bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting){
  (void) rhport;
  (void) itf;
  (void) ep_in;
  (void) cur_alt_setting;
  tud_audio_write ((uint8_t *)test_buffer_audio, CFG_TUD_AUDIO_EP_SZ_IN - 2);
  return true;
}
bool tud_audio_tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting){
  (void) rhport;
  (void) n_bytes_copied;
  (void) itf;
  (void) ep_in;
  (void) cur_alt_setting;
  for (size_t cnt = 0; cnt < (CFG_TUD_AUDIO_EP_SZ_IN - 2) / 2; cnt++){
    test_buffer_audio[cnt] = startVal++;
  }
  return true;
}
bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const * p_request){
  (void) rhport;
  (void) p_request;
  startVal = 0;
  return true;
}

} // extern "C"

#endif

// --- MIDI ---
extern "C"
{
    void tud_midi_rx_cb(uint8_t itf) { (void) itf; }
}


// --- MSC ---

#include "lm/log.hpp"
#include "lm/chip/memory.hpp"
// TODO: This really needs to go somewhere else. Probably lm::hook::arch_config.
static lm::chip::memory::storage* static_storage = nullptr;
void init_msc_partition() {
    auto storages = lm::chip::memory::get_storages();

    if (storages.empty()) {
        lm::log::error("No storages found! Check flash/HAL settings.\n");
        return;
    }

    lm::log::debug("Listing all storages...\n");
    for(auto& s : storages)
    {
        lm::log::debug(
            "Found: [%.*s] Type: 0x%02x Subtype: 0x%02x Size: %zu bytes\n",
            (int)s.label.size, s.label.data,
            s.type, s.subtype, s.size
        );

        if (s.label == "static") {
            static_storage = &s;
        }
    }
}


extern "C" {

// Mandatory: Max LUN (0-based)
uint8_t tud_msc_get_max_lun_cb(void) {
    return 0; // Only one drive
}

// Mandatory: Inquiry (8, 16, 4 bytes)
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;
    // IMPORTANT: Note the explicit lengths and padding
    memcpy(vendor_id,   "LoomANe ", 8);
    memcpy(product_id,  "Static-Flash    ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

// Mandatory: Test Unit Ready
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    return (static_storage != nullptr);
}

// Mandatory: Capacity
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void) lun;
    if (static_storage) {
        // ESP32 flash sectors are typically 4096 bytes,
        // but USB MSC usually expects 512-byte logical blocks.
        *block_size  = static_storage->sector_size;
        *block_count = static_storage->size / (*block_size);
    } else {
        *block_count = 0;
        *block_size  = 0;
    }
}

// Mandatory: Read10
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void) lun;
    if (static_storage == nullptr) return -1;

    // Calculate the flash address: (LBA * block_size) + offset
    // TinyUSB usually gives offset=0 and bufsize as a multiple of block_size
    uint32_t addr = (lba * static_storage->sector_size) + offset;

    auto err = static_storage->read(addr, {buffer, bufsize});
    return (err == lm::chip::memory::storage_op_status::ok) ? (int32_t)bufsize : -1;
}

// Optional but recommended: Mark as Write Protected
bool tud_msc_is_writable_cb(uint8_t lun) {
    (void) lun;
    return false; // Tells the host OS the drive is Read-Only
}

// Mandatory: Write10
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void) lun; (void) lba; (void) offset; (void) buffer;
    return -1;
}

// Mandatory: SCSI Start/Stop/Custom
// NOTE: scsi_cmd is a pointer to the 16-byte buffer
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize;
    return -1; // -1 means "let TinyUSB handle standard SCSI commands"
}

} // extern "C"
