cmake_minimum_required(VERSION 2.6)

project (encrust)

include_directories(
    include
    /usr/local/include/opencv
    /usr/include/SDL
    )

file(
    GLOB_RECURSE
    source_files
    src/*
    include/*
)

#add_executable(
#    bin/encrust
#    include/CFrameEncrustor.h
#    include/CImageBuilder.h
#    include/CSettingsReader.h
#    include/CVideoGrabber.h
#    include/CVideoManager.h
#    include/EncrustDefines.h
#    src/CFrameEncrustor.cpp
#    src/CImageBuilder.cpp
#    src/CSettingsReader.cpp
#    src/CVideoGrabber.cpp
#    src/CVideoManager.cpp
#    src/main.cpp
#)

add_executable(
    ../bin/encrust
    ${source_files}
)

target_link_libraries(
    ../bin/encrust
    opencv_core
    opencv_imgproc
    opencv_highgui
    tinyxml
    SDL
    SDL_Pango
    SDL_image
    boost_system
    boost_thread
    boost_date_time
    boost_filesystem
    avcodec
    avutil
    avformat
)


