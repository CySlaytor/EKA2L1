add_library(epoc
    include/system/devices.h
    include/system/epoc.h
    include/system/hal.h
    include/system/software.h
    src/devices.cpp
    src/epoc.cpp
    src/hal.cpp
    src/software.cpp)

target_include_directories(epoc PUBLIC include)
target_link_libraries(epoc PRIVATE
    bridge
    cpu
    common
    config
    disasm
    drivers
    epocdispatch
    epockern
    epocio
    epocloader
    epocservs
    FAT
    gdbstub
    miniz
    pugixml)

# Restore the conditional scripting link rule to resolve scripting include errors:
if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
    target_link_libraries(epoc PUBLIC scripting)
endif()