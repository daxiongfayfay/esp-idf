#include "lcd_ili9341.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PARALLEL_LINES 16

//define the lcd control pin.
#define PIN_NUM_MISO 25
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  19
#define PIN_NUM_CS   22

#define PIN_NUM_DC   21
#define PIN_NUM_RST  18
#define PIN_NUM_BCKL 5

#define LCD_TYPE_ILI 1

#define RGB565(r, g, b) ((r>>3)<<11)&((g>>2)<<5)&(b>>3)

#define BLUE RGB565(0, 0, 255)

/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;


//Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.

DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[]={
    /* Power contorl B, power control = 0, DC_ENA = 1 */
    {0xCF, {0x00, 0x83, 0X30}, 3},
    /* Power on sequence control,
     * cp1 keeps 1 frame, 1st frame enable
     * vcl = 0, ddvdh=3, vgh=1, vgl=2
     * DDVDH_ENH=1
     */
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    /* Driver timing control A,
     * non-overlap=default +1
     * EQ=default - 1, CR=default
     * pre-charge=default - 1
     */
    {0xE8, {0x85, 0x01, 0x79}, 3},
    /* Power control A, Vcore=1.6V, DDVDH=5.6V */
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    /* Pump ratio control, DDVDH=2xVCl */
    {0xF7, {0x20}, 1},
    /* Driver timing control, all=0 unit */
    {0xEA, {0x00, 0x00}, 2},
    /* Power control 1, GVDD=4.75V */
    {0xC0, {0x26}, 1},
    /* Power control 2, DDVDH=VCl*2, VGH=VCl*7, VGL=-VCl*3 */
    {0xC1, {0x11}, 1},
    /* VCOM control 1, VCOMH=4.025V, VCOML=-0.950V */
    {0xC5, {0x35, 0x3E}, 2},
    /* VCOM control 2, VCOMH=VMH-2, VCOML=VML-2 */
    {0xC7, {0xBE}, 1},
    /* Memory access contorl, MX=MY=0, MV=1, ML=0, BGR=1, MH=0 */
    {0x36, {0x68}, 1},
    /* Pixel format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Frame rate control, f=fosc, 70Hz fps */
    {0xB1, {0x00, 0x1B}, 2},
    /* Enable 3G, disabled */
    {0xF2, {0x08}, 1},
    /* Gamma set, curve 1 */
    {0x26, {0x01}, 1},
    /* Positive gamma correction */
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    /* Negative gamma correction */
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    /* Column address set, SC=0, EC=0xEF */
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    /* Page address set, SP=0, EP=0x013F */
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
    /* Memory write */
    {0x2C, {0}, 0},
    /* Entry mode set, Low vol detect disabled, normal display */
    {0xB7, {0x07}, 1},
    /* Display function control */
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},   
    /* Sleep out */
    {0x11, {0}, 0x80},
    /* Display on */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};


spi_device_handle_t lcd_spi;



/***************************************************************************
 *函数声明
***************************************************************************/
void lcd_spi_init();
void lcd_cmd(const uint8_t cmd);
void lcd_data(const uint8_t *data, int len);
void lcd_spi_pre_transfer_callback(spi_transaction_t *t);
uint32_t lcd_get_id();
void lcd_init();
void lcd_send_lines(int ypos, uint16_t *linedata);
void lcd_send_line_finish();


/***************************************************************************
 *函数名称：lcd_spi_init
 *函数功能：控制lcd 的spi初始化
 *函数参数：void
 *函数返回：void
***************************************************************************/
void lcd_spi_init(){

	esp_err_t ret;
	spi_device_handle_t spi;

    spi_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=PARALLEL_LINES*320*2+8
    };

    spi_device_interface_config_t devcfg={
        .clock_speed_hz=26*1000*1000,           //Clock out at 26 MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=PIN_NUM_CS,               //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };

    //Initialize the SPI bus
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &lcd_spi);
    ESP_ERROR_CHECK(ret);
}


/***************************************************************************
 *函数名称：lcd_cmd
 *函数功能：向LCD中写入命令
 *函数参数：(1) spi: spi操作结构体， (2) cmd: 写入的命令字节
 *函数返回：void
***************************************************************************/

