#ifndef __LCD_H
#define __LCD_H
#include "stm32f1xx.h"

#define LCD_COLS 16
#define LCD_ROWS 2

// addr: dia chi I2C da dich trai 1 bit (vd 0x27 << 1, PCF8574A: 0x3F << 1)
// Tra ve 1 neu LCD phan hoi, 0 neu khong (khi do moi ham khac la no-op)
uint8_t lcd_init(I2C_HandleTypeDef *hi2c, uint8_t addr);
void lcd_clear(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_print(const char *s);
// Ghi de du LCD_COLS ky tu tren 1 dong (dem khoang trang), khong can clear
void lcd_print_line(uint8_t row, const char *s);
void lcd_backlight(uint8_t on);

#endif
