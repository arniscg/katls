idf_component_register(SRCS main.c gui/gui.c gui/app/app.c gui/app/screen.c buttons.c
                       PRIV_REQUIRES spi_flash esp_driver_spi esp_driver_gpio esp_timer esp_ringbuf
                       INCLUDE_DIRS "")
