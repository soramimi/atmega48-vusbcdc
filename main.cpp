
/* Name: main.c
 * Project: AVR USB driver for CDC interface on Low-Speed USB
 * Author: Osamu Tamura
 * Creation Date: 2006-05-12
 * Tabsize: 4
 * Copyright: (c) 2006 by Recursion Co., Ltd.
 * License: Proprietary, free under certain conditions. See Documentation.
 *
 * 2006-07-08   removed zero-sized receive block
 * 2006-07-08   adapted to higher baud rate by T.Kitazawa
 *
 */

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <string.h>
#include <util/delay.h>

#define SEND_TO_HOST_BUF_SIZE 32
#define RECV_FROM_HOST_BUF_SIZE 32
#define HW_CDC_BULK_OUT_SIZE 8
#define HW_CDC_BULK_IN_SIZE 8

extern "C" {
#include "usbdrv.h"
}

// bool send_to_host(uchar c);
// bool is_recv_from_host_available();
// int recv_from_host();

// ArduinoISP
// Copyright (c) 2008-2011 Randall Bohn
// If you require a license, see
// https://opensource.org/licenses/bsd-license.php
//
// This sketch turns the Arduino into a AVRISP using the following Arduino pins:
//
// Pin 10 is used to reset the target microcontroller.
//
// By default, the hardware SPI pins MISO, MOSI and SCK are used to communicate
// with the target. On all Arduinos, these pins can be found
// on the ICSP/SPI header:
//
//               MISO °. . 5V (!) Avoid this pin on Due, Zero...
//               SCK   . . MOSI
//                     . . GND
//
// On some Arduinos (Uno,...), pins MOSI, MISO and SCK are the same pins as
// digital pin 11, 12 and 13, respectively. That is why many tutorials instruct
// you to hook up the target to these pins. If you find this wiring more
// practical, have a define USE_OLD_STYLE_WIRING. This will work even when not
// using an Uno. (On an Uno this is not needed).
//
// Alternatively you can use any other digital pin by configuring
// software ('BitBanged') SPI and having appropriate defines for PIN_MOSI,
// PIN_MISO and PIN_SCK.
//
// IMPORTANT: When using an Arduino that is not 5V tolerant (Due, Zero, ...) as
// the programmer, make sure to not expose any of the programmer's pins to 5V.
// A simple way to accomplish this is to power the complete system (programmer
// and target) at 3V3.
//
// Put an LED (with resistor) on the following pins:
// 9: Heartbeat   - shows the programmer is running
// 8: Error       - Lights up if something goes wrong (use red if that makes sense)
// 7: Programming - In communication with the slave
//

//#include "Arduino.h"
#undef SERIAL

#include <avr/io.h>
#include <util/delay.h>

uchar intr3_status; /* used to control interrupt endpoint transmissions */
uchar send_empty_frame;

uchar recv_from_host_buffer_n, recv_from_host_buffer_i;
uchar recv_from_host_buffer[RECV_FROM_HOST_BUF_SIZE];

uchar send_to_host_n, send_to_host_i;
uchar send_to_host_buffer[SEND_TO_HOST_BUF_SIZE + HW_CDC_BULK_IN_SIZE];

