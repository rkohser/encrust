#-------------------------------------------------
#
# Project created by QtCreator 2011-10-28T20:50:15
# and managed with git
# just to test
#
#-------------------------------------------------

QT       -= core
QT       -= gui

TARGET = encrust

CONFIG   += console
CONFIG   -= app_bundle
CONFIG   -= qt

TEMPLATE = app

OBJECTS_DIR = obj
DESTDIR     = bin


SOURCES += \
    src/main.cpp \
    src/CSettingsReader.cpp \
    src/CImageBuilder.cpp \
    #src/CVideoGrabber.cpp \
    src/CFrameEncrustor.cpp \
    src/CVideoManager.cpp \

HEADERS += \
    include/CSettingsReader.h \
    include/CImageBuilder.h \
    #include/CVideoGrabber.h \
    include/CFrameEncrustor.h \
    include/EncrustDefines.h \
    include/CVideoManager.h

INCLUDEPATH +=   \
    include \
    /usr/include/SDL

#opencv
#INCLUDEPATH += /usr/include/opencv
#LIBS        += -lcv  -lhighgui -lcxcore

INCLUDEPATH += /usr/local/include/opencv
#LIBS        += -L/usr/local/lib
LIBS        += -lopencv_core  -lopencv_imgproc -lopencv_highgui

LIBS += -ltinyxml
LIBS += -lSDL -lSDL_Pango -lSDL_image
LIBS += -lboost_system -lboost_thread -lboost_date_time -lboost_filesystem
LIBS += -lavcodec -lavutil -lavformat
