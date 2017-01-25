QT += gui core

CONFIG += c++11

TARGET = ShadowMap
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ShadowMap.cpp \
    teapot.cpp \
    vboplane.cpp \
    torus.cpp \
    frustum.cpp

HEADERS += \
    ShadowMap.h \
    teapotdata.h \
    teapot.h \
    vboplane.h \
    torus.h \
    frustum.h

OTHER_FILES += \
    fshader.txt \
    vshader.txt

RESOURCES += \
    shaders.qrc

DISTFILES += \
    fshader.txt \
    vshader.txt
