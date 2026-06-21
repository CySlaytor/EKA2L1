set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Gui REQUIRED)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  set(EXCLUSIVE_QT_COMPONENTS OpenGLWidgets)
else()
  set(EXCLUSIVE_QT_COMPONENTS OpenGL)
endif()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Network ${EXCLUSIVE_QT_COMPONENTS} REQUIRED)

set(PROJECT_SOURCES
    include/qt/btnmap/executor.h
    include/qt/dialog_driver.h
    include/qt/displaywidget.h
    include/qt/mainwindow.h
    include/qt/seh_handler.h
    include/qt/state.h
    include/qt/thread.h
    include/qt/utils.h
    src/btnmap/executor.cpp
    src/dialog_driver.cpp
    src/displaywidget.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/seh_handler.cpp
    src/state.cpp
    src/thread.cpp
    src/utils.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  qt_add_executable(eka2l1_qt MANUAL_FINALIZATION ${PROJECT_SOURCES})
else()
  if(ANDROID)
    add_library(eka2l1_qt SHARED ${PROJECT_SOURCES})
  else()
    add_executable(eka2l1_qt ${PROJECT_SOURCES})
  endif()
endif()

target_include_directories(eka2l1_qt PUBLIC include)

if (${QT_VERSION_MAJOR} EQUAL 6)
  target_include_directories(eka2l1_qt PRIVATE ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
else()
  target_include_directories(eka2l1_qt PRIVATE ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  set(EXCLUSIVE_QT_LIBRARIES Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
else()
  set(EXCLUSIVE_QT_LIBRARIES Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

target_link_libraries(eka2l1_qt PRIVATE
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Network
  ${EXCLUSIVE_QT_LIBRARIES}
  common config drivers epoc epocservs epockern gdbstub uvw stb yaml-cpp)

set_target_properties(eka2l1_qt PROPERTIES OUTPUT_NAME eka2l1_qt
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (WIN32)
  set_property(TARGET eka2l1_qt PROPERTY WIN32_EXECUTABLE true)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  qt_finalize_executable(eka2l1_qt)
endif()