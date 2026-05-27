zephyr_library()
zephyr_include_directories(include)

get_filename_component(WEST_TOP "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(PRIVATE_SRC "${WEST_TOP}/modules/lib/nrfmodule-core")

# 1. Check if Zephyr already loaded the module (via module.yml or -x)
if(TARGET private_modem_lib)
    message(STATUS " [nrfmodule] Source Module auto-loaded. No manual linking required.")

# 2. If not loaded, load manually (Public user with source access)
elseif(EXISTS "${PRIVATE_SRC}/CMakeLists.txt")
    message(STATUS " [nrfmodule] Loading module manually from ${PRIVATE_SRC}")
    add_subdirectory(${PRIVATE_SRC} ${CMAKE_CURRENT_BINARY_DIR}/private_src)

# 3. Fallback: Binary (only needed for modem builds without source)
else()
    if(CONFIG_NRF_MODEM_CLIENT)
        message(STATUS " [nrfmodule] Source missing. Linking PUBLIC BINARY")
        zephyr_link_libraries(${CMAKE_CURRENT_LIST_DIR}/lib/libmodem_core.a)
    else()
        message(STATUS " [nrfmodule] BLE-only build (no modem client)")
    endif()
endif()

# Register linker sections for modem callbacks (init, shutdown, etc.)
if(CONFIG_NRF_MODEM_CLIENT)
    zephyr_linker_sources(DATA_SECTIONS ${CMAKE_CURRENT_LIST_DIR}/zephyr/linker_data.ld)
endif()
