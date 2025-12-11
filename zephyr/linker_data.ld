/*
 * Linker script snippet to define iterable sections for nrf_modem_lib callbacks.
 * These sections are required for STRUCT_SECTION_ITERABLE to work correctly.
 */

#include <zephyr/linker/iterable_sections.h>

ITERABLE_SECTION_RAM(nrf_modem_lib_init_cb, 4)
ITERABLE_SECTION_RAM(nrf_modem_lib_shutdown_cb, 4)
ITERABLE_SECTION_RAM(nrf_modem_lib_dfu_cb, 4)
ITERABLE_SECTION_RAM(nrf_modem_lib_at_cfun_cb, 4)
