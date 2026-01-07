# nRFModule SDK

Public distribution of the nRFModule library. Contains headers and pre-compiled binaries for customer integration.

## Version Compatibility

| nRFModule Version | NCS Version | Branches | Status | Notes |
|-------------------|-------------|----------|--------|-------|
| **v2.x** | **3.2.x** | `main`, `v2.x` | ✅ **Active** | Uses `sm_at_client` transport |
| v1.x | 3.1.x | `v1.x` | 🔧 Maintenance | Legacy `slm_backend` |

**Current Version:** v2.0.0

### Choosing a Version

```yaml
# In your west.yml:

# For NCS 3.2.x projects - Choose one:
- name: nrfmodule-sdk
  revision: v2.0.0  # Pinned to specific release (recommended for production)
  # OR
  revision: v2.x    # Latest v2.x patches (recommended for active development)
  # OR
  revision: main    # Latest stable across all versions

# For NCS 3.1.x projects (legacy):
- name: nrfmodule-sdk
  revision: v1.x    # Latest v1.x patches
```

## Overview

This SDK enables nRF52840 devices to use standard Nordic LTE libraries (`lte_lc`, `modem_info`, `date_time`, etc.) by communicating with an external nRF9160 modem via UART. The nRF9160 runs Nordic's Serial Line Modem (SLM) firmware.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Your Application                     │
├─────────────────────────────────────────────────────────┤
│   lte_lc  │  modem_info  │  date_time  │  MQTT Client  │
├─────────────────────────────────────────────────────────┤
│              nrf_modem_at / nrf_modem_lib               │
├─────────────────────────────────────────────────────────┤
│                    sm_at_client (UART)                  │
├─────────────────────────────────────────────────────────┤
│                      nRF52840 Host                      │
└─────────────────────────────────────────────────────────┘
                           │ UART
                           ↓
┌─────────────────────────────────────────────────────────┐
│              nRF9160 (SLM Firmware)                     │
└─────────────────────────────────────────────────────────┘
```

### Directory Structure

```
nrfmodule-sdk/
├── CMakeLists.txt          # Build system (auto-detects source vs binary mode)
├── README.md               # This file
├── include/                # Public API headers
│   ├── modem_core.h
│   ├── nrf_modem_at.h
│   ├── nrf_modem_lib.h
│   ├── nrf_socket.h
│   ├── nrfmodule_mqtt.h
│   └── sm_at_client.h
├── lib/                    # Pre-compiled binaries (for customers)
│   └── libmodem_core.a
└── zephyr/
    ├── module.yml          # Zephyr module registration
    └── linker_data.ld      # Linker sections for callbacks
```

## Quick Start

### 1. Add to West Manifest

```yaml
manifest:
  projects:
    - name: nrfmodule-sdk
      url: https://github.com/nrfmodule/nrfmodule-sdk
      revision: main
      path: modules/lib/nrfmodule-sdk
```

### 2. Configure Kconfig

```kconfig
# Required
CONFIG_NRFMODULE_SM_AT_CLIENT=y

# Enable desired features
CONFIG_NRFMODULE_LTE_LINK_CONTROL=y
CONFIG_NRFMODULE_MODEM_INFO=y
CONFIG_NRFMODULE_DATE_TIME=y
CONFIG_NRFMODULE_MODEM_KEY_MGMT=y
CONFIG_NRFMODULE_MQTT=y
```

### 3. Configure Device Tree

Define the UART connection to the nRF9160:

```dts
/ {
    chosen {
        ncs,slm-uart = &uart1;
    };
};

&uart1 {
    status = "okay";
    current-speed = <115200>;
    pinctrl-0 = <&uart1_default>;
    pinctrl-1 = <&uart1_sleep>;
    pinctrl-names = "default", "sleep";
};
```

### 4. Use Standard Nordic APIs

```c
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>

int main(void) {
    /* Initialize modem (via UART) */
    nrf_modem_lib_init();
    
    /* Connect to LTE network */
    lte_lc_connect_async(lte_handler);
    
    /* ... */
}
```

## Dual-Mode Build System

The SDK automatically detects if `nrfmodule-core` (source code) is present:

| Mode | Condition | Result |
|------|-----------|--------|
| **Source** | `nrfmodule-core` exists in workspace | Compiles from source |
| **Binary** | `nrfmodule-core` not found | Links pre-compiled `.a` |

This allows developers to work with source while customers use binaries.

## Supported Libraries

| Library | Config Option | Description |
|---------|---------------|-------------|
| LTE Link Control | `CONFIG_NRFMODULE_LTE_LINK_CONTROL` | Network connection management |
| Modem Info | `CONFIG_NRFMODULE_MODEM_INFO` | IMEI, ICCID, signal strength |
| Date/Time | `CONFIG_NRFMODULE_DATE_TIME` | Network time synchronization |
| Modem Key Mgmt | `CONFIG_NRFMODULE_MODEM_KEY_MGMT` | Certificate/key storage |
| MQTT | `CONFIG_NRFMODULE_MQTT` | MQTT client (AWS IoT, etc.) |

## Requirements

- **Host MCU**: nRF52840 (or similar with UART)
- **Modem**: nRF9160 running SLM firmware
- **NCS Version**: v3.2.1+
- **Connection**: UART (115200 baud default)

## License

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
