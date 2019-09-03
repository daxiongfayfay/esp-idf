
#ifndef _LCD_ILI9341_H_
#define _LCD_ILI9341_H_

#include <stdio.h>
#include "driver/spi_master.h"


#define PARALLEL_LINES 16

//define the lcd control pin.
#define PIN_NUM_MISO 25
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  19
#define PIN_NUM_CS   22

#define PIN_NUM_DC   21
#define PIN_NUM_RST  18
#define PIN_NUM_BCKL 5

#define RGB565(r,g,b)  (((r>>3)<<11)|((g>>2)<<6)|(b>>3)) //RGB565颜色

#define RED				RGB565(255,0,0) //红色
#define GREEN			RGB565(0,255,0) //绿色
#define BLUE			RGB565(0,0,255) //蓝色
#define WHITE			RGB565(255,255,255) //白色
#define PURPLE			RGB565(120,9,130) //紫色
#define BLACK			RGB565(0,0,0) //黑色

#define YELLOW			RGB565(255,255,0)

void lcd_init();
void lcd_send_lines(int ypos, uint16_t *linedata);
void lcd_send_line_finish();
void lcd_clear_screen(uint16_t color);
void lcd_image_decode_init();
void lcd_show_picture();
void lcd_draw_point(uint16_t posX, uint16_t posY, uint16_t color);
void lcd_draw_circle(uint16_t cx,uint16_t cy,uint16_t r,uint16_t color);
void  lcd_draw_text_16(uint16_t charX,uint16_t charY,uint8_t *data,uint16_t color);
void  lcd_draw_text_h16(uint16_t charX,uint16_t charY,uint8_t *data,uint16_t color);
#endif
