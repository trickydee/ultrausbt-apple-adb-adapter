
add_library(adb
    adb.cpp
    adbkbdparser.cpp
    adbmouseparser.cpp
    usbtoadb.cpp
    )

if(ADB_HOST_MODE)
    target_sources(adb PRIVATE adb_host.cpp)
endif()

include_directories(../../misc/include)
target_link_libraries(adb QuokkADB)
target_include_directories(adb PUBLIC ../include)
target_compile_definitions(adb PRIVATE ADB_ATTENTION_LO_MIN_US=${ADB_ATTENTION_LO_MIN_US})
if(ADB_STRICT_DUTY_CYCLE_DECODE)
target_compile_definitions(adb PRIVATE ADB_STRICT_DUTY_CYCLE_DECODE=1)
else()
target_compile_definitions(adb PRIVATE ADB_STRICT_DUTY_CYCLE_DECODE=0)
endif()
if(ADB_STRICT_SYNC_WINDOW)
target_compile_definitions(adb PRIVATE ADB_STRICT_SYNC_WINDOW=1)
else()
target_compile_definitions(adb PRIVATE ADB_STRICT_SYNC_WINDOW=0)
endif()