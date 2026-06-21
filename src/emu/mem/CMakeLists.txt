add_library(epocmem
        include/mem/mem.h
        include/mem/allocator/std_page_allocator.h
        include/mem/model/multiple/control.h
        include/mem/model/multiple/chunk.h
        include/mem/model/multiple/mmu.h
        include/mem/model/multiple/process.h
        include/mem/model/section.h
        include/mem/chunk.h
        include/mem/common.h
        include/mem/control.h
        include/mem/mmu.h
        include/mem/page.h
        include/mem/process.h
        include/mem/ptr.h
        src/mem.cpp
        src/allocator/std_page_allocator.cpp
        src/model/multiple/control.cpp
        src/model/multiple/chunk.cpp
        src/model/multiple/mmu.cpp
        src/model/multiple/process.cpp
        src/chunk.cpp
        src/control.cpp
        src/mmu.cpp
        src/page.cpp
        src/process.cpp
        )

target_include_directories(epocmem PUBLIC include)

target_link_libraries(epocmem PUBLIC common)
target_link_libraries(epocmem PRIVATE cpu config)
