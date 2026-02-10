idf_component_register(SRCS main.c gui/gui.c buttons.c journal.c wifi.c state.c gui/screen/screens.c gui/screen/wifi_screen.c gui/screen/input_screen.c gui/screen/journal_screen.c common.c
                       PRIV_REQUIRES spi_flash esp_driver_spi esp_driver_gpio esp_timer esp_ringbuf vfs fatfs esp_wifi wpa_supplicant nvs_flash esp_netif esp_http_client
                       INCLUDE_DIRS "")

set(image ../fatfs_image)
fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)