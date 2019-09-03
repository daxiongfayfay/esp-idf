set(COMPONENT_SRCS "lcd_ili9341.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES driver) #ili9341 lcd request the spi driver

register_component()

