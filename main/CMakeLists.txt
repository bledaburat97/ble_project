set(IDF_TARGET "esp32c6")
set(app_sources "lp_core_main.c")
#set(CMAKE_TOOLCHAIN_FILE "C:/Users/bleda.burat/.espressif/tools/cmake/toolchain-esp32c6.cmake" CACHE STRING "Toolchain file" FORCE)
#set(CMAKE_C_COMPILER "C:/Users/bleda.burat/.espressif/tools/riscv32-esp-elf/esp-13.2.0_20240530/riscv32-esp-elf/bin/riscv32-esp-elf-gcc.exe" CACHE STRING "C Compiler" FORCE)
#set(CMAKE_ASM_COMPILER "C:/Users/bleda.burat/.espressif/tools/riscv32-esp-elf/esp-13.2.0_20240530/riscv32-esp-elf/bin/riscv32-esp-elf-as.exe" CACHE STRING "ASM Compiler" FORCE)

idf_component_register(SRCS "json_encoder.c" "log_parser.c" "therapy_counter.c" "log_utils.c" "log_writer.c" "timer_utils.c" "proximity_sensor_control.c" "temperature_sensor_control.c" 
                            "temperature_sensor.c" 
                            "i2c_control.c" 
                            "json_parser.c" 
                            "laser_driver_control.c" 
                            "main.c" 
                            "timer_management.c" 
                            "storage_management.c" 
                            "temperature_alarm_control.c"
                            "lp_core_queue_manager.c"
                            "proximity_int_control.c"
                            "proximity_sensor_control.c"
                            "boot_button_control.c"
                            "ble_control.c"
                            "state_manager.c"
                            "timer_utils.c"
                            "transaction_manager.c"
                            "activation_command_manager.c"
                            "transaction_message_encoder.c"
                            "deep_sleep_manager.c"
                            "main_button_controller.c"
                            "device_initializer.c"
                            "log_parser.c"
                            "log_utils.c"
                            "log_writer.c"
                            "therapy_counter.c"
                            "json_encoder.c" 
                            "lp_core_main.c"
                            "lp_core/main.c"
                    INCLUDE_DIRS "." 
                                "lp_core"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/ulp/lp_core/include"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/ulp/lp_core/lp_core/include"
                                "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/ulp/ulp_common/include"
                                 "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/bt/include"
                                 "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/nvs_flash/include"
                                 "C:/Users/bleda.burat/esp/v5.3.1/esp-idf/components/json/cjson"
                    REQUIRES ulp nvs_flash json bt)


set(ulp_app_name "lp_core_firmware")

set(ulp_lp_core_sources "lp_core/main.c")

set(ulp_exp_dep_srcs "lp_core_main.c")

ulp_embed_binary(${ulp_app_name} "${ulp_lp_core_sources}" "${ulp_exp_dep_srcs}")
target_link_libraries(${COMPONENT_LIB} PUBLIC idf::ulp)