#!/bin/sh
sudo openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg \
  -c "transport select swd" \
  -c "adapter speed 2000" \
  -c "init" -c "reset init" \
  -c "program build/fosswatch/zephyr/zephyr.elf verify reset exit"
