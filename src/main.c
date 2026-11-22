/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "drivers/sharp.h"
#include "drivers/gps.h"
#include "watch_face.h"



// I will express a view on minimal features here:

// I think we should have 5 buttons 

// One watchface whatever that looks like
// Forward back to switch applications. 
// Select/ Start button
// "Exit"
// Multipurpose

// Three applications
// Watchface 
// - Running Activity, Elapsed Time, Elapsed distance, Current and Average Pace
// - Music, Play Pause, Skip
// Keep a state "which app is displayed" 




LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS  1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
	int ret;
	bool led_state = true;

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	ret = spi_sharp_init();
	if (ret < 0) {
        LOG_ERR("spi_sharp_init failed: %d\n", ret);
		return 0;
	}

	// Initialize UART0 for debug output
	const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready\n");
	} else {
		// Optional: Configure UART at runtime
		struct uart_config cfg = {
			.baudrate = 115200,
			.parity = UART_CFG_PARITY_NONE,
			.stop_bits = UART_CFG_STOP_BITS_1,
			.data_bits = UART_CFG_DATA_BITS_8,
			.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
		};
		ret = uart_configure(uart_dev, &cfg);
		if (ret < 0) {
			LOG_ERR("UART configure failed: %d\n", ret);
		} else {
			LOG_INF("UART0 initialized successfully");
		}
	}

	ret = gps_init();
	if (ret < 0) {
		LOG_ERR("gps_init failed: %d\n", ret);
		// Continue anyway, GPS might not be connected
	} else {
		LOG_INF("GPS initialized successfully");
	}

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}
		
		render_watch_screen();
		sharp_draw();

		led_state = !led_state;
		printf("LED state: %s\n", led_state ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
