

#include "sharp.h"

#include <stdio.h>
#include <math.h>

#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(sharp, LOG_LEVEL_DBG);

#define SPIOP     (SPI_WORD_SET(8) | SPI_TRANSFER_LSB)

struct spi_dt_spec spispec = SPI_DT_SPEC_GET(DT_NODELABEL(gendev), SPIOP, 0);
const struct gpio_dt_spec spicsspec = SPI_CS_GPIOS_DT_SPEC_GET(DT_NODELABEL(gendev));


uint8_t buffer[CMD_W] = {0};

int spi_sharp_init() {
	if (!spi_is_ready_dt(&spispec)) {
        return -1;
    }
	if (!device_is_ready(spicsspec.port)){
        return -2;
    }
    
    return 0;
}



static bool vcom = false;

void clear_screen(){
    memset(buffer, 0, CMD_W);
}


void sharp_draw() {
    vcom  = !vcom;
    
    uint8_t *buf = buffer;

    *buf++ = 0x1 | (vcom ? 0x2 : 0);
    for (uint8_t i=1; i<= SHARP_H; i++) {
        *buf++ = i;
        for (int j=0; j<SHARP_ROW_W; j++) {
            buf++;
        }
        *buf++ = 0;
    }
    *buf++ = 0;
    if (buf - buffer != CMD_W) {
        LOG_ERR("unexpected len: %d\n", buf - buffer);
        for(;;);
    }

    struct spi_buf tx_spi_buf		= {.buf = buffer, .len = CMD_W};
    struct spi_buf_set tx_spi_buf_set 	= {.buffers = &tx_spi_buf, .count = 1};

    int err = spi_write_dt(&spispec, &tx_spi_buf_set);
    if (err < 0) {
        LOG_ERR("spi_write_dt() failed, err: %d", err);
        for(;;);
    }
}

