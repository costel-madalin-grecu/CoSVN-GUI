#-------------------------------------------------
#
# Project created by QtCreator 2015-12-25T21:39:32
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QMAKE_CXXFLAGS += -std=c++11

TARGET = CoSVN-GUI
TEMPLATE = app


SOURCES += main.cpp \
    Settings/AppSettings.cpp \
    Gui/AboutDialog.cpp \
    Gui/ChooseRepoDialog.cpp \
    Gui/CommitDialog.cpp \
    Gui/MainWindow.cpp \
    Gui/StatusDialog.cpp \
    Logger/Logger.cpp \
    Repos/SVN/SvnViewer.cpp \

HEADERS += \
    Settings/AppSettings.h \
    Repos/SVN/SvnCommands.h \
    Repos/SVN/SvnViewer.h \
    Gui/AboutDialog.h \
    Gui/ChooseRepoDialog.h \
    Gui/CommitDialog.h \
    Gui/CommonUI.h \
    Gui/MainWindow.h \
    Gui/StatusDialog.h \
    Logger/Logger.h

FORMS += \
    Gui/StatusDialog.ui \
    Gui/MainWindow.ui \
    Gui/CommitDialog.ui \
    Gui/ChooseRepoDialog.ui \
    Gui/AboutDialog.ui

RESOURCES += \
    Resources/Resources.qrc


