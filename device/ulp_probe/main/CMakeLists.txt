idf_component_register(SRCS probe_main.c INCLUDE_DIRS .
    REQUIRES ulp esp_hw_support esp_driver_gpio esp_driver_uart vfs esp_timer)
set(ulp_app_name ulp_${COMPONENT_NAME})
ulp_embed_binary(${ulp_app_name} "ulp/main.c" "probe_main.c")
