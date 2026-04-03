include_directories(${PROJECT_SOURCE_DIR}/include/)

add_executable(BT-USB-ADB-Adapter-firmware main.cpp
  display/display.c
  bt_host_coop.c
  ${CMAKE_SOURCE_DIR}/ssd1306/ssd1306.c
)

target_include_directories(BT-USB-ADB-Adapter-firmware PRIVATE
  ${CMAKE_SOURCE_DIR}/ssd1306
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BT-USB-ADB-Adapter-firmware QuokkADB pico_stdlib hardware_i2c)

pico_add_extra_outputs(BT-USB-ADB-Adapter-firmware)