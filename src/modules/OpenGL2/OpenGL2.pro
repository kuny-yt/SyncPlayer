TEMPLATE = lib
CONFIG += plugin

greaterThan(QT_MAJOR_VERSION, 4) {
	lessThan(QT_VERSION, 5.7.0): CONFIG -= c++11
	QT += widgets
}

win32|macx {
        DESTDIR = $${PWD}/../../../app/modules
	QMAKE_LIBDIR += ../../../app
}
else {
        DESTDIR = $${PWD}/../../../app/share/qmplay2/modules
	QMAKE_LIBDIR += ../../../app/lib
}
LIBS += $${PWD}/../../../app/libplaycore.a
include (../../../lib/lib.pri)

!ext_lib {
LIBS += -lqmplay2
}

RCC_DIR = build/rcc
OBJECTS_DIR = build/obj
MOC_DIR = build/moc

RESOURCES += icon.qrc

INCLUDEPATH += . ../../playcore/headers
DEPENDPATH += . ../../playcore/headers

HEADERS += OpenGL2.hpp OpenGL2Writer.hpp OpenGL2Common.hpp
SOURCES += OpenGL2.cpp OpenGL2Writer.cpp OpenGL2Common.cpp

equals(QT_VERSION, 5.6.0)|greaterThan(QT_VERSION, 5.6.0) {
	DEFINES += OPENGL_NEW_API VSYNC_SETTINGS
	HEADERS += OpenGL2Window.hpp OpenGL2Widget.hpp OpenGL2CommonQt5.hpp
	SOURCES += OpenGL2Window.cpp OpenGL2Widget.cpp OpenGL2CommonQt5.cpp
} else {
	QT += opengl
	DEFINES += DONT_RECREATE_SHADERS
	HEADERS += OpenGL2OldWidget.hpp
	SOURCES += OpenGL2OldWidget.cpp
	win32|unix:!macx:!android:!contains(QT_CONFIG, opengles2): DEFINES += VSYNC_SETTINGS
}

contains(QT_CONFIG, opengles2): DEFINES += OPENGL_ES2
