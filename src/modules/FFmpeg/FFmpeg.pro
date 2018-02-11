TEMPLATE = lib
CONFIG += plugin

greaterThan(QT_MAJOR_VERSION, 4) {
	lessThan(QT_VERSION, 5.7.0): CONFIG -= c++11
	QT += widgets
}
QT += network

win32|macx {
        DESTDIR = $${PWD}/../../../app/modules
        QMAKE_LIBDIR += $${PWD}/../../../app
}
else {
        DESTDIR = $${PWD}/../../../app/share/qmplay2/modules
        QMAKE_LIBDIR += $${PWD}/../../../app/lib
}
LIBS += $${PWD}/../../../app/libplaycore.a
include (../../../lib/lib.pri)
!ext_lib {
win32: LIBS += -lws2_32 -lavformat -lavcodec -lswscale -lavutil
else {
	macx: QT_CONFIG -= no-pkg-config
	CONFIG += link_pkgconfig
	PKGCONFIG += libavformat libavcodec libswscale libavutil
}
}


DEFINES += __STDC_CONSTANT_MACROS

RCC_DIR = build/rcc
OBJECTS_DIR = build/obj
MOC_DIR = build/moc

RESOURCES += icons.qrc

INCLUDEPATH += . ../../playcore/headers
DEPENDPATH += . ../../playcore/headers

HEADERS += FFmpeg.hpp FFDemux.hpp FFDec.hpp FFDecSW.hpp FFReader.hpp FFCommon.hpp FormatContext.hpp
SOURCES += FFmpeg.cpp FFDemux.cpp FFDec.cpp FFDecSW.cpp FFReader.cpp FFCommon.cpp FormatContext.cpp

unix:!macx:!android {
	PKGCONFIG += libavdevice
	DEFINES   += QMPlay2_libavdevice

#Common HWAccel
	HEADERS   += FFDecHWAccel.hpp HWAccelHelper.hpp
	SOURCES   += FFDecHWAccel.cpp HWAccelHelper.cpp

#VAAPI
	PKGCONFIG += libva libva-x11
	HEADERS   += FFDecVAAPI.hpp VAAPIWriter.hpp
	SOURCES   += FFDecVAAPI.cpp VAAPIWriter.cpp
	DEFINES   += QMPlay2_VAAPI

#VDPAU
	PKGCONFIG += vdpau
	HEADERS   += FFDecVDPAU.hpp VDPAUWriter.hpp
	SOURCES   += FFDecVDPAU.cpp VDPAUWriter.cpp
	DEFINES   += QMPlay2_VDPAU
#	HEADERS   += FFDecVDPAU_NW.hpp
#	SOURCES   += FFDecVDPAU_NW.cpp
#	DEFINES   += QMPlay2_VDPAU_NW
}
