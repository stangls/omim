# Generator binary

ROOT_DIR = ../..

DEPENDENCIES = generator routing search storage indexer editor platform geometry \
               coding base freetype expat fribidi tomcrypt jansson protobuf osrm stats_client \
               minizip succinct pugixml tess2 gflags oauthcpp 
include($$ROOT_DIR/common.pri)

INCLUDEPATH *= $$ROOT_DIR/3party/gflags/src

CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

# needed for Platform::WorkingDir() and unicode combining
QT *= core

macx-* {
  LIBS *= "-framework IOKit" "-framework SystemConfiguration"
}

SOURCES += \
    generator_tool.cpp \

HEADERS += \
