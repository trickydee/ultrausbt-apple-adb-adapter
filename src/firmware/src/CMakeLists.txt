include_directories(${PROJECT_SOURCE_DIR}/include/)

set(FIRMWARE_SRCS
  main.cpp
  display/display.c
  bt_host_coop.c
  ${CMAKE_SOURCE_DIR}/ssd1306/ssd1306.c
)

if(ADB_HOST_MODE)
  list(APPEND FIRMWARE_SRCS adb_mode.cpp adb_to_usb.cpp usb_hid_device.c)
endif()

add_executable(ultrausbt-Apple-ADB-adapter-firmware ${FIRMWARE_SRCS})

target_include_directories(ultrausbt-Apple-ADB-adapter-firmware PRIVATE
  ${CMAKE_SOURCE_DIR}/ssd1306
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ultrausbt-Apple-ADB-adapter-firmware QuokkADB pico_stdlib hardware_i2c)

if(ADB_HOST_MODE)
  target_compile_definitions(ultrausbt-Apple-ADB-adapter-firmware PRIVATE ADB_HOST_MODE=1)
endif()

pico_add_extra_outputs(ultrausbt-Apple-ADB-adapter-firmware)