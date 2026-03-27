
add_library(usb
    char2usbkeycode.cpp
    usbkbdparser.cpp
    usbmouseparser.cpp
    )

    include_directories(../../misc/include)
    target_link_libraries(usb QuokkADB)
    target_include_directories(usb PUBLIC ../include)

if(ADB_MOUSE_ACCUMULATE_DELTAS)
    target_compile_definitions(usb PRIVATE ADB_MOUSE_ACCUMULATE_DELTAS=1)
else()
    target_compile_definitions(usb PRIVATE ADB_MOUSE_ACCUMULATE_DELTAS=0)
endif()