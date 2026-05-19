TODO: add a row telling if its a platform API or if it needs a install

TODO: add libraries to meson.build and also figure out the meson dependency() fallbacks for each library.

NOTE: these are all from the perspective of the loomane framework so an I (input) means we inject data into the framework and an O (output) means we push out data from the framework. I am aware we can both push and pull data using libusb, but this is specifically about loom.a.ne (the framework).

QUESTION: Im not entirely sure what we'd use NET for but it sure is interesting and maybe worth keeping our options in mind.

<h1>USB Capability Map</h1>
<p class="subtitle">Per-platform techniques and libraries for presenting or consuming USB device classes</p>

## Legend

- I: Input — steal / consume an existing device/data.
- O: Output — present / create a virtual device/data stream.
- ~: Partial — works with caveats.
- X: Dead end/undesirable — no viable path/too much work or money.
- 🚧: In construction/desirable.
- 🗃️: Planned/desirable (may need research or a lot of work).
- ?: Unsure if desirable.

# Embedded

<table>
  <thead><tr>
    <th colspan="2"></th>
    <th>MIDI</th><th>Audio</th><th>Video</th>
    <th>CDC</th><th>HID</th><th>MSC</th>
    <th>NET</th><th>Vendor</th>
    <th colspan="2">Notes</th>
  </tr></thead>
  <tbody>
  <tr>
    <td>TinyUSB</td>
    <td>🚧</td>
    <td>I/O</td><td>I/O</td><td>I/O</td>
    <td>I/O</td><td>I/O</td><td>I/O</td>
    <td>I/O</td><td>I/O</td>
    <td colspan="2">All USB device classes; NET via CDC-ECM / CDC-NCM / RNDIS; WebUSB additive on Vendor class (BOS descriptor + landing page URL)</td>
  </tr>
  <tr>
    <td>USB/IP Server</td>
    <th>🚧</th>
    <td>O</td><td>O</td><td>O</td>
    <td>O</td><td>O</td><td>O</td>
    <td>O</td><td>O</td>
    <td colspan="2"></td>
  </tr>
  <tr>
    <td>Chip BLE stack</td>
    <td>🗃️</td>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Platform-dependent: NimBLE (ESP32, nRF52 — Apache 2.0), BTstack (RP2040 — GPL/commercial), SoftDevice (nRF52 — proprietary closed binary), ST wireless stack (STM32WB — proprietary)</td>
  </tr>
  </tbody>
</table>

# Linux

