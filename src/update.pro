#-------------------------------------------------
#
# Project created by QtCreator 2017-04-20T21:32:51
#
#-------------------------------------------------

QT       += core network
QT -= gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = NVNUD
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += main.cpp\
    App/canmsgproc.cpp \
    App/ipcmsgproc.cpp \
    Config/appinfo.cpp \
    Lib/Can/canstack.cpp \
    Lib/Ipc/ipcstack.cpp \
    Lib/Transport/transport.cpp \
    Log/log.cpp \
    Lib/Tcp/tcpserver.cpp \
    Lib/Tcp/tcpserversocket.cpp \
    Lib/Tcp/tcpclientsocket.cpp \
    Config/sysconfig.cpp

HEADERS  += \
    App/canmsgproc.h \
    App/ipcmsgproc.h \
    Comm/call_once.h \
    Comm/comm.h \
    Comm/singleton.h \
    Comm/worker.h \
    Config/appinfo.h \
    Lib/Can/canstack.h \
    Lib/Ipc/ipcstack.h \
    Lib/Transport/transport.h \
    Log/log.h \
    Lib/Tcp/tcpserver.h \
    Lib/Tcp/tcpserversocket.h \
    Lib/Tcp/tcpclientsocket.h \
    Lib/Ipc/ipcprotocol.h \
    Lib/Can/canprotocol.h \
    Comm/util.h \
    Config/sysconfig.h

FORMS    +=
