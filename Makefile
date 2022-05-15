
AVRISP = /dev/ttyUSB0

all:
	cd mega48/default; make


write:
	avrdude -c avrisp -P $(AVRISP) -b 19200 -p m48 -U efuse:w:0xff:m -U hfuse:w:0xce:m  -U lfuse:w:0xff:m -U flash:w:mega48/default/cdcmega48.hex