void poll()
{
	wdt_reset();
	usbPoll();

#if USB_CFG_HAVE_INTRIN_ENDPOINT3
	/* We need to report rx and tx carrier after open attempt */
	if (intr3_status != 0 && usbInterruptIsReady3()) {
		static uchar serialStateNotification[10] = { 0xa1, 0x20, 0, 0, 0, 0, 2, 0, 3, 0 };
		if (intr3_status == 2) {
			usbSetInterrupt3(serialStateNotification, 8);
		} else {
			usbSetInterrupt3(serialStateNotification + 8, 2);
		}
		intr3_status--;
	}
#endif

	// USB flow control
	if (usbAllRequestsAreDisabled() && recv_from_host_buffer_n >= RECV_FROM_HOST_BUF_SIZE - HW_CDC_BULK_OUT_SIZE) {
		usbEnableAllRequests();
	}

	// USB <= device
	if (usbInterruptIsReady() && (send_to_host_n != 0 || send_empty_frame)) {
		uchar len = send_to_host_n < HW_CDC_BULK_IN_SIZE ? send_to_host_n : HW_CDC_BULK_IN_SIZE;

		if (send_to_host_i + send_to_host_n > SEND_TO_HOST_BUF_SIZE) {
			memcpy(send_to_host_buffer + SEND_TO_HOST_BUF_SIZE, send_to_host_buffer, HW_CDC_BULK_IN_SIZE);
		}
		usbSetInterrupt(send_to_host_buffer + send_to_host_i, len);
		send_to_host_i = (send_to_host_i + len) & (SEND_TO_HOST_BUF_SIZE - 1);
		send_to_host_n -= len;
		//		if (len) {
		//			UART_CTRL_PORT |= (1 << UART_CTRL_RTS);
		//		}

		/* send an empty block after last data block to indicate transfer end */
		send_empty_frame = (len == HW_CDC_BULK_IN_SIZE && send_to_host_n == 0) ? 1 : 0;
	}
}

bool send_to_host(uchar c)
{
	if (send_to_host_n < SEND_TO_HOST_BUF_SIZE) {
		uchar i = (send_to_host_i + send_to_host_n) & (SEND_TO_HOST_BUF_SIZE - 1);
		send_to_host_buffer[i] = c;
		send_to_host_n++;
		return true;
	}
	return false;
}

static inline bool is_recv_from_host_available()
{
	return recv_from_host_buffer_n > 0;
}

uint8_t recv_from_host()
{
	if (is_recv_from_host_available()) {
		uint8_t c = recv_from_host_buffer[recv_from_host_buffer_i];
		recv_from_host_buffer_i = (recv_from_host_buffer_i + 1) & (RECV_FROM_HOST_BUF_SIZE - 1);
		recv_from_host_buffer_n--;
		return c;
	}
	return 0;
}

static inline bool read_available()
{
	return is_recv_from_host_available();
}

static inline char read_byte()
{
	return recv_from_host();
}

static inline void write_byte(char c)
{
	send_to_host(c);
}

void write_string(char const *p)
{
	while (*p) {
		write_byte(*p);
		poll();
		p++;
	}
}

void setup()
{
}

void loop()
{
	if (is_recv_from_host_available()) {
		uint8_t c = recv_from_host();
		send_to_host(c);
	}
}

//

enum {
	SEND_ENCAPSULATED_COMMAND = 0,
	GET_ENCAPSULATED_RESPONSE,
	SET_COMM_FEATURE,
	GET_COMM_FEATURE,
	CLEAR_COMM_FEATURE,
	SET_LINE_CODING = 0x20,
	GET_LINE_CODING,
	SET_CONTROL_LINE_STATE,
	SEND_BREAK
};

