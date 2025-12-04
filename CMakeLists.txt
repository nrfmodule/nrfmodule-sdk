zephyr_library()
zephyr_include_directories(include)

get_filename_component(WEST_TOP "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(PRIVATE_SRC "${WEST_TOP}/modules/lib/nrfmodule-core")

# 1. Check if Zephyr already loaded the module (via module.yml or -x)
if(TARGET private_modem_lib)
    message(STATUS " [TEST] Source Module auto-loaded. No manual linking required.")

# 2. If not loaded, load manually (Public user with source access)
elseif(EXISTS "${PRIVATE_SRC}/CMakeLists.txt")
    message(STATUS " [TEST] Loading module manually from ${PRIVATE_SRC}")
    add_subdirectory(${PRIVATE_SRC} ${CMAKE_CURRENT_BINARY_DIR}/private_src)
    # We might need to link manually here depending on Zephyr version
    # zephyr_link_libraries(private_modem_lib)

# 3. Fallback: Binary
else()
    message(STATUS " [TEST] Source missing. Linking PUBLIC BINARY")
    zephyr_link_libraries(${CMAKE_CURRENT_LIST_DIR}/lib/libmodem_core.a)
endif()