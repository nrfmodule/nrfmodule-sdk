# SPDX-License-Identifier: Apache-2.0

# nRF52840-host only: board.c writes nRF52 UICR REGOUT0, board_power.c gates USBD.
if(CONFIG_BOARD_LIVETRACKER_NRF52840)
  zephyr_library()
  zephyr_library_sources(board.c)
  zephyr_library_sources_ifdef(CONFIG_GPIO board_power.c)
endif()
