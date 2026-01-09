# Getting Started
MicroPythonRT takes advantage of its dynamic linking functionality to build a board out of independent modules. First the base firmware is flashed onto the device, then after that zero or more extensions modules can also be flashed.

## Installing base firmware

### Download
> [!IMPORTANT]
> I've stopped making binary releases for now. It is best to [build](#building) from sources.

Download a firmware UF2 image for your board:
- [RPI PICO](https://github.com/gneverov/micropythonrt/releases/download/v0.0.2/firmware.uf2)

Only the RPI PICO and RPI PICO W boards are supported. This firmware may also work on other RP2040 boards, but it is not tested.

> [!CAUTION]
> If you have another MicroPython/CircuitPython installation on the board, flashing this UF2 file will erase any existing filesystem on your board.

### Building
Before building the project, you first need to do a one-off build and install of Picolibc. Run the script:
```
./lib/morelibc/build_picolibc.sh
```
Refer to the Morelibc [documentation](/lib/morelibc/README.md) for more context about this process.

Once Picolibc is installed, building MicroPythonRT firmware is the same as building MicroPython. 
```
cd ports/rp2
make BOARD=RPI_PICO2
```
Only the board types RPI_PICO and RPI_PICO2 are supported. The "W" variants are also supported, but they build the same firmware binary with the addition of an extension binary for CYW43.
Refer to the MicroPython building [guide](https://docs.micropython.org/en/latest/develop/gettingstarted.html) for more information.

Building an extension module is new process. Refer to the [audio_mp3](/extmod/audio_mp3/modaudio_mp3.c) module for an example. In addition to creating a CMake library target for your module (i.e., micropy_lib_audio_mp3, for the audio example), the main CMakeLists needs to be modified to build the top-level extension module executable and UF2 file. Add the following line to [micropy.cmake](/ports/rp2/cmake/micropy.cmake) where "audio_mp3" will be the name of the ELF/UF2 files produced.
```
add_micropy_extension_library(audio_mp3 micropy_lib_audio_mp3)
```

### Install
Installing firmware is the same as MicroPython:
1. Press and hold down BOOTSEL button while connecting the board to USB.
1. A USB drive should appear. Copy the downloaded UF2 file to this drive.
1. The device should automatically reboot and a new USB serial port should appear on your computer.
1. Connect to the device using [mpremote](https://docs.micropython.org/en/latest/reference/mpremote.html) or other terminal program.

## Configuring the device
The firmware configures the device as follows:
- One USB MSC device for accessing the on-board filesystem, and acting as a UF2 loader for extension modules.
- One USB CDC device that is used as stdio.
- A FAT filesystem on the XIP flash mounted as root.
- No network

However this is just the default configuration and once you are connected to MicroPython you can make changes.

### Configure USB devices
The USB devices that the board exposes can be dynamically configured. For example, to add USB networking, you could run.
```
import usb
usb_config = usb.UsbConfig()
usb_config.device()         # Create device descriptor
usb_config.configuration()  # Create configuration descriptor
usb_config.net_rndis()      # Create RNDIS network descriptors
usb_config.cdc()            # Create serial port descriptors
usb_config.msc()            # Create mass storage descriptor
usb_config.save()
```
This code only needs to be executed once and the configuration is persisted across reboots.

This `UsbConfig` methods also take keyword arguments to define values for various USB metadata such as VID:PID and name strings. See the source [code](/extmod/usb/usb_config.c) for details.

### Configure IO
If you want to use a UART instead of USB serial for accessing the MicroPython REPL, you could run.
```
import os
os.putenv('TTY', '/dev/ttyS0')
# reboot
```
The TTY environment variable tells the system which device to use for stdio. Be careful how you set this as you may leave your board without a way to connect to it and you'll need to manually erase the flash chip to reset it.

The TX and RX pins are determined by the Pico SDK defaults.

### Configure storage
Upon reset, the system is configured with the root filesystem in read/write mode, and the USB mass storage device not mounted. There is a helper module, [`fsutil`](/ports/rp2/modules/fsutil.py), that helps manage the root filesystem and the USB mass storage device.
```
import fsutil
# Expose the filesystem over USB so that your computer can write to it.
fsutil.usb(readonly=False)

# Make the filesystem read-only to the device so that it doesn't interfere with USB access.
fsutil.local(readonly=True)

# Stop exposing the filesystem over USB.
fsutil.none()

# Unmount the root filesystem
import os
os.umount('/')
```
You can put some of this code in `boot.py` so that storage is always configured to your liking after boot.

#### Configure an SD card
```
fsutil.sdcard(spi=0, sck=10, mosi=11, miso=12, cs=14, path="/sdcard"):
```
This will mount the SD card on the path `/sdcard`. Although the parameters have defaults, it's best to specify the SPI instance (0 or 1) and the 4 SPI pins explicitly. Currently SD card access is read-only.

### Configure network
Network configuration is described in a separate [document](/network.md).

### Configure time
The TZ environment variable controls the time zone of the device, following the POSIX [standard](https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap08.html). This functionality is implemented by Picolibc. For example:
```
import os
os.putenv('TZ', 'PST8PDT')  # US Pacific time zone
```

If the board has a working network connection, and it was configured by DHCP, and the DHCP server provided SNTP server addresses, then the board will automatically try to synchronize the current time. To manually set a SNTP server, run:
```
import network, socket
network.sntp([socket.gethostbyname('time.windows.com')])
```

### Configure MicroPython
By default, MicroPython uses some about of stack and heap space. If you need to change this, you can run:
```
import os
os.putenv('MP_STACK', str(8 * 1024))  # Sets MicroPython stack to 8 kB per thread
os.putenv('MP_HEAP', str(64 * 1024))  # Sets MicroPython heap to 64 kB
# reboot
```

## Adding extension modules
A novel feature of MicroPythonRT is the ability to add optional native code extension modules to the base firmware. To begin, put the board in UF2 mode by running:
```
import fsutil
fsutil.uf2()
```
A USB drive named "MPRT_UF2" should appear on the host computer.


Next choose the extension module you wish to flash. The following are provided prebuilt.
- [audio_mp3](https://github.com/gneverov/micropythonrt/releases/download/v0.0.2/libaudio_mp3.uf2): An MP3 audio stream decoder.
- [cyw43](https://github.com/gneverov/micropythonrt/releases/download/v0.0.2/libcyw43.uf2): The wifi driver for the PICO W.
- [lvgl](https://github.com/gneverov/micropythonrt/releases/download/v0.0.2/liblvgl.uf2): The [LVGL](https://github.com/lvgl/lvgl) graphics library.

Download a UF2 file and copy it to the UF2 drive the same way you copied the firmware UF2 file.

If flashing was successful, then MicroPython will automatically restart and you can import the new module. If flashing was unsuccessful, then nothing will happen. To see a potentially helpful error message, run:
```
import os
os.dlerror()
```

Note that the PICO only has 2 MB of on-board flash and this is not enough space to flash all of the prebuilt extension modules simultaneously. Attempts to do so will eventually lead to an out of space error. Also be aware that the same "space" is used for [freezing](/examples/freeze/README.md) MicroPython modules. So freezing MicroPython modules reduces the size of extension modules that can be flashed.

To get back to a clean slate, run:
```
import freeze
freeze.clear()
```
This deletes all extension modules and all frozen MicroPython modules, and returns to the state after having just flashed the firmware UF2. From there you can reflash the things you want.

## Examples
Check out the [demo apps](/examples/async/README.md) to see examples of MicroPythonRT's unique capabilities.