<table>
  <thead><tr>
    <th colspan="2"></th>
    <th>MIDI</th><th>Audio</th><th>Video</th>
    <th>CDC</th><th>HID</th><th>MSC</th>
    <th>NET</th><th>Vendor</th>
    <th colspan="2">Notes</th>
  </tr></thead>
  <tbody>
  <tr>
    <td>libusb + libudev</td>
    <th>🗃️</th>
    <td>I</td><td>I</td><td>I</td>
    <td>I</td><td>I</td><td>I</td>
    <td>I</td><td>I</td>
    <td colspan="2">Device stealing + hotplug detection via netlink; does not create virtual devices</td>
  </tr>
  <tr>
    <td>USB/IP Server</td>
    <th>🚧</th>
    <td>O</td><td>O</td><td>O</td>
    <td>O</td><td>O</td><td>O</td>
    <td>O</td><td>O</td>
    <td colspan="2">Requires a USB/IP client on the other end; vhci-hcd can be loaded programmatically (root) on the client to import devices into the local OS</td>
  </tr>
  <tr>
    <td>dummy_hcd + ConfigFS / libusbgx</td>
    <th>🗃️</th>
    <td>O</td><td>~O</td><td>O</td>
    <td>O</td><td>O</td><td>O</td>
    <td>O</td><td>O</td>
    <td colspan="2">Isochronous audio unreliable on dummy_hcd; use a real UDC (Pi Zero, BeagleBone, etc.) for audio; root required to load modules</td>
  </tr>
  <tr>
    <td>SimpleBLE</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Requires BT hardware; O=advertise as BLE MIDI peripheral (CBPeripheral-style via BlueZ GATT server); I=connect to BLE MIDI devices</td>
  </tr>
  <tr>
    <td>RTP-MIDI (rtpmidid)</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Network MIDI over RTP/UDP; requires rtpmidid daemon; exposes ALSA sequencer ports</td>
  </tr>
  <tr>
    <td>libasound</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td>I/O</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td>MIDI via snd-virmidi; Audio via snd-aloop</td>
    <td rowspan="2">Can create virtual sinks/sources OR use existing mic/speaker from the system. User must be in the audio group. Monitor sources available for loopback (read system output as input stream).</td>
  </tr>
  <tr>
    <td>libpipewire</td>
    <th>🗃️</th>
    <td></td>
    <td>I/O</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td>Pre-installed on Ubuntu 22.04+ / Fedora 34+; fallback to libasound if absent</td>
  </tr>
  <tr>
    <td>v4l2loopback</td>
    <th>🗃️</th>
    <td></td><td></td>
    <td>O</td>
    <td></td><td></td><td></td>
    <td></td><td></td>
    <td colspan="2">modprobe v4l2loopback; creates /dev/videoN loopback devices; root required</td>
  </tr>
  <tr>
    <td>POSIX PTY pair</td>
    <th>🗃️</th>
    <td></td><td></td><td></td>
    <td>O</td>
    <td></td><td></td><td></td><td></td>
    <td colspan="2">posix_openpt() → grantpt() → unlockpt(); appears as /dev/pts/N to any application; no root required</td>
  </tr>
  <tr>
    <td>libevdev</td>
    <th>🗃️</th>
    <td></td><td></td><td></td><td></td>
    <td>O</td>
    <td></td><td></td><td></td>
    <td colspan="2">Virtual input devices via uinput; keyboard, mouse, gamepad; user must be in input group or root</td>
  </tr>
  <tr>
    <td>loop device / ublk</td>
    <th>🗃️</th>
    <td></td><td></td><td></td>
    <td></td><td></td>
    <td>O</td>
    <td></td><td></td>
    <td colspan="2">loop: losetup + /dev/loop-control ioctl, root required; ublk: userspace block driver, kernel 6.0+, root required</td>
  </tr>
  <tr>
    <td>TUN/TAP (ioctl)</td>
    <th>?</th>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td>O</td>
    <td></td>
    <td colspan="2">/dev/net/tun; TUN=L3/IP packets, TAP=L2/Ethernet frames; root required</td>
  </tr>
  </tbody>
</table>

# macOS

