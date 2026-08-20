# ============================================================
# tests/CMakeLists.txt - 单元测试（阶段 6）
# 仅编译纯逻辑模块（帧编解码/载荷解析/协议组包/状态/地理算法），
# 不链接 WebEngine / FFmpeg，构建轻量、可离线运行。
# ============================================================

# 将测试文件所在目录加入编译，便于复用公共宏
set(TEST_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../src")

# 公共辅助：创建单个 Qt Test 可执行文件
function(tjson_add_test target_name)
    set(multi_value_args SOURCES)
    cmake_parse_arguments(TEST "" "" "${multi_value_args}" ${ARGN})

    qt_add_executable(${target_name} ${TEST_SOURCES})
    target_include_directories(${target_name} PRIVATE ${TEST_INCLUDE_DIR})
    target_link_libraries(${target_name} PRIVATE Qt::Core Qt::Test)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8)
    endif()
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

# ---- 1. 帧编解码器：粘包/半包/重同步/组包 ----
tjson_add_test(test_tjsonframecodec
    SOURCES
        test_tjsonframecodec.cpp
        ../src/infrastructure/tjsonframecodec.cpp
)

# ---- 2. 载荷解析器：JSON/ACK/抓拍帧 ----
tjson_add_test(test_jsonframeparser
    SOURCES
        test_jsonframeparser.cpp
        ../src/infrastructure/tjsonprotocolparser.cpp
)

# ---- 3. 协议组包：Pelco-D checksum / VISCA ----
tjson_add_test(test_protocolbuilder
    SOURCES
        test_protocolbuilder.cpp
)

# ---- 4. 状态模型：DeviceState 字段更新 ----
tjson_add_test(test_devicestate
    SOURCES
        test_devicestate.cpp
)

# ---- 5. 地理算法：parseCoord/距离/航向/抽稀 ----
tjson_add_test(test_geocalc
    SOURCES
        test_geocalc.cpp
        ../src/core/GeoCalculator.cpp
)

# ---- 6. MODBUS 传输：CRC16 校验 ----
tjson_add_test(test_modbustransport
    SOURCES
        test_modbustransport.cpp
        ../src/infrastructure/modbustransport.cpp
)
target_link_libraries(test_modbustransport PRIVATE Qt::SerialPort)

# ---- 7. STM32-TCP 传输：初始状态 / 未连接 send ----
tjson_add_test(test_stm32tcptransport
    SOURCES
        test_stm32tcptransport.cpp
        ../src/infrastructure/stm32tcptransport.cpp
)
target_link_libraries(test_stm32tcptransport PRIVATE Qt::Network)