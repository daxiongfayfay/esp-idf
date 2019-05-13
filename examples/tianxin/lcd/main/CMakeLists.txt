set(COMPONENT_SRCS "decode_image.c"
                   "pretty_effect.c"
                   "lcd_ili9341.c"
                   "lcd_main.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_EMBED_FILES image.jpg)

register_component()