<table>
  <thead><tr>
    <th colspan="2"></th>
    <th>MIDI</th><th>Audio</th><th>Video</th>
    <th>CDC</th><th>HID</th><th>MSC</th>
    <th>NET</th><th>Vendor</th>
    <th colspan="2">Notes</th>
  </tr></thead>
  <tbody>
  <tr>
    <td>libusb + IOKit (framework)</td>
    <th>🗃️</th>
    <td>I</td><td>I</td><td>I</td>
    <td>I</td><td>I</td><td>I</td>
    <td>I</td><td>I</td>
    <td colspan="2">Device stealing via libusb; IOKit for hotplug detection</td>
  </tr>
  <tr>
    <td>CoreMIDI (framework)</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Virtual MIDI sources/destinations; no user action required; programmatic via MIDISourceCreate / MIDIDestinationCreate</td>
  </tr>
  <tr>
    <td>RTP-MIDI (CoreMIDI)</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Built-in via CoreMIDI network sessions; no install; discoverable via Bonjour on LAN</td>
  </tr>
  <tr>
    <td>SimpleBLE (CoreBluetooth)</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">BLE MIDI; wraps CoreBluetooth; requires BT hardware; O=CBPeripheralManager, I=CBCentralManager</td>
  </tr>
  <tr>
    <td>CoreAudio — existing mic/speaker</td>
    <th>🗃️</th>
    <td></td>
    <td>I/O</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">
          - Mic: NSMicrophoneUsageDescription privacy prompt;
      <br>- Speaker output: no permission. Loopback capture via ScreenCaptureKit (macOS 13+, screen recording permission). No loopback path on older macOS without a virtual device installed.</td>
  </tr>
  <tr>
    <td>Virtual Audio (BlackHole-based)</td>
    <th>X</th>
    <td></td>
    <td>I/O</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Fork BlackHole (MIT licensed); build a signed CoreAudio HAL plugin; install to /Library/Audio/Plug-Ins/HAL/ (admin) or ~/Library/... (no admin, re-login required); Apple Developer ID + notarization ($99/yr)</td>
  </tr>
  <tr>
    <td>POSIX PTY pair</td>
    <th>🗃️</th>
    <td></td><td></td><td></td>
    <td>O</td>
    <td></td><td></td><td></td><td></td>
    <td colspan="2">Same as Linux; macOS is POSIX compliant; appears as /dev/ttys/N</td>
  </tr>
  <tr>
    <td>hdiutil / DiskArbitration (framework)</td>
    <th>🗃️</th>
    <td></td><td></td><td></td>
    <td></td><td></td>
    <td>O</td>
    <td></td><td></td>
    <td colspan="2">Virtual disk image via hdiutil subprocess or DiskArbitration framework; no admin required</td>
  </tr>
  <tr>
    <td>utun (PF_SYSTEM socket)</td>
    <th>?</th>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td>O</td>
    <td></td>
    <td colspan="2">Virtual TUN interface via PF_SYSTEM socket; no entitlement required; mechanism used by WireGuard on macOS</td>
  </tr>
  <tr>
    <td>*Unsupported*</td>
    <th>X</th>
    <td></td>
    <td></td>
    <td>X</td>
    <td></td>
    <td>X</td>
    <td></td>
    <td></td>
    <td></td>
    <td colspan="2">Dead end
      <br>- Video: CoreMediaIO Extension (macOS 12.3+) requires Apple entitlement + notarization; not viable for general distribution
      <br>- HID: DriverKit + com.apple.developer.driverkit.family.hid.device entitlement required; Apple approval needed; not viable for general distribution
      </td>
  </tr>
  </tbody>
</table>


# Windows

