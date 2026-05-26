/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * BeeScales BT early board init — runs in ALL images (MCUboot + application).
 *
 * - Power rails enabled via HAL before any driver init. Required for MCUboot
 *   to access QSPI flash (secondary slot). In the application the regulator-fixed
 *   DTS nodes also manage these pins — setting an already-high pin is a no-op.
 * - REGOUT0 set to 3.0V for battery operation (one-time UICR write with reset).
 */

#include <zephyr/init.h>
#include <hal/nrf_power.h>
#include <hal/nrf_gpio.h>

#define POWER_EN_PIN  NRF_GPIO_PIN_MAP(1, 4)
#define HX711_EN_PIN  NRF_GPIO_PIN_MAP(1, 9)

void board_early_init_hook(void)
{
	/* Enable power rails before QSPI flash driver probes */
	nrf_gpio_pin_set(POWER_EN_PIN);
	nrf_gpio_cfg_output(POWER_EN_PIN);

	nrf_gpio_pin_set(HX711_EN_PIN);
	nrf_gpio_cfg_output(HX711_EN_PIN);
	if ((nrf_power_mainregstatus_get(NRF_POWER) ==
	     NRF_POWER_MAINREGSTATUS_HIGH) &&
	    ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) ==
	     (UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos))) {

		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
			;
		}

		NRF_UICR->REGOUT0 =
			(NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
			(UICR_REGOUT0_VOUT_3V0 << UICR_REGOUT0_VOUT_Pos);

		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
			;
		}

		NVIC_SystemReset();
	}
}
