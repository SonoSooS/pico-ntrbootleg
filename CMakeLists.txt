cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(asd C CXX ASM)

pico_sdk_init()

add_executable(negcart
    src/main.c
    src/coreboot.c
    src/frm.c
    src/neg.c
    src/picart_setup.c
    src/bf_table.c
    src/bf.c
    src/png.c
    )

pico_generate_pio_header(negcart ${CMAKE_CURRENT_LIST_DIR}/src/nicart.pio)

pico_set_linker_script(negcart ${CMAKE_CURRENT_LIST_DIR}/neg.ld)
#pico_set_linker_script(negcart ${CMAKE_CURRENT_LIST_DIR}/neg_ram.ld)

pico_set_binary_type(negcart blocked_ram)
#pico_set_binary_type(negcart no_flash)

target_link_libraries(negcart
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_dma
    hardware_gpio
    hardware_irq
    hardware_exception
    )

#target_compile_options(negcart PUBLIC -Os -g)
#target_link_options(negcart PUBLIC -Os -g)

#target_compile_definitions(hardware_timer INTERFACE PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1)

add_compile_definitions(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000)
add_compile_definitions(PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=0)
add_compile_definitions(PICO_NO_FPGA_CHECK=1)
#add_compile_definitions(PICO_FLOAT_SUPPORT_ROM_V1=0)
#add_compile_definitions(PICO_DOUBLE_SUPPORT_ROM_V1=0)
#add_compile_definitions(PICO_RP2040_B1_SUPPORTED=0)
#add_compile_definitions(PICO_RP2040_B2_SUPPORTED=0)

pico_enable_stdio_uart(negcart 0)
pico_enable_stdio_usb(negcart 1)

pico_add_extra_outputs(negcart)