<table>
  <thead><tr>
    <th colspan="2"></th>
    <th>MIDI</th><th>Audio</th><th>Video</th>
    <th>CDC</th><th>HID</th><th>MSC</th>
    <th>NET</th><th>Vendor</th>
    <th colspan="2">Notes</th>
  </tr></thead>
  <tbody>
  <tr>
    <td>libusb + WinUSB</td>
    <th>🗃️</th>
    <td>I</td><td>I</td><td>I</td>
    <td>I</td><td>I</td><td>I</td>
    <td>I</td><td>I</td>
    <td colspan="2">Device stealing; WinUSB filter installed per-device; MS OS 2.0 descriptors can auto-load WinUSB from the device side without any INF
      <br>May need `libwdi` to force windows to use `WinUSB` driver (DeepSeek):
      <br>- For libusb to control a device, that device must be bound to a generic, user-mode accessible driver (like WinUSB, libusbK, or libusb0.sys), not its default class driver (e.g., USBSTOR for flash drives, usbhid for keyboards, or usbccgp for composite devices). If a device is using the wrong driver, libusb calls like libusb_open() or libusb_claim_interface() will fail, usually with a LIBUSB_ERROR_ACCESS.
      The standard solution is to use Zadig to force the device to bind to a compatible driver like WinUSB before your program can run.
      <br>- The driver you bind the device to significantly impacts reliability. The current recommendation is WinUSB > libusbK > libusb0.sys. WinUSB is the most actively maintained and broadly compatible choice, while libusb0.sys has known issues and is often not recommended for new projects.
    </td>
  </tr>
  <tr>
    <td>USB/IP Server</td>
    <th>🚧</th>
    <td>O</td><td>O</td><td>O</td>
    <td>O</td><td>O</td><td>O</td>
    <td>O</td><td>O</td>
    <td colspan="2">Needs a USB/IP client with a signed VHCI kernel driver; imports devices from our USB/IP server; requires MSI install (one-time); github.com/dorssel/usbipd-win</td>
  </tr>
  <tr>
    <td>WinMM</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Client API only; cannot create virtual MIDI ports; available on all Windows versions</td>
  </tr>
  <tr>
    <td>Windows MIDI Services</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Win 11 22H2+ only; supports virtual MIDI ports natively; no install required; supersedes WinMM for modern apps</td>
  </tr>
  <tr>
    <td>virtualMIDI SDK</td>
    <th>?</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Virtual MIDI ports visible to all apps; requires signed driver install; tobias-erichsen.de/software/virtualmidi<br><br>Unsure, since theres licensing things and stuff.</td>
  </tr>
  <tr>
    <td>RTP-MIDI (rtpMIDI)</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Needs a client: Network MIDI over RTP/UDP; by Tobias Erichsen; requires install; tobias-erichsen.de/software/rtpmidi</td>
  </tr>
  <tr>
    <td>SimpleBLE (WinRT)</td>
    <th>🗃️</th>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">BLE MIDI; wraps WinRT Bluetooth; requires BT hardware; adapter support inconsistent (Intel generally works, some Realtek do not)</td>
  </tr>
  <tr>
    <td>WASAPI — existing mic/speaker</td>
    <th>🗃️</th>
    <td></td>
    <td>I/O</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">
    - Mic: privacy permission in Settings required;
    <br>-Speaker output: no permission. Loopback capture via AUDCLNT_STREAMFLAGS_LOOPBACK, no extra permission, DRM-protected content silenced.</td>
  </tr>
  <tr>
    <td>Virtual Audio (sysvad-based)</td>
    <th>X</th>
    <td></td>
    <td>I/O</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Fork WDK sysvad sample; requires EV code signing cert (~$300/yr) + Microsoft attestation signing (free); one-time UAC prompt at install; app talks to it via WASAPI</td>
  </tr>
  <tr>
    <td>Named Pipes (Win32)</td>
    <th>🗃️</th>
    <td></td><td></td><td></td>
    <td>O</td>
    <td></td><td></td><td></td><td></td>
    <td colspan="2">CreateNamedPipe; does not appear as a COM port to third-party applications</td>
  </tr>
  <tr>
    <td>VHD API (Win32)</td>
    <th>🗃️</th>
    <td></td><td></td><td></td>
    <td></td><td></td>
    <td>O</td>
    <td></td><td></td>
    <td colspan="2">CreateVirtualDisk + AttachVirtualDisk; built-in since Win 7; no install required</td>
  </tr>
  <tr>
    <td>WinTun</td>
    <th>?</th>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td>O</td>
    <td></td>
    <td colspan="2">WireGuard's bundlable DLL; ships a signed kernel driver that installs programmatically; no user-visible install prompt</td>
  </tr>
  <tr>
    <td>*Unsupported*</td>
    <th>X</th>
    <td></td>
    <td></td>
    <td>X</td>
    <td></td>
    <td>X</td>
    <td></td><td></td><td></td>
    <td colspan="2">Dead end:
      <br>- ViGEm and all alternatives require signed driver install; no no-install path
      <br>- DirectShow virtual camera filter requires COM registration with elevation; no no-install path
    </td>
  </tr>
  </tbody>
</table>

# Pipedream platforms

## iOS

