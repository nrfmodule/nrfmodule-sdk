# nRFModule SDK - AI Agent Instructions

## Architecture Overview

**This is "The Face" repository** - the public distribution layer for nRFModule library.

**Key Distinction from nrfmodule-core:**
- **nrfmodule-sdk** (THIS REPO): Public headers (`.h`), pre-compiled binaries (`.a`), Zephyr module config
- **nrfmodule-core**: Private implementation (`.c` source files) - not distributed to customers

**Dual-Mode Operation:**
Customers see ONLY this repo, but build system auto-detects:
1. **Source mode**: If `nrfmodule-core` exists in workspace → compile from source
2. **Binary mode**: If `nrfmodule-core` missing → link pre-compiled `.a` files

## Critical Build Logic - `CMakeLists.txt`

**This file contains ALL the intelligence** (nrfmodule-core/CMakeLists.txt is intentionally minimal):

```cmake
# 1. Check if Zephyr auto-loaded the module
if(TARGET private_modem_lib)
    message(STATUS "Source Module auto-loaded")

# 2. Manual source loading (internal dev workspace)
elseif(EXISTS "${PRIVATE_SRC}/CMakeLists.txt")
    message(STATUS "Loading module manually from source")
    add_subdirectory(${PRIVATE_SRC} ${CMAKE_CURRENT_BINARY_DIR}/private_src)

# 3. Binary fallback (customer mode)
else()
    message(STATUS "Linking PUBLIC BINARY")
    zephyr_link_libraries(${CMAKE_CURRENT_LIST_DIR}/lib/libmodem_core.a)
endif()
```

**Why this pattern?**
- Internal developers work with source transparently
- Customers get pre-compiled binaries without seeing implementation
- Same CMake logic works in both scenarios

**Path calculation:**
```cmake
get_filename_component(WEST_TOP "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(PRIVATE_SRC "${WEST_TOP}/modules/lib/nrfmodule-core")
```
Assumes West workspace structure: `workspace/modules/lib/nrfmodule-sdk/` and `workspace/modules/lib/nrfmodule-core/`

## Zephyr Module Integration

### `zephyr/module.yml`

**Minimal configuration:**
```yaml
build:
  cmake: .
```

**What this does:**
- Registers this repo as a Zephyr module
- Tells Zephyr to run `CMakeLists.txt` during build
- Automatically adds to include path and library targets

**No Kconfig/settings needed yet** - pure C library without configuration options.

## Public API Contract

### `include/modem_core.h`

**Interface-only header** - no implementation details:
```c
#ifndef MODEM_CORE_H
#define MODEM_CORE_H
void modem_say_hello(void);
int modem_calc_add(int a, int b);
#endif
```

**Naming conventions (match NCS/Zephyr):**
- Prefix all functions with module name: `modem_*`
- Use snake_case for functions: `modem_say_hello()` not `modemSayHello()`
- Header guards: `MODULE_NAME_H` pattern

**Zephyr include pattern:**
```c
// In customer application code:
#include <modem_core.h>  // Works because zephyr_include_directories(include)
```

## Binary Distribution

### `lib/libmodem_core.a`

**Pre-compiled archive created externally** (not in this repo's build):

1. Build `nrfmodule-core` project with Zephyr
2. Extract compiled object files
3. Use ARM GCC archiver: `arm-none-eabi-ar rcs libmodem_core.a modem_core.o`
4. Copy resulting `.a` file to `nrfmodule-sdk/lib/`
5. Commit binary to this repo

**When to update binary:**
- After merging changes to `nrfmodule-core`
- Before customer releases
- When API surface changes

**Architecture considerations:**
- Binary must match target architecture (ARM Cortex-M for Nordic)
- Built with same Zephyr SDK version as customers will use
- Include debug symbols? Trade-off: size vs debugging

## Development Workflow

**This repo is rarely edited directly** - most work happens in `nrfmodule-core`.

**When to modify THIS repo:**
1. **API changes** - Update headers in `include/`
2. **Binary updates** - Replace files in `lib/`
3. **Build system changes** - Modify `CMakeLists.txt` logic
4. **Module config** - Update `zephyr/module.yml` (e.g., adding Kconfig)

**When NOT to modify:**
- Implementing features → do that in `nrfmodule-core`
- Bug fixes → fix source in `nrfmodule-core`, then regenerate binary

## West Multi-Repo Context

**In developer workspace:**
```
workspace/
├── modules/lib/
│   ├── nrfmodule-core/   # Source (private)
│   └── nrfmodule-sdk/    # THIS REPO (public)
```
Build system detects both → uses source mode.

**In customer workspace:**
```
workspace/
├── modules/lib/
│   └── nrfmodule-sdk/    # ONLY this repo
```
Build system detects missing core → uses binary mode.

**Coordinated via `nrfmodule-dev-manifest/west.yml`:**
```yaml
projects:
  - name: nrfmodule-sdk
    import: true  # Loads this repo's west.yml (if any)
  - name: nrfmodule-core  # Only in dev manifest, not customer manifests
```

## Versioning Strategy

**Currently using branch-based:**
- `main` branch tracks latest development
- No semantic versioning tags yet

**Future considerations:**
- Tag releases: `v1.0.0`, `v1.1.0`
- Customers pin to tags: `revision: v1.0.0` in their `west.yml`
- Maintain LTS branches for critical fixes

## Testing Integration

**No tests in this repo** - all unit tests live in `nrfmodule-core/tests/`.

**Why?**
- Tests require source code to run
- Customers with binaries-only can't run unit tests
- Integration tests happen in `nrfmodule-product-template`

**CI verification:**
- `nrfmodule-product-template` builds against this SDK
- Ensures binary compatibility and header correctness

## Common Pitfalls

1. **Forgetting to update binary after source changes** - headers and binary can drift
2. **Wrong architecture binary** - must match target (e.g., ARM Cortex-M33 for nRF9160)
3. **Path assumptions in CMakeLists.txt** - breaks if West structure changes
4. **Adding source files here** - defeats the purpose of separation
5. **Binary size bloat** - optimize compilation flags when generating `.a`

## Adding New APIs

**Checklist for new public functions:**

1. **Add declaration** in `include/modem_core.h`:
   ```c
   int modem_new_feature(void);
   ```

2. **Implement** in `nrfmodule-core/src/modem_core.c`:
   ```c
   int modem_new_feature(void) {
       // implementation
   }
   ```

3. **Test** in `nrfmodule-core/tests/`:
   ```c
   ZTEST(modem_suite, test_new_feature) {
       zassert_equal(modem_new_feature(), 0, "...");
   }
   ```

4. **Regenerate binary**:
   ```bash
   cd nrfmodule-core
   west build -b nrf9160dk_nrf9160_ns
   cp build/zephyr/libmodem_core.a ../nrfmodule-sdk/lib/
   ```

5. **Commit both repos** (separate PRs):
   - PR to `nrfmodule-core` with implementation + tests
   - PR to `nrfmodule-sdk` with header + updated binary

## Customer-Facing Documentation

**README.md should explain:**
- How to add this module to their `west.yml`
- Include path usage: `#include <modem_core.h>`
- API reference (or link to external docs)
- Binary architecture compatibility

**Example customer west.yml:**
```yaml
manifest:
  projects:
    - name: nrfmodule-sdk
      url: https://github.com/nrfmodule/nrfmodule-sdk
      revision: v1.0.0
      path: modules/lib/nrfmodule-sdk
```
