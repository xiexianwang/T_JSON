cmake_minimum_required(VERSION 3.19)

# JOM 生成器在中文 Windows 上会写出 UTF-8 的 /showIncludes 前缀，
# 而 cl.exe 实际输出 GBK，导致头文件依赖跟踪失效（改头文件不重编 → 陈旧 obj → 崩溃）。
# 强制 CMake 使用自带依赖扫描器，绕开该编码不匹配问题。
set(CMAKE_DEPENDS_USE_COMPILER FALSE)

project(T-JSON LANGUAGES CXX)

list(APPEND CMAKE_PREFIX_PATH "D:/Coding/Qt/6.11.2/msvc2022_64")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Gui WebEngineWidgets WebChannel SerialPort Test)
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
    src/core/GeoCalculator.cpp
    src/core/GeoCalculator.h
    src/core/JsonFrameParser.cpp
    src/core/JsonFrameParser.h
    src/core/DeviceState.h
    src/core/DeviceConfig.h
    src/core/DeviceConfig.cpp
    src/core/DeviceEntry.h
    src/core/DeviceEntry.cpp
    src/core/DeviceTypes.h
    src/core/EventBus.cpp
    src/core/EventBus.h
    src/service/DeviceContext.cpp
    src/service/DeviceContext.h
    src/service/DeviceManager.cpp
    src/service/DeviceManager.h
    src/ui/main/MainPresenter.cpp
    src/ui/main/MainPresenter.h
    src/ui/main/DeviceSessionState.cpp
    src/ui/main/DeviceSessionState.h
    src/ui/main/DeviceSessionService.cpp
    src/ui/main/DeviceSessionService.h
    src/ui/main/DeviceStateService.cpp
    src/ui/main/DeviceStateService.h
    src/ui/main/PresenterStateViewService.cpp
    src/ui/main/PresenterStateViewService.h
    src/ui/main/PresenterAiViewService.cpp
    src/ui/main/PresenterAiViewService.h
    src/ui/main/PresenterMediaService.cpp
    src/ui/main/PresenterMediaService.h
    src/ui/main/PresenterDeviceService.cpp
    src/ui/main/PresenterDeviceService.h
    src/ui/main/PresenterMotorService.cpp
    src/ui/main/PresenterMotorService.h
    src/ui/main/DeviceControlService.cpp
    src/ui/main/DeviceControlService.h
    src/ui/main/PresenterMapService.cpp
    src/ui/main/PresenterMapService.h
    src/ui/views/MainWindowNavigation.cpp
    src/ui/views/MainWindowNavigation.h
    src/ui/views/MainWindowDialogService.cpp
    src/ui/views/MainWindowDialogService.h
    src/ui/views/MainWindowLayoutService.cpp
    src/ui/views/MainWindowLayoutService.h
    src/ui/views/MainWindowSystemService.cpp
    src/ui/views/MainWindowSystemService.h
    src/ui/views/MainWindowControlService.cpp
    src/ui/views/MainWindowControlService.h
    src/ui/components/VideoGridWidget.cpp
    src/ui/components/VideoGridWidget.h
    src/ui/components/DeviceTreeWidget.cpp
    src/ui/components/DeviceTreeWidget.h
    src/ui/views/mainwindow.cpp
    src/ui/views/mainwindow.h
    src/ui/views/mainwindow_imainview.cpp
    ui/mainwindow.ui
    src/infrastructure/tjsonframe.h
    src/infrastructure/tjsonframecodec.cpp
    src/infrastructure/tjsonframecodec.h
    src/infrastructure/tjsonprotocolparser.cpp
    src/infrastructure/tjsonprotocolparser.h
    src/infrastructure/tjsonclient.cpp
    src/infrastructure/tjsonclient.h
    src/infrastructure/pelcodprotocol.h
    src/infrastructure/viscaprotocol.h
    src/infrastructure/modbustransport.cpp
    src/infrastructure/modbustransport.h
    src/infrastructure/stm32tcptransport.cpp
    src/infrastructure/stm32tcptransport.h
    src/infrastructure/devicecommandservice.cpp
    src/infrastructure/devicecommandservice.h
    src/infrastructure/devicecontroller.cpp
    src/infrastructure/devicecontroller.h
    src/infrastructure/configmanager.cpp
    src/infrastructure/configmanager.h
    src/ui/views/devicepropertiesdialog.cpp
    src/ui/views/devicepropertiesdialog.h
    src/ui/views/wheelredirectfilter.cpp
    src/ui/views/wheelredirectfilter.h
    ui/mapwidget.ui
    src/ui/views/videowidget.cpp
    src/ui/views/videowidget.h
    src/infrastructure/rtspthread.cpp
    src/infrastructure/rtspthread.h
    src/ui/views/mapwidget.cpp
    src/ui/views/mapwidget.h
    src/ui/views/mapbridge.cpp
    src/ui/views/mapbridge.h
    src/ui/views/cmdlogdialog.cpp
    src/ui/views/cmdlogdialog.h
    src/infrastructure/ptzforwarder.cpp
    src/infrastructure/ptzforwarder.h
    resources/resources.qrc
    resources/app.rc
)

# 重命名输出 exe（任务管理器显示为 LSSVideoManager.exe）
set_target_properties(T-JSON PROPERTIES OUTPUT_NAME "LSSVideoManager")

target_include_directories(T-JSON PRIVATE "${FFMPEG_INCLUDE_DIR}" src)

target_link_libraries(T-JSON
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::Gui
        Qt::WebEngineWidgets
        Qt::WebChannel
        Qt::SerialPort
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

# ── 测试与构建体系（阶段 6） ──
# 单元测试目标独立于主程序，仅编译纯逻辑模块，避免依赖 WebEngine/FFmpeg。
# 通过 -DBUILD_TESTING=OFF 可完全跳过测试。
include(CTest)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
