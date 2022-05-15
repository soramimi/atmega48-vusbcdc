#ifndef ISPLCD_H
#define ISPLCD_H

#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>

class ISPLCD {
private:
	const uint8_t LCD_ADDRESS = 0x27;
	const uint8_t LCD_CMD = 0;
	const uint8_t LCD_CHR = 1;
	const uint8_t LCD_BACKLIGHT = 0x08;
	const uint8_t ENABLE = 0x04;

	enum class Port {
		B,
		C,
		D,
	};

	template <Port PORT, int PIN> class GPIO {
	public:
	private:
		const uint8_t bit = 1 << PIN;
		bool pullup_ = false;
		void setdir(bool output)
		{
			switch (PORT) {
			case Port::B: if (output) { DDRB |= bit; } else { DDRB &= ~bit; } return;
			case Port::C: if (output) { DDRC |= bit; } else { DDRC &= ~bit; } return;
			case Port::D: if (output) { DDRD |= bit; } else { DDRD &= ~bit; } return;
			}
		}
	public:
		GPIO()
		{
			setdir(false);
			setpullup(false);
		}
		void setpullup(bool pullup)
		{
			pullup_ = pullup;
			switch (PORT) {
			case Port::B: write(pullup_); return;
			case Port::C: write(pullup_); return;
			case Port::D: write(pullup_); return;
			}
		}
		bool read()
		{
			setdir(false);
			switch (PORT) {
			case Port::B: return (PINB & bit) ? 1 : 0;
			case Port::C: return (PINC & bit) ? 1 : 0;
			case Port::D: return (PIND & bit) ? 1 : 0;
			}
		}
		void write(bool b)
		{
			if (pullup_ && b) {
				setdir(false);
			} else {
				setdir(true);
			}
			switch (PORT) {
			case Port::B: if (b) { PORTB |= bit; } else { PORTB &= ~bit; } return;
			case Port::C: if (b) { PORTC |= bit; } else { PORTC &= ~bit; } return;
			case Port::D: if (b) { PORTD |= bit; } else { PORTD &= ~bit; } return;
			}
		}
	};

	// software I2C
	class I2C {
		friend class ISPLCD;
	private:
		GPIO<Port::B, 1> i2c_clk;
		GPIO<Port::B, 2> i2c_dat;
		GPIO<Port::B, 3> led_pin;

		void delay()
		{
			asm("nop");
		}

		// 初期化
		void init_i2c()
		{
			i2c_clk.setpullup(true);
			i2c_dat.setpullup(true);
		}

		void i2c_cl_0()
		{
			i2c_clk.write(false);
		}

		void i2c_cl_1()
		{
			i2c_clk.write(true);
		}

		void i2c_da_0()
		{
			i2c_dat.write(false);
		}

		void i2c_da_1()
		{
			i2c_dat.write(true);
		}

		uint8_t i2c_get_da()
		{
			return i2c_dat.read();
		}

		// スタートコンディション
		void i2c_start()
		{
			i2c_da_0(); // SDA=0
			delay();
			i2c_cl_0(); // SCL=0
			delay();
		}

		// ストップコンディション
		void i2c_stop()
		{
			i2c_cl_1(); // SCL=1
			delay();
			i2c_da_1(); // SDA=1
			delay();
		}

		// リピーテッドスタートコンディション
		void i2c_repeat()
		{
			i2c_cl_1(); // SCL=1
			delay();
			i2c_da_0(); // SDA=0
			delay();
			i2c_cl_0(); // SCL=0
			delay();
		}

		// 1バイト送信
		bool i2c_write(uint8_t c)
		{
			bool nack;

			delay();

			// 8ビット送信
			for (uint8_t i = 0; i < 8; i++) {
				if (c & 0x80) {
					i2c_da_1(); // SCL=1
				} else {
					i2c_da_0(); // SCL=0
				}
				c <<= 1;
				delay();
				i2c_cl_1(); // SCL=1
				delay();
				i2c_cl_0(); // SCL=0
				delay();
			}

			i2c_da_1(); // SDA=1
			delay();

			i2c_cl_1(); // SCL=1
			delay();
			// NACKビットを受信
			nack = i2c_get_da();
			i2c_cl_0(); // SCL=0

			return nack;
		}

		uint8_t address; // I2Cデバイスアドレス

	public:
		I2C(uint8_t address)
			: address(address)
		{
			init_i2c();
		}

		// デバイスのレジスタに書き込む
		void write(uint8_t reg, uint8_t data)
		{
			i2c_start();                   // スタート
			i2c_write(address << 1);       // デバイスアドレスを送信
			i2c_write(reg);                // レジスタ番号を送信
			i2c_write(data);               // データを送信
			i2c_stop();                    // ストップ
		}

		void write(uint8_t data)
		{
			i2c_start();                   // スタート
			i2c_write(address << 1);       // デバイスアドレスを送信
			i2c_write(data);               // データを送信
			i2c_stop();                    // ストップ
		}

	};

	I2C wire_ = {LCD_ADDRESS};
	void lcd_byte(uint8_t bits, uint8_t mode)
	{
		uint8_t hi = mode | (bits & 0xf0) | LCD_BACKLIGHT;
		uint8_t lo = mode | ((bits << 4) & 0xf0) | LCD_BACKLIGHT;
		_delay_us(10);
		const int W = 1;
		wire_.write(hi);
		_delay_us(W);
		wire_.write(hi | ENABLE);
		_delay_us(W);
		wire_.write(hi);
		_delay_us(W);
		wire_.write(lo);
		_delay_us(W);
		wire_.write(lo | ENABLE);
		_delay_us(W);
		wire_.write(lo);
		_delay_us(10);
	}
public:
	void led(bool f)
	{
		wire_.led_pin.write(f);
	}
	void home()
	{
		lcd_byte(0x80, LCD_CMD);
	}
	void putchar(char c)
	{
		lcd_byte(c, LCD_CHR);
	}
	void print(char const *ptr)
	{
		while (*ptr) {
			putchar(*ptr);
			ptr++;
		}
	}
	void clear()
	{
		lcd_byte(0x01, LCD_CMD);
		_delay_ms(2);
	}
	void init()
	{
		lcd_byte(0x33, LCD_CMD);
		_delay_us(40);
		lcd_byte(0x32, LCD_CMD);
		_delay_us(40);
		lcd_byte(0x06, LCD_CMD);
		_delay_us(40);
		lcd_byte(0x0c, LCD_CMD);
		_delay_us(40);
		lcd_byte(0x28, LCD_CMD);
		_delay_us(40);
		clear();
	}
	void start()
	{
		ISPLCD::init();
		ISPLCD::clear();
		ISPLCD::home();
	}
};

#endif // ISPLCD_H
