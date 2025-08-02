#!/bin/sh
ZEPHYR_BASE=~/v3.0.2/zephyr west build -b nrf52832_feather . -- -DBOARD_ROOT=.
