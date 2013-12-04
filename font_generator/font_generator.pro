#-------------------------------------------------
#
# Project created by QtCreator 2014-05-29T10:57:50
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

ROOT_DIR = ..
include($$ROOT_DIR/common.pri)
INCLUDEPATH *= $$ROOT_DIR/3party/boost
TARGET = FontGenerator
TEMPLATE = app

SOURCES += main.cpp\
        widget.cpp \
    engine.cpp \
    qlistmodel.cpp \
    BinPacker.cpp \
    df_map.cpp \
    image.cpp

HEADERS  += widget.hpp \
    engine.hpp \
    qlistmodel.hpp \
    stb_tt.hpp \
    BinPacker.hpp \
    df_map.hpp \
    image.h

FORMS    += widget.ui