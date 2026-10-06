cmake_minimum_required(VERSION 3.16)

# 让 windres 与 C++ 编译器保持同目录，避免 PATH 中其他 MinGW 的 windres 被选中
if(WIN32 AND CMAKE_CXX_COMPILER AND NOT DEFINED CMAKE_RC_COMPILER)
    get_filename_component(_cxx_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    if(EXISTS "${_cxx_bin_dir}/windres.exe")
        set(CMAKE_RC_COMPILER "${_cxx_bin_dir}/windres.exe")
    endif()
endif()

project(VAMergeSplit VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

qt_standard_project_setup()

qt_add_executable(VAMergeSplit
    WIN32
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/SplitWidget.h
    src/SplitWidget.cpp
    src/MergeWidget.h
    src/MergeWidget.cpp
    src/SegmentModel.h
    src/SegmentModel.cpp
    src/FFmpegRunner.h
    src/FFmpegRunner.cpp
    src/MediaInfo.h
    src/MediaInfo.cpp
    src/TimeUtils.h
    src/TimeUtils.cpp
    src/WaveformWidget.h
    src/WaveformWidget.cpp
    resources/resources.qrc
    resources/app.rc
)

target_link_libraries(VAMergeSplit PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)
