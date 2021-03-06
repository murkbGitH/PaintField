
include(../src.pri)
include(../sse2-support.pri)

macx {
	PF_PLATFORM = "mac"
}

unix:!macx {
	PF_PLATFORM = "unix"
}

windows {
	PF_PLATFORM = "windows"
}

QT += core gui network xml svg widgets
CONFIG += c++11

INCLUDEPATH += $$PWD/.. $$PWD/../libs $$PWD/../libs/Malachite/include $$PWD/../libs/amulet/include

# defining a function that returns the relative path from 2 paths
# note that the head of a result is always '/'
defineReplace(relativePathFrom) {
	
	path_to = $$1
	path_from = $$2
	
	path_to_from = $$replace(path_to, ^$${path_from}, )
	
	contains(path_to_from, ^$${path_to}$) {
		path_to_from = $$replace(path_from, ^$$path_to, )
		path_to_from = $$replace(path_to_from, [^/]+, ..)
	}
	
	return($$path_to_from)
}

PF_OUT_PWD = $$OUT_PWD$$relativePathFrom($$PWD, $$_PRO_FILE_PWD_)

CONFIG(debug, debug|release) {
	PF_DEBUG_OR_RELEASE = debug
} else {
	PF_DEBUG_OR_RELEASE = release
}

win32 {
	PF_OUT_SUBDIR = $$PF_DEBUG_OR_RELEASE
} else {
	PF_OUT_SUBDIR =
}

LIBS += -L$$PF_OUT_PWD/../libs/Malachite/src/$$PF_OUT_SUBDIR
LIBS += -L$$PF_OUT_PWD/../libs/minizip/$$PF_OUT_SUBDIR
LIBS += -L$$PF_OUT_PWD/../libs/qtsingleapplication/$$PF_OUT_SUBDIR
LIBS += -lfreeimage -lmalachite -lpaintfield-minizip -lpaintfield-qtsingleapplication
