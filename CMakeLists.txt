cmake_minimum_required(VERSION 3.16)
project(EchoBoxII VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Network)

qt_add_executable(EchoBoxII
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/visualizer.cpp
    src/visualizer.h
    src/backgroundwidget.cpp
    src/backgroundwidget.h
    src/logo.h
    src/icons.h
    src/discordrpc.h
    src/discordrpc.cpp
    src/settingsdialog.h
    src/settingsdialog.cpp
    src/waveformslider.h
    src/waveformslider.cpp
    src/libraryscanner.h
    src/libraryscanner.cpp
)

target_link_libraries(EchoBoxII PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
)

set_target_properties(EchoBoxII PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE   TRUE
)
