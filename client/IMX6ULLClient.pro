QT += core gui widgets network

CONFIG += c++11

TARGET = IMX6ULLClient
TEMPLATE = app

SOURCES += src/main.cpp src/protocolutil.cpp src/networkmanager.cpp src/mainwindow.cpp src/settingsdialog.cpp src/filemanagerwindow.cpp src/systemmonitorwindow.cpp src/transferprogressdialog.cpp src/logviewerwindow.cpp src/fileviewerdialog.cpp

HEADERS += include/protocolutil.h include/networkmanager.h include/mainwindow.h include/settingsdialog.h include/filemanagerwindow.h include/systemmonitorwindow.h include/transferprogressdialog.h include/logviewerwindow.h include/fileviewerdialog.h

FORMS += ui/mainwindow.ui ui/settingsdialog.ui ui/filemanagerwindow.ui ui/systemmonitorwindow.ui ui/transferprogressdialog.ui ui/logviewerwindow.ui

INCLUDEPATH += . include ../protocol

win32 {
    LIBS += -lws2_32 -lwinmm
}
