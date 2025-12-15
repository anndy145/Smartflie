cmake_minimum_required(VERSION 3.16)

project(SmartFileOrganizer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Network Sql)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WIN32_WINNT=0x0A00 -D_Frees_ptr_opt_=") # Force define for C
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32_WINNT=0x0A00 -D_Frees_ptr_opt_=") # Force define for C++
    link_libraries(ws2_32)
endif()

# === Vision Dependencies (Auto-downloaded by setup_vision_deps.ps1) ===
set(LIBS_DIR "${CMAKE_SOURCE_DIR}/libs")

# 1. OpenCV
# set(OpenCV_DIR "${LIBS_DIR}/OpenCV-MinGW-Build-OpenCV-4.5.5-x64/x64/mingw/lib") # Hint for find_package
# set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${LIBS_DIR}/OpenCV-MinGW-Build-OpenCV-4.5.5-x64/x64/mingw")

find_package(OpenCV QUIET)
if (OpenCV_FOUND)
    message(STATUS "Found OpenCV: ${OpenCV_VERSION}")
else()
    message(WARNING "OpenCV not found! Vision features will be disabled. Run setup_vision_deps.ps1")
endif()

# 2. ONNX Runtime
set(ONNX_ROOT "${LIBS_DIR}/onnxruntime-win-x64-1.16.3")
if(EXISTS "${ONNX_ROOT}/include/onnxruntime_c_api.h")
    message(STATUS "Found ONNX Runtime at: ${ONNX_ROOT}")
    set(ONNX_FOUND ON)
    include_directories("${ONNX_ROOT}/include")
    link_directories("${ONNX_ROOT}/lib")
else()
    message(WARNING "ONNX Runtime not found! Run setup_vision_deps.ps1")
    set(ONNX_FOUND OFF)
endif()

include(FetchContent)

# Set llama.cpp options before MakeAvailable
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    llama_cpp
    GIT_REPOSITORY https://github.com/ggerganov/llama.cpp
    GIT_TAG        b3196 # Stable release June 2024
)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/releases/download/2.2.0/miniz-2.2.0.zip
)

FetchContent_MakeAvailable(llama_cpp json miniz)

add_executable(SmartFileOrganizer
    src/main.cpp
    src/gui/MainWindow.cpp
    src/gui/MainWindow.h
    src/gui/GraphWidget.cpp
    src/gui/GraphWidget.h
    src/core/FileScanner.cpp
    src/core/FileScanner.h
    src/core/TagManager.cpp
    src/core/TagManager.h
    src/ai/LlamaEngine.cpp
    src/ai/LlamaEngine.h
    src/ai/VisionEngine.cpp
    src/ai/VisionEngine.h
    src/core/DocumentParser.cpp
    src/core/DocumentParser.h
    src/core/DirectoryWatcher.cpp
    src/core/DirectoryWatcher.h
    src/core/DatabaseManager.cpp
    src/core/DatabaseManager.h
    src/core/VectorIndex.cpp
    src/core/VectorIndex.h
    ${miniz_SOURCE_DIR}/miniz.c

)

target_include_directories(SmartFileOrganizer PRIVATE 
    ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/vendor
    ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/common
    ${miniz_SOURCE_DIR}
    src/vendor/hnswlib
)

target_link_libraries(SmartFileOrganizer PRIVATE Qt6::Widgets Qt6::Concurrent Qt6::Network Qt6::Sql llama nlohmann_json::nlohmann_json mingw32)

if(OpenCV_FOUND)
    target_link_libraries(SmartFileOrganizer PRIVATE ${OpenCV_LIBS})
    target_compile_definitions(SmartFileOrganizer PRIVATE ENABLE_OPENCV)
endif()

if(ONNX_FOUND)
    target_link_libraries(SmartFileOrganizer PRIVATE onnxruntime)
    target_compile_definitions(SmartFileOrganizer PRIVATE ENABLE_ONNX)
endif()

if(WIN32)
    set_property(TARGET SmartFileOrganizer PROPERTY WIN32_EXECUTABLE ON)
endif()
