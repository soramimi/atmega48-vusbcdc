
TEMPLATE = app
TARGET = vusbavrcdc

INCLUDEPATH += . usbdrv
INCLUDEPATH += /usr/avr/include

HEADERS += \
           usbconfig.h \
           usbdrv/usbconfig-prototype.h \
           usbdrv/usbdrv.h \
		   usbdrv/usbportability.h \
		   usbconfig.h

SOURCES += \
    main.cpp \
	usbdrv/usbdrv.c

DISTFILES += \
	usbdrv/usbdrvasm.S \
	usbdrv/asmcommon.inc \
	usbdrv/usbdrvasm20.inc