void lcd_cmd(const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_polling_transmit(lcd_spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

/***************************************************************************
 *函数名称：lcd_data
 *函数功能：向LCD中写入数据
 *函数参数：(1) spi: spi操作结构体， (2) cmd: 写入的命令数据buffer，
            (3) len: 写入数据长度
 *函数返回：void
***************************************************************************/
void lcd_data(const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_polling_transmit(lcd_spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

/***************************************************************************
 *函数名称：lcd_spi_pre_transfer_callback
 *函数功能：spi开始传输回掉函数
 *函数参数：(1) *t: spi 传输传输事物结构体
 *函数返回：void
***************************************************************************/
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}


/***************************************************************************
 *函数名称：lcd_get_id
 *函数功能：获取LCD的ID
 *函数参数：void 
 *函数返回：esp_err_t
***************************************************************************/
uint32_t lcd_get_id()
{
    //get_id cmd
    lcd_cmd(0x04);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=8*3;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void*)1;

    esp_err_t ret = spi_device_polling_transmit(lcd_spi, &t);
    assert( ret == ESP_OK );

    return *(uint32_t*)t.rx_data;
}

/***************************************************************************
 *函数名称：lcd_init
 *函数功能：Lcd初始化
 *函数参数：void 
 *函数返回：void
***************************************************************************/

void lcd_init()
{
    int cmd=0;
    const lcd_init_cmd_t* lcd_init_cmds;

	lcd_spi_init();

    //Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);

    //Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);

    //detect LCD type
    uint32_t lcd_id = lcd_get_id();
    printf("LCD ID: %08X\n", lcd_id);

	//init the lcd
    printf("LCD ILI9341 initialization.\n");
    lcd_init_cmds = ili_init_cmds;


    //Send all the commands
    while (lcd_init_cmds[cmd].databytes!=0xff) {
        lcd_cmd(lcd_init_cmds[cmd].cmd);
        lcd_data(lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);
        if (lcd_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }

    ///Enable backlight
    gpio_set_level(PIN_NUM_BCKL, 0);
}

/***************************************************************************
 *函数名称：lcd_send_lines
 *函数功能：向Lcd中写入行数据
 *函数参数：(1) ypos: 写入数据的行位置， (2) *linedata: 要写入的行数据buffer
 *函数返回：void
***************************************************************************/
void lcd_send_lines(int ypos, uint16_t *linedata)
{
    esp_err_t ret;
    int x;
    //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
    //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
    static spi_transaction_t trans[6];

    //In theory, it's better to initialize trans and data only once and hang on to the initialized
    //variables. We allocate them on the stack, so we need to re-init them each call.
    for (x=0; x<6; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));
        if ((x&1)==0) {
            //Even transfers are commands
            trans[x].length=8;
            trans[x].user=(void*)0;
        } else {
            //Odd transfers are data
            trans[x].length=8*4;
            trans[x].user=(void*)1;
        }
        trans[x].flags=SPI_TRANS_USE_TXDATA;
    }
    trans[0].tx_data[0]=0x2A;           //Column Address Set
    trans[1].tx_data[0]=0;              //Start Col High
    trans[1].tx_data[1]=0;              //Start Col Low
    trans[1].tx_data[2]=(240)>>8;       //End Col High
    trans[1].tx_data[3]=(240)&0xff;     //End Col Low
    trans[2].tx_data[0]=0x2B;           //Page address set
    trans[3].tx_data[0]=ypos>>8;        //Start page high
    trans[3].tx_data[1]=ypos&0xff;      //start page low
    trans[3].tx_data[2]=(ypos)>>8;    //end page high
    trans[3].tx_data[3]=(ypos)&0xff;  //end page low
    trans[4].tx_data[0]=0x2C;           //memory write
    trans[5].tx_buffer=linedata;        //finally send the line data
    trans[5].length=240*2*8;          //Data length, in bits
    trans[5].flags=0; //undo SPI_TRANS_USE_TXDATA flag

    //Queue all transactions.
    for (x=0; x<6; x++) {
        ret=spi_device_queue_trans(lcd_spi, &trans[x], portMAX_DELAY);
        assert(ret==ESP_OK);
    }
}

/***************************************************************************
 *函数名称：send_line_finish
 *函数功能：发送行数据完成时回调
 *函数参数：void
 *函数返回：void
***************************************************************************/
void lcd_send_line_finish()
{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    //Wait for all 6 transactions to be done and get back the results.
    for (int x=0; x<6; x++) {
        ret=spi_device_get_trans_result(lcd_spi, &rtrans, portMAX_DELAY);
        assert(ret==ESP_OK);
        //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
    }
}

/***************************************************************************
 *函数名称：lcd_clear_screen
 *函数功能：lcd清屏
 *函数参数：(1) color: 清屏颜色
 *函数返回：void
***************************************************************************/
void lcd_clear_screen(){
	uint16_t x;
	uint16_t data[240];

    for (x=0; x<240; x++){
		data[x] = BLUE;
	}

	for (x=0; x<320; x++){
		lcd_send_lines(x, data);
	}

}

