# SPDX-License-Identifier: Apache-2.0

# board.c writes nRF52 UICR REGOUT0, board_power.c gates USBD.
if(CONFIG_BOARD_PIGEONTRACKER_NRF52840)
  zephyr_library()
  zephyr_library_sources(board.c)
  zephyr_library_sources_ifdef(CONFIG_GPIO board_power.c)
  # Producer side of the shared VBUS signal (usb_vbus_internal.h).
  zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR}/../../../lib/power)
endif()
