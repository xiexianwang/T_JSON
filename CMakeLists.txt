cmake_minimum_required(VERSION 3.19)
project(T-JSON LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Gui WebEngineWidgets WebChannel)
# Compute Qt binary directory from Qt6_DIR
get_filename_component(QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)

qt_standard_project_setup()
set(CMAKE_AUTOUIC_SEARCH_PATHS "ui")
set(CMAKE_AUTORCC ON)

# 强制使用 UTF-8 编码编译，解决 MSVC 下中文字符串乱码问题
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---- FFmpeg ----
set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/thirdparty/ffmpeg" CACHE PATH "FFmpeg root directory")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_DIR}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_DIR}/lib")

set(FFMPEG_LIBS
    "${FFMPEG_LIB_DIR}/avformat.lib"
    "${FFMPEG_LIB_DIR}/avcodec.lib"
    "${FFMPEG_LIB_DIR}/avutil.lib"
    "${FFMPEG_LIB_DIR}/swscale.lib"
)

qt_add_executable(T-JSON
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    ui/mainwindow.ui
    src/tjsonclient.cpp
    src/tjsonclient.h
    src/devicecontroller.cpp
    src/devicecontroller.h
    src/configmanager.cpp
    src/configmanager.h
    src/settingsdialog.cpp
    src/settingsdialog.h
    ui/settingsdialog.ui
    ui/mapwidget.ui
    src/videowidget.cpp
    src/videowidget.h
    src/rtspthread.cpp
    src/rtspthread.h
    src/mapwidget.cpp
    src/mapwidget.h
    src/mapbridge.cpp
    src/mapbridge.h
    src/cmdlogdialog.cpp
    src/cmdlogdialog.h
    resources/resources.qrc
    resources/app.rc
)

# 重命名输出 exe（任务管理器显示为 LSSVideoManager.exe）
set_target_properties(T-JSON PROPERTIES OUTPUT_NAME "LSSVideoManager")

target_include_directories(T-JSON PRIVATE "${FFMPEG_INCLUDE_DIR}")

target_link_libraries(T-JSON
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::Gui
        Qt::WebEngineWidgets
        Qt::WebChannel
        ${FFMPEG_LIBS}
)

# ── Copy FFmpeg DLLs to output directory ──
if(WIN32)
    set(FFMPEG_DLL_DIR "${FFMPEG_DIR}/bin")
    file(GLOB FFMPEG_DLLS "${FFMPEG_DLL_DIR}/*.dll")
    foreach(dll ${FFMPEG_DLLS})
        add_custom_command(TARGET T-JSON POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:T-JSON>"
            COMMENT "Copying ${dll}"
        )
    endforeach()

    # ── Deploy Qt runtimes (WebEngine, WebChannel, etc.) ──
    if(EXISTS "${QT_BIN_DIR}/windeployqt6.exe")
        add_custom_command(TARGET T-JSON POST_BUILD
            COMMAND "${QT_BIN_DIR}/windeployqt6" "$<TARGET_FILE_DIR:T-JSON>/LSSVideoManager.exe" --verbose 0
            COMMENT "Deploying Qt runtimes via windeployqt"
        )
    endif()
endif()

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