<table>
  <thead><tr>
    <th></th>
    <th>MIDI</th><th>Audio</th><th>Video</th>
    <th>CDC</th><th>HID</th><th>MSC</th>
    <th>NET</th><th>Vendor</th>
    <th colspan="2">Notes</th>
  </tr></thead>
  <tbody>
  <tr>
    <td>CoreMIDI (framework)</td>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Virtual MIDI ports; BLE MIDI (central + peripheral) and RTP-MIDI network sessions built-in</td>
  </tr>
  <tr>
    <td>CoreBluetooth (framework)</td>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">BLE MIDI; O=CBPeripheralManager (advertise), I=CBCentralManager (connect)</td>
  </tr>
  <tr>
    <td>CoreAudio / AVFoundation — existing mic</td>
    <td></td>
    <td>I</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Microphone only via AVCaptureSession; NSMicrophoneUsageDescription required; no system audio capture possible</td>
  </tr>
  <tr>
    <td>NetworkExtension (framework)</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td>O</td>
    <td></td>
    <td colspan="2">NEPacketTunnelProvider; sandboxed TUN-like virtual network interface; Network Extensions entitlement required</td>
  </tr>
  <tr>
    <td>ReplayKit (framework)</td>
    <td></td><td></td>
    <td>I</td>
    <td></td><td></td><td></td><td></td><td></td>
    <td colspan="2">Screen/app capture only; no virtual camera device; user must consent each session</td>
  </tr>
  <tr>
    <td>*Unsupported*</td>
    <td></td><td></td><td></td>
    <td>x</td>
    <td>x</td>
    <td>x</td>
    <td></td>
    <td>x</td>
    <td colspan="2">Platform locked; no access to USB gadget stack on iOS</td>
  </tr>
  </tbody>
</table>

## Android

<table>
  <thead><tr>
    <th></th>
    <th>MIDI</th><th>Audio</th><th>Video</th>
    <th>CDC</th><th>HID</th><th>MSC</th>
    <th>NET</th><th>Vendor</th>
    <th colspan="2">Notes</th>
  </tr></thead>
  <tbody>
  <tr>
    <td>android.media.midi / amidi.h</td>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Java API 23+ or NDK amidi.h API 29+; acting as USB MIDI gadget (O) requires API 33+ and OEM hardware support</td>
  </tr>
  <tr>
    <td>BluetoothGattServer + BLE Advertiser</td>
    <td>I/O</td>
    <td></td><td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">BLE MIDI; API 21+; Java/JNI; O=advertise as BLE MIDI peripheral, I=connect to BLE MIDI central</td>
  </tr>
  <tr>
    <td>AAudio / OpenSL ES — existing mic/speaker</td>
    <td></td>
    <td>I/O</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">Use existing mic/speaker; RECORD_AUDIO permission required; no virtual audio device creation</td>
  </tr>
  <tr>
    <td>MediaProjection</td>
    <td></td>
    <td>I</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td colspan="2">System audio capture; API 29+; user must confirm a screen capture prompt at the start of every session</td>
  </tr>
  <tr>
    <td>libusb</td>
    <td>I</td><td>I</td><td>I</td>
    <td>I</td><td>I</td><td>I</td>
    <td>I</td><td>I</td>
    <td colspan="2">Device stealing; UsbManager permission grant required first (user dialog)</td>
  </tr>
  <tr>
    <td>VpnService</td>
    <td></td><td></td><td></td>
    <td></td><td></td><td></td>
    <td>O</td>
    <td></td>
    <td colspan="2">TUN-like virtual network interface; no root; user must accept a VPN prompt once</td>
  </tr>
  <tr>
    <td>Camera2 / CameraX</td>
    <td></td><td></td>
    <td>I</td>
    <td></td><td></td><td></td><td></td><td></td>
    <td colspan="2">Camera capture only; CAMERA permission; no virtual camera device</td>
  </tr>
  <tr>
    <td>*Unsupported*</td>
    <td></td>
    <td>x</td>
    <td></td>
    <td>x</td>
    <td>x</td>
    <td>x</td>
    <td></td><td></td>
    <td colspan="2">Dead end or root required</td>
  </tr>
  </tbody>
</table>
