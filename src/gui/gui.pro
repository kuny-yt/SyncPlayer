TEMPLATE = app
!win32: CONFIG += link_pkgconfig

QT += network
greaterThan(QT_MAJOR_VERSION, 4) {
	lessThan(QT_VERSION, 5.7.0): CONFIG -= c++11
	QT += widgets
}
else:unix:!macx: PKGCONFIG += x11

TARGET = SyncPlayer

win32|macx {
	QMAKE_LIBDIR += ../../app
        DESTDIR = $$PWD/../../app
}
else {
	QMAKE_LIBDIR += ../../app/lib
        DESTDIR = $$PWD/../../app/bin
	!android: LIBS += -lrt #For glibc < 2.17
}
LIBS += $${PWD}/../../app/libplaycore.a
include (../../lib/lib.pri)
include (sync/sync.pri)

!ext_lib {
LIBS += -lqmplay2
}

RESOURCES += resources.qrc
win32 {
        #RC_FILE = Windows/icons.rc
	DEFINES -= UNICODE
}

OBJECTS_DIR = build/obj
MOC_DIR = build/moc
RCC_DIR = build/rcc

INCLUDEPATH += . ../playcore/headers
DEPENDPATH  += . ../playcore/headers

HEADERS += Main.hpp MenuBar.hpp MainWidget.hpp AddressBox.hpp VideoDock.hpp InfoDock.hpp PlaylistDock.hpp PlayClass.hpp DemuxerThr.hpp AVThread.hpp VideoThr.hpp AudioThr.hpp SettingsWidget.hpp OSDSettingsW.hpp DeintSettingsW.hpp OtherVFiltersW.hpp PlaylistWidget.hpp EntryProperties.hpp AboutWidget.hpp AddressDialog.hpp VideoEqualizer.hpp Appearance.hpp
SOURCES += Main.cpp MenuBar.cpp MainWidget.cpp AddressBox.cpp VideoDock.cpp InfoDock.cpp PlaylistDock.cpp PlayClass.cpp DemuxerThr.cpp AVThread.cpp VideoThr.cpp AudioThr.cpp SettingsWidget.cpp OSDSettingsW.cpp DeintSettingsW.cpp OtherVFiltersW.cpp PlaylistWidget.cpp EntryProperties.cpp AboutWidget.cpp AddressDialog.cpp VideoEqualizer.cpp Appearance.cpp

greaterThan(QT_MAJOR_VERSION, 4):qtHaveModule(x11extras) {
	DEFINES += X11_EXTRAS
	QT += x11extras
	PKGCONFIG += x11
}

DEFINES += QMPlay2_TagEditor
HEADERS += TagEditor.hpp
SOURCES += TagEditor.cpp
win32 {
        #DEFINES += UPDATER
        #HEADERS += Updater.hpp
        #SOURCES += Updater.cpp

        DEFINES += TAGLIB_STATIC TAGLIB_FULL_INCLUDE_PATH
}
else {
	macx: QT_CONFIG -= no-pkg-config
	PKGCONFIG += taglib
}
