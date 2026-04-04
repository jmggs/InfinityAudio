cmake_minimum_required(VERSION 3.21)
project(InfinityAudio VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Gui Multimedia Network)

add_executable(InfinityAudio
    main.cpp
    wav_writer.cpp
    wav_writer.h
    recorder.cpp
    recorder.h
    watchdog.cpp
    watchdog.h
    ui_widget.cpp
    ui_widget.h
    web_server.cpp
    web_server.h
    vu_meter_widget.cpp
    vu_meter_widget.h
)

target_include_directories(InfinityAudio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(InfinityAudio PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Multimedia
    Qt6::Network
)

if(WIN32)
    target_compile_definitions(InfinityAudio PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(InfinityAudio PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS InfinityAudio
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/infinityaudio.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    RENAME InfinityAudio.desktop
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/infinityaudio.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
)

set(CPACK_PACKAGE_NAME "infinityaudio")
set(CPACK_PACKAGE_VENDOR "InfinityAudio")
set(CPACK_PACKAGE_CONTACT "jmggs")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "InfinityAudio continuous WAV recorder")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
