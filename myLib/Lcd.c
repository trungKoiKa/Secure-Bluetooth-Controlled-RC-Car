#include "Lcd.h"

// Backpack PCF8574: P0=RS P1=RW P2=E P3=BL P4..P7=D4..D7
#define LCD_RS 0x01
#define LCD_EN 0x04
#define LCD_BL 0x08

#define LCD_I2C_TIMEOUT 10 //ms

static I2C_HandleTypeDef *lcd_i2c;
static uint8_t lcd_addr;
static uint8_t lcd_present = 0;
static uint8_t lcd_bl = LCD_BL;

// gui 4 bit cao cua nibble, kem xung E
static void lcd_send_nibble(uint8_t nibble, uint8_t rs)
{
	uint8_t data = (nibble & 0xF0) | rs | lcd_bl;
	uint8_t buf[2] = {data | LCD_EN, data};
	if(HAL_I2C_Master_Transmit(lcd_i2c, lcd_addr, buf, 2, LCD_I2C_TIMEOUT) != HAL_OK)
	{
		lcd_present = 0; // khong phan hoi: bo qua cac lenh sau, khong treo xe
	}
}

static void lcd_send_byte(uint8_t byte, uint8_t rs)
{
	if(!lcd_present)
	{
		return;
	}
	lcd_send_nibble(byte & 0xF0, rs);
	lcd_send_nibble((byte << 4) & 0xF0, rs);
}

static void lcd_send_cmd(uint8_t cmd)
{
	lcd_send_byte(cmd, 0);
}

uint8_t lcd_init(I2C_HandleTypeDef *hi2c, uint8_t addr)
{
	lcd_i2c = hi2c;
	lcd_addr = addr;
	lcd_bl = LCD_BL;
	lcd_present = (HAL_I2C_IsDeviceReady(hi2c, addr, 2, LCD_I2C_TIMEOUT) == HAL_OK);
	if(!lcd_present)
	{
		return 0;
	}

	HAL_Delay(50);
	// chuoi khoi tao 4-bit theo datasheet HD44780
	lcd_send_nibble(0x30, 0);
	HAL_Delay(5);
	lcd_send_nibble(0x30, 0);
	HAL_Delay(1);
	lcd_send_nibble(0x30, 0);
	HAL_Delay(1);
	lcd_send_nibble(0x20, 0); // chuyen sang 4-bit

	lcd_send_cmd(0x28); // 4-bit, 2 dong, font 5x8
	lcd_send_cmd(0x0C); // bat hien thi, tat con tro
	lcd_send_cmd(0x06); // tu dong tang con tro
	lcd_clear();
	return lcd_present;
}

void lcd_clear(void)
{
	lcd_send_cmd(0x01);
	if(lcd_present)
	{
		HAL_Delay(2);
	}
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
	static const uint8_t row_offset[LCD_ROWS] = {0x00, 0x40};
	if(row >= LCD_ROWS)
	{
		row = LCD_ROWS - 1;
	}
	lcd_send_cmd(0x80 | (row_offset[row] + col));
}

void lcd_print(const char *s)
{
	while(*s)
	{
		lcd_send_byte((uint8_t)*s++, LCD_RS);
	}
}

void lcd_print_line(uint8_t row, const char *s)
{
	lcd_set_cursor(row, 0);
	uint8_t i = 0;
	for(; i < LCD_COLS && s[i]; i++)
	{
		lcd_send_byte((uint8_t)s[i], LCD_RS);
	}
	for(; i < LCD_COLS; i++)
	{
		lcd_send_byte(' ', LCD_RS);
	}
}

void lcd_backlight(uint8_t on)
{
	lcd_bl = on ? LCD_BL : 0;
	if(lcd_present)
	{
		uint8_t data = lcd_bl;
		HAL_I2C_Master_Transmit(lcd_i2c, lcd_addr, &data, 1, LCD_I2C_TIMEOUT);
	}
}
