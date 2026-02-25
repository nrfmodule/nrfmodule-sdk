# SPDX-License-Identifier: Apache-2.0

# Check SoC variant using BOARD_QUALIFIERS (Zephyr 3.x+ with board variants)
if("${BOARD_QUALIFIERS}" STREQUAL "/nrf52840")
  board_runner_args(nrfjprog "--nrf-family=NRF52")
  board_runner_args(jlink "--device=nRF52840_xxAA" "--speed=4000")
  board_runner_args(pyocd "--target=nrf52840" "--frequency=4000000")
  
  include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
endif()

if("${BOARD_QUALIFIERS}" STREQUAL "/nrf9151" OR "${BOARD_QUALIFIERS}" STREQUAL "/nrf9151/ns")
  board_runner_args(nrfjprog "--nrf-family=NRF91")
  board_runner_args(jlink "--device=nRF9160_xxAA" "--speed=4000")
  
  include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
endif()
