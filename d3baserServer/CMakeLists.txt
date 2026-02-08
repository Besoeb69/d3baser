cmake_minimum_required(VERSION 3.16)

project(d3baserServer LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core LinguistTools Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core LinguistTools Network)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(OPUS QUIET opus)
endif()

if(NOT OPUS_FOUND)
    find_path(OPUS_INCLUDE_DIR opus/opus.h
        PATHS
            "C:/msys64/mingw64/include"
            ENV INCLUDE
    )
    find_library(OPUS_LIBRARY NAMES opus
        PATHS
            "C:/msys64/mingw64/lib"
            ENV LIB
    )

    if(OPUS_INCLUDE_DIR AND OPUS_LIBRARY)
        set(OPUS_FOUND TRUE)
        set(OPUS_INCLUDE_DIRS ${OPUS_INCLUDE_DIR})
        set(OPUS_LIBRARIES ${OPUS_LIBRARY})
    endif()
endif()

if(NOT OPUS_FOUND)
    message(FATAL_ERROR "libopus not found!")
endif()

set(TS_FILES d3baserServer_en_US.ts)

add_executable(d3baserServer
  main.cpp
  ${TS_FILES}
  server.cpp
  server.h
)
target_link_libraries(d3baserServer
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    ${OPUS_LIBRARIES}
)

target_include_directories(d3baserServer PRIVATE
    ${OPUS_INCLUDE_DIRS}
)

if(COMMAND qt_create_translation)
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

include(GNUInstallDirs)
install(TARGETS d3baserServer
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_compile_definitions(_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
