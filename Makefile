
PROJECT = a
TARGET = $(PROJECT).elf

AVRISP = /dev/ttyUSB0

MCU = atmega48
CLK = 16000000UL

INCLUDES = -I. -Iusbdrv
OBJECTS = usbdrv.o usbdrvasm.o uart.o main.o 

COMMON = -mmcu=$(MCU) -DF_CPU=$(CLK)
CFLAGS = $(COMMON) -Os -Wno-narrowing

CC = avr-gcc $(CFLAGS) $(INCLUDES)
CXX = avr-g++ $(CFLAGS) $(INCLUDES)
LD = avr-gcc $(COMMON)

all: $(TARGET) $(PROJECT).hex size

usbdrvasm.o: usbdrv/usbdrvasm.S
	$(CC) -c $^

usbdrv.o: usbdrv/usbdrv.c
	$(CC) -c $^

uart.o: uart.cpp
	$(CXX) -c $^

main.o: main.cpp
	$(CXX) -c $^

$(TARGET): $(OBJECTS)
	$(LD) $^ -o $@

%.hex: $(TARGET)
	avr-objcopy -O ihex $< $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=$(MCU) ${TARGET}

.PHONY: clean
clean:
	-rm -rf *.o *.elf *.hex

.PHONY: write
write: $(PROJECT).hex
	avrdude -c avrisp -P $(AVRISP) -b 19200 -p m48 -U hfuse:w:0xdf:m  -U lfuse:w:0xe6:m -U flash:w:$(PROJECT).hex
