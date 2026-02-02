import machine, os, usb

if os.getenv("ROOT"):
    root_device, root_fstype = os.getenv("ROOT").split()[:2]
else:
    root_device, root_fstype = "/dev/storage", "fatfs"

msc = usb.MscDevice()


def local(readonly=False):
    os.mount(root_device, "/", root_fstype, 33 if readonly else 32)


def usb(readonly=False):
    msc.eject()
    local(not readonly)
    msc.insert(root_device, 1 if readonly else 0)


def uf2():
    msc.eject()
    local()
    msc.insert("/dev/uf2")


def none():
    msc.eject()
    local()


def sdcard(spi=0, sck=10, mosi=11, miso=12, cs=14, path="/sdcard", readonly=True):
    machine.SPI(spi, 400000, sck=sck, mosi=mosi, miso=miso)
    os.mount(f"/dev/sdcard{spi}?cs={cs}", path, "fatfs", 33 if readonly else 32)
