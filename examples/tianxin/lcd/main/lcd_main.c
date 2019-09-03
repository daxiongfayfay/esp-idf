/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include <sys/stat.h>

#include "esp_vfs_fat.h"
#include "lcd_ili9341.h"
#include "sd_file.h"

char *utf8  = "我的老家就住住决定离开家里附近开了的拉开空间龙卡及地方可立即按数量肯定就分了就爱了就地方可垃圾";


uint8_t  utf8_to_unicode(char *unicode, char *utf8) {
	char *pchar = utf8;
	int nbytes = 0;

	if (0 == (*utf8 & 0x80)) {
		nbytes = 1;
		unicode[0] = *utf8;
	} else {
		int i;
		if((*utf8 & 0xf0) == 0xe0) {
			nbytes = 3;
			unicode[0] = ((utf8[0] & 0x0f)<<4) + ((utf8[1] & 0x3f)>>2);
			unicode[1] = ((utf8[1] & 0x03)<<6) + (utf8[2] & 0x3f);
		}
		 else {
			nbytes = 0;
			unicode[0] = '?';
			return 0;
		}
	}

	
	return nbytes;;
}

void show_text(uint16_t startX, uint16_t startY, char *str, uint16_t length, FILE *file) {

	uint16_t cont = 0;
	char unicode[2];
	char *str_temp = NULL;
	uint16_t code = 0;
	uint8_t text_temp [32];
	uint16_t index=0, xpos=0, ypos=0;

	while(cont != length){
		str_temp = str + cont;
		unicode[0] = 0;
		unicode[1] = 0;
		cont += utf8_to_unicode(unicode, str_temp);
		//printf("unicode is: 0x%02x 0x%02x\r\n", unicode[0],unicode[1]);	
		
		code = (unicode[0]<<8) +unicode[1];
		fseek(file, code*32, 0);
		fread(text_temp, 32, 1, file);
		xpos = startX + (index%16)*16;
		ypos = startY+ (index/16)*16;
		lcd_draw_text_h16(xpos,ypos,text_temp,BLACK);
		index++;
	}
}



void app_main()
{
	uint8_t ziku[32];
	char unicode[2];
	uint8_t line[480];

	uint8_t length;
	length = strlen(utf8);


    //Initialize the LCD

    lcd_init();
    lcd_clear_screen(WHITE);

	lcd_draw_circle(100,100,30,RED);

	sd_file_mount();
	file_read_write_test();
	struct stat st;
    if (stat("/sdcard/unicode", &st) == 0) {
        // Delete it if it exists
        printf("file unicode exist!!.\n");
        printf("unicode Size: %lu\n", st.st_size);
    }

	FILE* f = fopen("/sdcard/unicode", "rb");
    if (f == NULL) {
		printf("Failed to open uincode16b file for reading.\n");
        return;
    }
	
	FILE* bmp = fopen("/sdcard/android1.bin", "r");
    if (bmp == NULL) {
       
        return;
    }
	
	for(int i=0; i<320; i++){
		fseek(bmp, 480*i, 0);
		fread(line, 480, 1, bmp);
		lcd_send_lines(i, line);
	}

	//show_text(0, 10, utf8, length, f);
	
	fclose(bmp);
	fclose(f);
	sd_file_unmount();
    
    while(1){

		//printf("lcd clear PURPLE.\n");
    	//lcd_clear_screen(PURPLE);
		//vTaskDelay(2000 / portTICK_RATE_MS);

		//printf("lcd show picture.\n");
		//lcd_show_picture();
		//vTaskDelay(2000 / portTICK_RATE_MS);

		//printf("lcd clear YELLOW.\n");
		//lcd_clear_screen(YELLOW);
		//vTaskDelay(2000 / portTICK_RATE_MS);

	}
    
}
