# MicroPythonRT
MicroPythonRT is a fork of [MicroPython](https://github.com/micropython/micropython/) with added support for concurrency, dynamic linking and interoperability with non-Python projects. 

Concurrency means multiple programs running at the same time. MicroPythonRT achieves this by adopting [FreeRTOS](https://www.freertos.org/) as a foundation layer. FreeRTOS provides robust concurrency and parallelism support for MicroPython and C components. This support extends into the Python API by providing support for the [`threading`](https://docs.python.org/3/library/threading.html), [`select`](https://docs.python.org/3/library/select.html)/[`selectors`](https://docs.python.org/3/library/selectors.html), and [`asyncio`](https://docs.python.org/3/library/asyncio.html) modules.

Interoperability means the ease at which non-Python (e.g., C) code can be integrated into a MicroPython project to expand functionality either in cooperation with or independently of the MicroPython runtime. MicroPythonRT does this in several ways by: dynamically loading C libraries on-device, partitioning software components into independent tasks, and providing a common C runtime library.

## Support
If you are interested in using MicroPythonRT, please open an [issue](https://github.com/gneverov/micropythonrt/issues) or [discussion](https://github.com/gneverov/micropythonrt/discussions) to let me know how MicroPythonRT helps your application, or what additional functionality you need, or to simply report bugs. Your feedback will help to direct future development efforts.

## Highlights
- Support for [dynamic linking](/examples/dynlink/README.md) (i.e., DLLs or shared libraries). Native code libraries can be built, distributed, and installed without recompiling or reinstalling the firmware. This allows users to add new native code to their application, with similar ease to which Python code can be added by copying files to the device.

- FreeRTOS brings full concurrency support including multicore and multiple threads. All use of busy polling and background tasks have been removed from the MicroPython core and replaced with FreeRTOS-based concurrency. Never worry about Python code blocking the USB stack.

- [Picolibc](https://github.com/picolibc/picolibc) is used as the system-wide C runtime library. Crucially it implements malloc, allowing C programs to allocate their own memory independent of MicroPython; and thread-local storage, allowing code to execute on multiple cores. Picolibc is also extended with additional POSIX functionality from [Morelibc](https://github.com/gneverov/morelibc), including a virtual filesystem.

- System services such as [lwIP](https://savannah.nongnu.org/projects/lwip/) and [TinyUSB](https://docs.tinyusb.org/en/latest/) run as separate FreeRTOS tasks isolated of MicroPython. There is no problem with MicroPython or Python code being able to interfere with these services, and the MicroPython VM can be restarted without any effect on them.

- MicroPython threads are implemented as FreeRTOS tasks. The user can create any number of Python threads, which can execute on all available hardware cores. 

- lwIP is configured to use dynamic memory allocation, thus allowing the user to create any number of sockets. Also includes a telnet server and support for IPv6.

- USB device configuration can be changed at run-time, making it possible to change which USB device classes are exposed to the USB host without rebuilding firmware.

- TinyUSB support for network devices is exposed, allowing the microcontroller to connect to the Internet over a USB cable.

- Robust integration for [Mbed-TLS](https://github.com/Mbed-TLS/mbedtls) including certificate verification, CA certificate paths, and server-side.

- Arbitrary Python modules can be loaded into flash at run-time instead of RAM. This frees up large amounts of valuable RAM and allows for larger Python programs.

- The MicroPython REPL is extended to support the `await` keyword for `asyncio` programming.

## Supported Hardware
**Ports**: Currently MicroPythonRT is only supported on the RP2 port. However with time and effort it can be supported on any platform that support FreeRTOS. Only the [RPI PICO 2](https://micropython.org/download/RPI_PICO2/), [RPI PICO](https://micropython.org/download/RPI_PICO/) and [RPI PICO W](https://micropython.org/download/RPI_PICO_W/) boards have been tested.

**WIFI**: CYW43 support is ported in MicroPythonRT. Other wifi hardware (ninaw10, wiznet5k) are not currently supported.

**Bluetooth**: The Bluetooth functionality of the CYW43 is not currently supported.

## Getting Started ##
See the [getting started](/getstarted.md) section for details on how to install MicroPythonRT and run example code.

## Technical Differences
A detailed list of some of the technical differences from MicroPython.

### Modules
- `array`: The array subscript operator is extended to support store and delete of slices. For example,
```
x = bytearray(10)
x[2:4] = b'hello'
del x[7:]
```

- `audio_mp3`: A new module that provides a Python wrapper of the libhelix-mp3 library for decoding MP3 streams. Primarily  introduced to support the  [audio player demo](/examples/async/audio_player.md). Note that libhelix-mp3 is released under a different [license](/lib/audio/src/libhelix-mp3/LICENSE.txt) that does not permit free commercial use. This module can be disabled at build-time.

- `freeze`: A new module for "freezing" MicroPython runtime data structures into flash to free up RAM. See the [freezing demo](/examples/freeze/README.md) for more details.

- `lvgl`: An interface to the [LVGL](https://github.com/lvgl/lvgl/tree/master) graphics library from MicroPythonRT. See the [LVGL demo](/examples/lvgl/README.md) for more details.

- `micropython`: Information about FreeRTOS tasks is available by calling  new `tasks` method, and information about the C heap is available by calling the `malloc_stats` method.

- `machine.AudioOutPwm`: A class for generating audio through PWM hardware, similar to CircuitPython's [`PWMAudioOut`](https://docs.circuitpython.org/en/8.2.x/shared-bindings/audiopwmio/index.html#audiopwmio.PWMAudioOut) class. See [audio player demo](/examples/async/audio_player.md) for example of use.

- `machine.I2S`: This native class is removed because it can be implemented in pure Python. See [audio_i2s.py](/examples/async/audio_i2s.py) for an example.

- `machine.Pin`: MicroPythonRT does not support user-defined interrupt handlers, so all the interrupt support on the `Pin` class is removed. In its place is the `APin` class (*A* is for asynchronous, I guess). This class has a `wait` method that allows you to wait on 6 types of pin events: level low/high, edge fall/rise, and pulse down/up. The pulse events also return the duration of the pulse similar to Arduino's [`pulseIn`](https://www.arduino.cc/reference/en/language/functions/advanced-io/pulsein/) function. Additionally the `APin` class does not have the concept of modes and just supports a 3-value state:
```
pin.value = None  # input
pin.value = 0     # output low
pin.value = 1     # output high
```

- `machine.PioStateMachine`: Replaces the `rp2.StateMachine` for accessing the RP2040's PIO hardware. The class is redesigned to better support asynchronous operations. The ability to implement all PIO applications in Python is a goal for this redesigned class.

- `network`: Networking is now hard-coded to only support lwIP. This module has been repurposed to access LwIP functionality that's not already in the `socket` module, such as managing network interfaces. See network configuration [guide](/network.md) for more details.

- `network_cyw43`: This subcomponent of the network module still exists as a way to configure the cyw43 network device (e.g., tell it which wifi network to connect to). It is called the `cyw43` module and is available as a dynamically loadable extension module. The wifi scan method has be updated to use FreeRTOS instead of polling.

- `os`: The MicroPython VFS implementation is replaced with Morelibc. This allows interoperation of the filesystem between MicroPython and C, including the mounting of block devices, and redirecting stdio through character devices. This module also contains the dynamic linking API and other functionality from Morelibc and the CPython `os` module.

- `select`/`selectors`: The select module is rewritten to use Morelibc and FreeRTOS. The select/poll implementation can wait on any sort of file descriptor, including sockets, serial devices, and other hardware. The selector is a crucial part of any asyncio implementation. At its heart, an asyncio event loop will contain a selector to bring about IO concurrency between tasks.

- `signal`: A new module similar to the [signal](https://docs.python.org/3/library/signal.html) module from CPython. Signals are important because they are the way of controlling Ctrl-C behavior in Python. For example, when running an asyncio event loop and the user presses Ctrl-C, you don't want to break inside the event loop code and leave the running tasks in an undefined state. Instead you want to cancel all the tasks and wait for their cleanup to finish. The signal module provides a portable way of overriding Ctrl-C behavior. To this end, the only signal type MicroPythonRT supports is SIGINT.

- `socket`: The socket module is implemented using the POSIX API from Morelibc, while maintaining a CPython-compatible API. The `settimeout` method on socket objects is used to control whether the socket is blocking or non-blocking. A blocking socket will block the caller's task but allow other system tasks to run, including other MicroPython threads.

- `time`: Module refactored to be more consistent with CPython and to use Picolibc for time functions, including timezone information.

- `threading`: The threading (and lower-level `_thread`) module is implemented using FreeRTOS tasks. Python code can create any number of threads (limited by available RAM). The implementation relies on a GIL, so just as in CPython, there is no automatic compute parallelism advantage to using multiple threads. A full suite of synchronization objects is also implemented.

- `usb`: A new module that allows Python code to interact with TinyUSB. Support is somewhat limited and currently only exposes CDC and MSC devices, in addition to network devices through `network`.

- `usb.UsbConfig`: Typically the USB configuration of a board is fixed in firmware. However this class allows you to reconfigure the USB configuration at runtime (e.g., change which device classes the board exposes to a USB host). To use this class some prior understanding of the [USB descriptors](https://www.beyondlogic.org/usbnutshell/usb5.shtml) is required. For example, to set the board to have 5 CDC (serial) devices, you could run:
```
usb_config = usb.UsbConfig()
usb_config.device()
usb_config.configuration()
for _ in range(5):
  usb_config.cdc()
usb_config.save()
```

### Framework
- **blocking**: Functions such as sleep, blocking I/O, and select will block the calling task using a FreeRTOS API, thus allowing the CPU to continue executing other tasks. Additionally the GIL is released before the blocking call to give other MicroPython threads a chance to run. The macro `MICROPY_EVENT_POLL_HOOK` is not needed anymore.

<!--- **flash translation layer**: Uses a variant of [Dhara](https://github.com/gneverov/dhara16) to provide a flash translation layer to the XIP flash storage. This allows for wear levelling of the on-chip flash memory and a smaller filesystem block size.-->

- **main**: The C main function is responsible for starting FreeRTOS tasks for various subsystems. MicroPython, lwIP, and TinyUSB all execute as separate tasks. cyw43_driver executes as a FreeRTOS timer.

- **memory map**: The 8 kB of RAM in the scratch X and Y regions are used as the ARM "main" stacks for each core. The "process" stacks for all FreeRTOS tasks, including the MicroPython threads, are dynamically allocated from the C heap.

- **MICROPY_FREERTOS**: The rp2 port of this fork is hard-coded to depend on FreeRTOS. The C preprocessor macro `MICROPY_FREERTOS` is used in common code outside of the port directory to optionally refer to FreeRTOS functionally.

- **argument parsing**: New functions that provide an alternate way of parsing Python function arguments in C that is based on the [PyArg](https://docs.python.org/3/c-api/arg.html) API from CPython.

- **REPL await**: You can use "await" syntax directly from the MicroPython REPL:
```
>>> await coro()
```
This functionality requires that the expression "asyncio.repl_runner" evaluates to an asyncio `Runner` object.

## Acknowledgements
Thanks to the authors of MicroPython, CircuitPython, Picolibc, and FreeRTOS for their awesome projects I was able to build upon.
