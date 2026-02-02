import audio_i2s
import audio_mp3
import audio_wav
import machine
import os
import re
import select
import threading
import time


I2S_SD = machine.Pin(9)
I2S_SCK = machine.Pin(10)
I2S_WS = machine.Pin(11)


def i2s_factory(num_channels, sample_rate, bytes_per_sample, buf_size, **kwargs):
    return audio_i2s.AudioOutI2S(
        I2S_SD,
        I2S_SCK,
        I2S_WS,
        num_channels,
        sample_rate,
        bytes_per_sample,
        fifo_size=8 * buf_size,
        threshold=2 * buf_size,
        **kwargs,
    )


PWM_A = machine.Pin(12)
PWM_B = machine.Pin(13)


def pwm_factory(num_channels, sample_rate, bytes_per_sample, buf_size, **kwargs):
    return machine.AudioOutPwm(
        PWM_A,
        PWM_B,
        num_channels,
        sample_rate,
        bytes_per_sample,
        fifo_size=4 * buf_size,
        threshold=buf_size,
        **kwargs,
    )


BUTTON = machine.Pin(14)


def button_factory():
    button = machine.APin(BUTTON)
    button.set_pulls(True, False)
    button.set_edges(True, False)
    return button


def open_stream(path):
    if path.startswith("http://") or path.startswith("https://"):
        import http.client

        m = re.match("http(s?)://([^/]+)(/.*)", path)
        print(m.group(1), m.group(2), m.group(3))
        if m.group(1):
            conn = http.client.HTTPSConnection(m.group(2))
        else:
            conn = http.client.HTTPConnection(m.group(2))
        conn.request("GET", m.group(3))
        stream = conn.getresponse()
        if stream.status != http.HTTPStatus.OK:
            raise OSError(f"HTTP error {stream.status}")
        path = m.group(3)
    else:
        stream = open(path, "rb")

    if path.endswith(".wav"):
        return audio_wav.WavReader(stream)
    elif path.endswith(".mp3"):
        return audio_mp3.AudioMP3Decoder(stream)
    else:
        raise ValueError("file type not supported")


def control(eventfd, audio_out):
    stopped = False
    button = button_factory()
    button.debounce_time_us = 30_000
    os.set_blocking(button.fileno(), False)
    try:
        while True:
            select.select([eventfd, button], [], [])
            try:
                os.eventfd_read(eventfd)
            except BlockingIOError:
                pass
            else:
                break

            try:
                button.wait()
            except BlockingIOError:
                pass
            else:
                if stopped:
                    print("\n", end="")
                    audio_out.start()
                else:
                    audio_out.stop()
                    print("\nstopped", end="", flush=True)
                stopped = not stopped
    finally:
        button.close()


def play(path, audio_out_factory, buf_size=2304, **kwargs):
    decoder = open_stream(path)

    bytes_per_sample = (decoder.bits_per_sample + 7) // 8
    print(
        f"{decoder.num_channels} channels, {decoder.sample_rate} Hz, {decoder.bits_per_sample} bits"
    )
    audio_out = audio_out_factory(
        decoder.num_channels, decoder.sample_rate, bytes_per_sample, buf_size, **kwargs
    )

    eventfd = os.eventfd(0, os.O_NONBLOCK)
    control_thread = threading.Thread(target=control, args=(eventfd, audio_out))
    control_thread.start()

    try:
        byte_rate = decoder.num_channels * decoder.sample_rate * bytes_per_sample
        buf = bytearray(buf_size * decoder.num_channels * bytes_per_sample // 2)
        br = decoder.readinto(buf)
        total_bw = 0
        next_bw = byte_rate
        audio_out.start()

        x = time.monotonic()
        while br > 0:
            assert br == len(buf)
            bw = audio_out.write(buf)
            assert bw == br
            total_bw += bw
            if total_bw > next_bw:
                print(".", end="", flush=True)
                next_bw += byte_rate
            br = decoder.readinto(buf)
        audio_out.drain()
        print(f"\ntime = {time.monotonic() - x}")
    finally:
        decoder.close()
        os.eventfd_write(eventfd, 1)
        # audio_out.debug()
        control_thread.join()
        audio_out.close()


try:
    import casyncio as asyncio
except:
    print("no asyncio")
else:

    def pin_wait(pin, value):
        return asyncio.stream_wait(pin, None, type(pin).wait, pin, value)

    async def control_async(button, audio_out):
        stopped = False
        while True:
            await pin_wait(button, button.PULSE_DOWN)

            if stopped:
                print("\n", end="")
                audio_out.start()
            else:
                audio_out.stop()
                print("\nstopped", end="")
            stopped = not stopped

    stream_readinto = asyncio.stream_readinto

    stream_write = asyncio.stream_write

    def audio_out_drain(audio_out):
        return asyncio.stream_wait(None, audio_out, type(audio_out).drain, audio_out)

    async def play_async(path, audio_out_factory, buf_size=2304, **kwargs):
        decoder = open_stream(path)
        decoder.settimeout(0)

        bytes_per_sample = (decoder.bits_per_sample + 7) // 8
        print(
            f"{decoder.num_channels} channels, {decoder.sample_rate} Hz, {decoder.bits_per_sample} bits"
        )
        audio_out = audio_out_factory(
            decoder.num_channels, decoder.sample_rate, bytes_per_sample, buf_size, **kwargs
        )
        audio_out.settimeout(0)

        button = button_factory()
        button.settimeout(0)
        control_task = asyncio.create_task(control_async(button, audio_out))

        try:
            byte_rate = decoder.num_channels * decoder.sample_rate * bytes_per_sample
            buf = bytearray(buf_size * decoder.num_channels * bytes_per_sample // 2)
            br = await stream_readinto(decoder, buf)
            total_bw = bw = await stream_write(audio_out, buf, br)
            next_bw = byte_rate
            audio_out.start()

            x = time.monotonic()
            while bw:
                br = await stream_readinto(decoder, buf)
                bw = await stream_write(audio_out, buf, br)
                assert bw == br
                total_bw += bw
                if total_bw > next_bw:
                    print(".", end="")
                    next_bw += byte_rate
                    await asyncio.sleep(0)
            await audio_out_drain(audio_out)
            print(f"\ntime = {time.monotonic() - x}")
        finally:
            control_task.cancel()
            decoder.close()
            button.close()
            # audio_out.debug()
            audio_out.close()