PROGMEM const char usbDescriptorConfiguration[] = {
	// USB configuration descriptor
	9, // sizeof(usbDescrConfig): length of descriptor in bytes
	USBDESCR_CONFIG, // descriptor type
	9 + 9 + 5 + 4 + 5 + 7 + 9 + 7 + 7,
	0, // total length of data returned (including inlined descriptors)
	2, // number of interfaces in this configuration
	1, // index of this configuration
	0, // configuration name string index
#if USB_CFG_IS_SELF_POWERED
	(1 << 7) | USBATTR_SELFPOWER, // attributes
#else
	(1 << 7), // attributes
#endif
	USB_CFG_MAX_BUS_POWER / 2, // max USB current in 2mA units

	// interface descriptor follows inline:
	9, // sizeof(usbDescrInterface): length of descriptor in bytes
	USBDESCR_INTERFACE, // descriptor type
	0, // index of this interface
	0, // alternate setting for this interface
	USB_CFG_HAVE_INTRIN_ENDPOINT, // endpoints excl 0: number of endpoint descriptors to follow
	USB_CFG_INTERFACE_CLASS,
	USB_CFG_INTERFACE_SUBCLASS,
	USB_CFG_INTERFACE_PROTOCOL,
	0, // string index for interface

	// CDC Class-Specific descriptor
	5, // sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes
	0x24, // descriptor type
	0, // header functional descriptor
	0x10, 0x01,

	// CDC ACM
	4, // sizeof(usbDescrCDC_AcmFn): length of descriptor in bytes
	0x24, // descriptor type
	2, // abstract control management functional descriptor
	0x02, // line coding and serial state

	// CDC Union
	5, // sizeof(usbDescrCDC_UnionFn): length of descriptor in bytes
	0x24, // descriptor type
	6, // union functional descriptor
	0, // CDC_COMM_INTF_ID
	1, // CDC_DATA_INTF_ID

	// Endpoint Descriptor
	7, // sizeof(usbDescrEndpoint)
	USBDESCR_ENDPOINT, // descriptor type = endpoint
	0x80 | USB_CFG_EP3_NUMBER, // IN endpoint number
	0x03, // attrib: Interrupt endpoint
	8, 0, // maximum packet size
	USB_CFG_INTR_POLL_INTERVAL, // in ms

	// Interface Descriptor
	9, // sizeof(usbDescrInterface): length of descriptor in bytes
	USBDESCR_INTERFACE, // descriptor type
	1, // index of this interface
	0, // alternate setting for this interface
	2, // endpoints excl 0: number of endpoint descriptors to follow
	0x0A, // Data Interface Class Codes
	0,
	0, // Data Interface Class Protocol Codes
	0, // string index for interface

	// Endpoint Descriptor
	7, // sizeof(usbDescrEndpoint)
	USBDESCR_ENDPOINT, // descriptor type = endpoint
	0x01, // OUT endpoint number 1
	0x02, // attrib: Bulk endpoint
	8, 0, // maximum packet size
	0, // in ms

	// Endpoint Descriptor
	7, // sizeof(usbDescrEndpoint)
	USBDESCR_ENDPOINT, // descriptor type = endpoint
	0x82, // IN endpoint number 1
	0x02, // attrib: Bulk endpoint
	8, 0, // maximum packet size
	0, // in ms
};

uchar usbFunctionDescriptor(usbRequest_t *rq)
{

	if (rq->wValue.bytes[1] == USBDESCR_DEVICE) {
		usbMsgPtr = (uchar *)usbDescriptorDevice;
		return usbDescriptorDevice[0];
	} else { /* must be config descriptor */
		usbMsgPtr = (uchar *)usbDescriptorConfiguration;
		return sizeof(usbDescriptorConfiguration);
	}
}

// union usb_dword_t {
//	uint32_t dword;
//	uint8_t bytes[4];
// };

// usb_dword_t baud;

static const uint32_t baud = 19200;
static const uchar stopbit = 0;
static const uchar parity = 0;
static const uchar databit = 8;

void reset_io_buffer(void)
{
	send_to_host_i = 0;
	send_to_host_n = 0;
	recv_from_host_buffer_i = 0;
	recv_from_host_buffer_n = 0;
}

// USB interface
uchar usbFunctionSetup(uchar data[8])
{
#if 1
	usbRequest_t *rq = (usbRequest *)data;

	if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) { /* class request type */

		if (rq->bRequest == GET_LINE_CODING || rq->bRequest == SET_LINE_CODING) {
			return 0xff;
			/*    GET_LINE_CODING -> usbFunctionRead()    */
			/*    SET_LINE_CODING -> usbFunctionWrite()    */
		}
		if (rq->bRequest == SET_CONTROL_LINE_STATE) {
//			UART_CTRL_PORT = (UART_CTRL_PORT & ~(1 << UART_CTRL_DTR)) | ((rq->wValue.word & 1) << UART_CTRL_DTR);

#if USB_CFG_HAVE_INTRIN_ENDPOINT3
			/* Report serial state (carrier detect). On several Unix platforms,
			 * tty devices can only be opened when carrier detect is set.
			 */
			if (intr3_status == 0) {
				intr3_status = 2;
			}
#endif
		}
#if 1
		/*  Prepare bulk-in endpoint to respond to early termination   */
		if ((rq->bmRequestType & USBRQ_DIR_MASK) == USBRQ_DIR_HOST_TO_DEVICE) {
			send_empty_frame = 1;
		}
#endif
	}
