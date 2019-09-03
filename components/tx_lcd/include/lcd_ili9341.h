#include "esp_err.h"


void lcd_init(spi_device_handle_t spi);
static void send_lines(spi_device_handle_t spi, int ypos, uint16_t *linedata);

