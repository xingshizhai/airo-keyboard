idf_component_register(
    SRCS "bsp_esp32_s3_usb_otg_ev.c"
    INCLUDE_DIRS "include"
    REQUIRES driver button esp_lcd esp_driver_spi
)
