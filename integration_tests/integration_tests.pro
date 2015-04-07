# This subproject implements integration tests. 
# This tests are launched on the whole world dataset.

# It is recommended to place tests here in the following cases:
# - tests are written to be launch on the whole world dataset;
# - tests covers significant number of subsystems;

TARGET = integration_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ../
DEPENDENCIES = map routing search storage indexer platform geometry coding base osrm jansson protobuf tomcrypt succinct

macx-*: LIBS *= "-framework Foundation" "-framework IOKit"

include($$ROOT_DIR/common.pri)

QT *= core

win32* : LIBS *= -lShell32


SOURCES += \
  ../testing/testingmain.cpp \
  osrm_route_test.cpp \
  osrm_turn_test.cpp \
  osrm_test_tools.cpp \

HEADERS += \
  osrm_test_tools.hpp \