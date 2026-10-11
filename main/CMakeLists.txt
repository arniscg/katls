set(COMMON_SRCS main.c gui/gui.c journal.c state.c common.c
                 gui/screen/screens.c gui/screen/wifi_screen.c gui/screen/input_screen.c gui/screen/journal_screen.c)

idf_build_get_property(target IDF_TARGET)

if(target STREQUAL "linux")
    # Desktop LVGL simulator build (idf.py --preview set-target linux):
    # no real GPIO/SPI/WiFi hardware, so the hardware-facing modules are
    # swapped for host stand-ins (see buttons_sim.c / wifi_sim.c / gui.c).
    idf_component_register(SRCS ${COMMON_SRCS} buttons_sim.c wifi_sim.c
                           PRIV_REQUIRES esp_timer esp_ringbuf
                           INCLUDE_DIRS "")
    target_link_libraries(${COMPONENT_LIB} PUBLIC SDL2)
else()
    idf_component_register(SRCS ${COMMON_SRCS} buttons.c wifi.c
                           PRIV_REQUIRES spi_flash esp_driver_spi esp_driver_gpio esp_timer esp_ringbuf vfs fatfs esp_wifi wpa_supplicant nvs_flash esp_netif esp_http_client
                           INCLUDE_DIRS "")

    set(image ../fatfs_image)
    fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)
endif()
