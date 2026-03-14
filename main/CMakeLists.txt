idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES bsp_esp32_s3_usb_otg_ev esp_tinyusb nvs_flash usb_hid ui wifi
)

target_compile_options(${COMPONENT_LIB} PRIVATE 
                                        -Wno-unused-variable
                                        -Wno-unused-function)
