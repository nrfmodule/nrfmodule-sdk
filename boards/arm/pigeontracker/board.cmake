# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SOC_NRF52840)
  board_runner_args(nrfjprog "--nrf-family=NRF52")
  board_runner_args(jlink "--device=nRF52840_xxAA" "--speed=4000")
  board_runner_args(pyocd "--target=nrf52840" "--frequency=4000000")

  include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
elseif(CONFIG_SOC_SERIES_NRF91)
  # nRF9151 targets (secure and ns)
  board_runner_args(nrfjprog "--nrf-family=NRF91")
  board_runner_args(jlink "--device=nRF9151_xxCA" "--speed=4000")

  include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
endif()
