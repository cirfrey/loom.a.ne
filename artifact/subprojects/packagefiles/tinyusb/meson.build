project('tinyusb', 'c', version: '0.20.0')

tinyusb_sources = files(
    'src/class/audio/audio_device.c',
    'src/class/bth/bth_device.c',
    'src/class/cdc/cdc_device.c',
    'src/class/cdc/cdc_host.c',
    'src/class/cdc/cdc_rndis_host.c',
    'src/class/dfu/dfu_device.c',
    'src/class/dfu/dfu_rt_device.c',
    'src/class/hid/hid_device.c',
    'src/class/hid/hid_host.c',
    'src/class/midi/midi_device.c',
    'src/class/midi/midi_host.c',
    'src/class/msc/msc_device.c',
    'src/class/msc/msc_host.c',
    'src/class/mtp/mtp_device.c',
    'src/class/net/ecm_rndis_device.c',
    'src/class/net/ncm_device.c',
    'src/class/usbtmc/usbtmc_device.c',
    'src/class/vendor/vendor_device.c',
    'src/class/vendor/vendor_host.c',
    'src/class/video/video_device.c',
    'src/common/tusb_fifo.c',
    'src/device/usbd.c',
    'src/device/usbd_control.c',
    'src/host/hub.c',
    'src/host/usbh.c',
    'src/tusb.c',
    'src/typec/usbc.c',
)

portabilities = {
    'none': [],
    'synopsys-dwc2': files(
        'src/portable/synopsys/dwc2/dcd_dwc2.c',
        'src/portable/synopsys/dwc2/dcd_common.c',
        'src/portable/synopsys/dwc2/hcd_dwc2.c',
    ),
}

tinyusb_sources += portabilities[get_option('portability')]

tinyusb_dep = declare_dependency(
  include_directories: include_directories('src'),
  sources: tinyusb_sources
)
