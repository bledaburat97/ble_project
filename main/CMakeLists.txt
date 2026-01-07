set(IDF_TARGET "esp32c6")
set(app_sources "lp_core_main.c")
#set(CMAKE_TOOLCHAIN_FILE "C:/Users/bleda.burat/.espressif/tools/cmake/toolchain-esp32c6.cmake" CACHE STRING "Toolchain file" FORCE)
#set(CMAKE_C_COMPILER "C:/Users/bleda.burat/.espressif/tools/riscv32-esp-elf/esp-13.2.0_20240530/riscv32-esp-elf/bin/riscv32-esp-elf-gcc.exe" CACHE STRING "C Compiler" FORCE)
#set(CMAKE_ASM_COMPILER "C:/Users/bleda.burat/.espressif/tools/riscv32-esp-elf/esp-13.2.0_20240530/riscv32-esp-elf/bin/riscv32-esp-elf-as.exe" CACHE STRING "ASM Compiler" FORCE)

idf_component_register(SRCS "i2c/temperature/temperature_sensor_controller.c" 
                            "i2c/temperature/temperature_sensor.c"
                            "i2c/temperature/temperature_alert_controller.c"
                            "i2c/humidity/humidity_sensor_controller.c"
                            "i2c/humidity/humidity_sensor.c"
                            "i2c/proximity/proximity_int_controller.c"
                            "i2c/proximity/proximity_sensor_controller.c"
                            "i2c/laser/laser_driver_controller.c"
                            "i2c/i2c_control.c"

                            "main.c"

                            "nvs/storage_manager.c"

                            "lp_core/lp_core_queue_manager.c"
                            "lp_core/lp_core_main.c"
                            #"lp_core/main.c"

                            "ble/src/ble_advertisement_manager.c"
                            "ble/src/ble_connection_state_manager.c"
                            "ble/src/ble_gap_handler.c"
                            "ble/src/ble_gatts_handler.c"
                            "ble/src/ble_initiator.c"
                            "ble/src/ble_message_sender.c"
                            "ble/src/ble_phy_manager.c"
                            "ble/src/ble_profile_manager.c"
                            "ble/src/ble_security_manager.c"
                            "ble/src/ble_state.c"

                            "helper/timer_utils.c"
                            "helper/binary_message_encoder.c"
                            "helper/binary_message_parser.c" 

                            "state/deep_sleep_manager.c"
                            "state/mode_selector.c"

                            "button/main_button_controller.c"

                            "storage/log_utils.c"
                            "storage/log_writer.c"
                            "storage/log_partition_manager.c"
                            "storage/therapy_counter.c"

                            "transaction/matching_message_encoder.c"
                            "transaction/message_queue_manager.c"
                            "transaction/device_info_message_creator.c"
                            "transaction/measurement_info_message_creator.c"
                            "transaction/notification_info_message_creator.c"
                            "transaction/records_info_message_creator.c"
                            "transaction/timer_state_info_message_creator.c"
                            "transaction/default_configuration_handler.c"
                            "transaction/passkey_handler.c"
                            "transaction/wifi_config_handler.c"

                            "manager/therapy_duration_manager.c"
                            "manager/current_therapy_state_manager.c"
                            "manager/device_event_router.c"
                            "manager/device_initiator.c"
                            "manager/device_manager.c"
                            "manager/helmet_off_debounce_timer_manager.c"
                            "manager/state_controller.c"
                            "manager/therapy_id_manager.c"
                            "manager/timer_controller.c"
                            "manager/session_timer_manager.c"

                            "common/device_states.c"

                            "buzzer/buzzer.c"

                        INCLUDE_DIRS "." 
                                "lp_core"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/ulp/lp_core/"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/ulp/lp_core/include"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/ulp/lp_core/lp_core/include"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/ulp/ulp_common/include"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/bt/include"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/nvs_flash/include"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/json/cjson"
                        REQUIRES ulp nvs_flash json bt esp_https_ota esp_http_client esp-tls mbedtls esp_netif esp_event)

set(ulp_app_name "lp_core_firmware")

set(ulp_lp_core_sources "lp_core/main.c")

set(ulp_exp_dep_srcs "lp_core_main.c")

ulp_embed_binary(${ulp_app_name} "${ulp_lp_core_sources}" "${ulp_exp_dep_srcs}")
target_link_libraries(${COMPONENT_LIB} PUBLIC idf::ulp)