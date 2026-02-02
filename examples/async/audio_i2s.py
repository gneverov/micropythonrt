import array
import machine


class AudioOutI2S(machine.PioStateMachine):
    PIO_PROGRAM = array.array("H", [59438, 24577, 2113, 28673, 63534, 28673, 6213, 24577])

    def __init__(
        self,
        sd_pin,
        sck_pin,
        ws_pin,
        num_channels,
        sample_rate,
        bytes_per_sample,
        *,
        fifo_size=1024,
        threshold=0,
    ):
        super().__init__(self.PIO_PROGRAM, [sd_pin, sck_pin, ws_pin])

        if num_channels == 1:
            dma_size = self.DMA_SIZE_16
        elif num_channels == 2:
            dma_size = self.DMA_SIZE_32
        else:
            raise ValueError("num channels not supported")

        if bytes_per_sample != 2:
            raise ValueError("bytes per sample not supported")

        self.configure_fifo(True, fifo_size, threshold, dma_size)
        self.set_frequency(sample_rate * 2 * bytes_per_sample * 8 * 2)
        self.set_pins(self.OUT, sd_pin, 1)
        self.set_shift(self.OUT, False, True, 32)
        self.set_sideset(2, False, False)
        self.set_pins(self.SIDESET, sck_pin, 2)
        self.set_pindirs([], [sd_pin, sck_pin, ws_pin])
        self.reset(0)

    def start(self):
        self.set_enabled(True)

    def stop(self):
        self.set_enabled(False)
