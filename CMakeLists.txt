cmake_minimum_required(VERSION 3.19)
project(T-JSON LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Gui)

qt_standard_project_setup()

# 强制使用 UTF-8 编码编译，解决 MSVC 下中文字符串乱码问题
if(MSVC)
    add_compile_options(/utf-8)
endif()

qt_add_executable(T-JSON
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    tjsonclient.cpp
    tjsonclient.h
    devicecontroller.cpp
    devicecontroller.h
    configmanager.cpp
    configmanager.h
    settingsdialog.cpp
    settingsdialog.h
    settingsdialog.ui
    resources.qrc
)

target_link_libraries(T-JSON
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::Gui
)

install(TARGETS T-JSON
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET T-JSON
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
