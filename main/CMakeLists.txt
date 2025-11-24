idf_component_register(SRCS main.c gui/gui.c gui/app/app.c gui/app/screen.c buttons.c journal.c wifi.c
                       PRIV_REQUIRES spi_flash esp_driver_spi esp_driver_gpio esp_timer esp_ringbuf vfs fatfs esp_wifi wpa_supplicant nvs_flash
                       INCLUDE_DIRS "")

set(image ../fatfs_image)
fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)