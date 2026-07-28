idf_component_register(
    SRCS
        "main.cpp"
        "io_expander.cpp"
        "lcd_panel.cpp"
        "backlight.cpp"
        "state.cpp"
        "render.cpp"
        "protocol.cpp"
        "buttons.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_lcd
        esp_timer
        freertos
        nvs_flash
)