#endif
	return 0;
}

#if 1
// usbFunctionRead
uchar usbFunctionRead(uchar *data, uchar len)
{
#if 1
#if 0
	usb_dword_t baud;
	baud.dword = 19200;
	data[0] = baud.bytes[0];
	data[1] = baud.bytes[1];
	data[2] = baud.bytes[2];
	data[3] = baud.bytes[3];
#else
	data[0] = uint8_t(baud);
	data[1] = uint8_t(baud >> 8);
	data[2] = uint8_t(baud >> 16);
	data[3] = uint8_t(baud >> 24);
#endif
	data[4] = stopbit;
	data[5] = parity;
	data[6] = databit;
	return 7;
#endif
	return 0;
}
#endif

// usbFunctionWrite
uchar usbFunctionWrite(uchar *data, uchar len)
{
#if 0
	baud.bytes[0] = data[0];
	baud.bytes[1] = data[1];
	baud.bytes[2] = data[2];
	baud.bytes[3] = data[3];

	stopbit = data[4];
	parity = data[5];
	databit = data[6];

	if (parity > 2) {
		parity = 0;
	}
	if (stopbit == 1) {
		stopbit = 0;
	}
#endif

	reset_io_buffer();

	return 1;
}

void usbFunctionWriteOut(uchar *data, uchar len)
{
	while (len > 0) {
		if (recv_from_host_buffer_n < RECV_FROM_HOST_BUF_SIZE) {
			uchar i = (recv_from_host_buffer_i + recv_from_host_buffer_n) & (RECV_FROM_HOST_BUF_SIZE - 1);
			recv_from_host_buffer[i] = *data++;
			recv_from_host_buffer_n++;
			len--;
		} else {
			usbDisableAllRequests();
			break;
		}
	}
}

void hardwareInit(void)
{
	/* activate pull-ups except on USB lines */
	USB_CFG_IOPORT = (uchar) ~((1 << USB_CFG_DMINUS_BIT) | (1 << USB_CFG_DPLUS_BIT));
	/* all pins input except USB (-> USB reset) */
#ifdef USB_CFG_PULLUP_IOPORT /* use usbDeviceConnect()/usbDeviceDisconnect() if available */
	USBDDR = 0; /* we do RESET by deactivating pullup */
	usbDeviceDisconnect();
#else
	USBDDR = (1 << USB_CFG_DMINUS_BIT) | (1 << USB_CFG_DPLUS_BIT);
#endif

	/* 250 ms disconnect */
	wdt_reset();
	_delay_ms(250);

#ifdef USB_CFG_PULLUP_IOPORT
	usbDeviceConnect();
#else
	USBDDR = 0; /*  remove USB reset condition */
#endif

	/*    USART configuration    */
	//	baud.dword = 19200;
	//	stopbit = 0;
	//	parity = 0;
	//	databit = 8;
	reset_io_buffer();
}

int main()
{
#if 0
//	_delay_ms(10);
	ISPLCD lcd;
	lcd.start();
	lcd.led(true);
//	lcd.putchar('A');
#endif

	wdt_enable(WDTO_1S);
	hardwareInit();
	usbInit();

	intr3_status = 0;
	send_empty_frame = 0;

	sei();

	wdt_reset();
	setup();

	while (1) {
		poll();
		loop();
	}

	return 0;
}
