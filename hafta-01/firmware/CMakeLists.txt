#############################################################################################################################
# file:  CMakeLists.txt
# brief: Template "CMakeLists.txt" for building of executables and static libraries.
#
# usage: Edit "VARIABLES"-section to suit project requirements.
#        For debug build:
#          cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake  -S ./ -B Debug -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
#          make -C Debug VERBOSE=1 -j
#        For release build:
#          cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake  -S ./ -B Release -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
#          make -C Release VERBOSE=1 -j
#############################################################################################################################
cmake_minimum_required(VERSION 3.20)

###################### CONSTANTS ######################################
set (PROJECT_TYPE_EXECUTABLE          "exe")
set (PROJECT_TYPE_STATIC_LIBRARY      "static-lib")
set (MCPU_CORTEX_M0				      "-mcpu=cortex-m0")
set (MCPU_CORTEX_M0PLUS				  "-mcpu=cortex-m0plus")
set (MCPU_CORTEX_M3				      "-mcpu=cortex-m3")
set (MCPU_CORTEX_M4				      "-mcpu=cortex-m4")
set (MCPU_CORTEX_M7				      "-mcpu=cortex-m7")
set (MCPU_CORTEX_M33				  "-mcpu=cortex-m33")
set (MCPU_CORTEX_M55				  "-mcpu=cortex-m55")
set (MCPU_CORTEX_M85				  "-mcpu=cortex-m85")
set (MFPU_FPV4_SP_D16                 "-mfpu=fpv4-sp-d16")
set (MFPU_FPV4                        "-mfpu=vfpv4")
set (MFPU_FPV5_D16                    "-mfpu=fpv5-d16")
set (RUNTIME_LIBRARY_REDUCED_C        "--specs=nano.specs")
set (RUNTIME_LIBRARY_STD_C            "")
set (RUNTIME_LIBRARY_SYSCALLS_MINIMAL "--specs=nosys.specs")
set (RUNTIME_LIBRARY_SYSCALLS_NONE    "")
set (MFLOAT_ABI_SOFTWARE              "-mfloat-abi=soft")
set (MFLOAT_ABI_HARDWARE              "-mfloat-abi=hard")
set (MFLOAT_ABI_MIX                   "-mfloat-abi=softfp")
#######################################################################

###################### VARIABLES ######################################
set (PROJECT_NAME             "TimingHomework")
set (PROJECT_TYPE             "exe")
set (LINKER_SCRIPT            "../STM32F407VGTX_FLASH.ld")
set (MCPU                     "-mcpu=Cortex-M4")
set (MFPU                 "-mfpu=fpv4-sp-d16")
set (MFLOAT_ABI               "-mfloat-abi=hard")
set (RUNTIME_LIBRARY          "--specs=nano.specs")
set (RUNTIME_LIBRARY_SYSCALLS "--specs=nosys.specs")

# Surucu kaynaklari: git submodule, sabit etiket (bkz. docs/setup.md)
set (HAL_DIR      third_party/stm32f4xx-hal-driver)
set (CMSIS_DEV    third_party/cmsis-device-f4)
set (CMSIS_CORE   third_party/cmsis-core)
set (RTOS_DIR     third_party/FreeRTOS-Kernel)

set (PROJECT_SOURCES
	# LIST SOURCE FILES HERE
	Startup/startup_stm32f407vgtx.s
	Core/Src/main.c
	Core/Src/system_clock.c
	Core/Src/gpio.c
	Core/Src/dma.c
	Core/Src/usart.c
	Core/Src/tim.c
	Core/Src/stm32f4xx_it.c
	Core/Src/stm32f4xx_hal_msp.c
	Core/Src/stm32f4xx_hal_timebase_tim.c
	Core/Src/freertos_hooks.c
	Core/Src/syscalls.c
	Core/Src/sysmem.c

	# Uygulama katmani (odev mantigi)
	App/Src/app_time.c
	App/Src/app_button.c
	App/Src/app_msg.c
	App/Src/app_record.c
	App/Src/app_telemetry.c
	App/Src/app_uart_tx.c
	App/Src/app_tasks.c

	# CMSIS device: SystemInit + SystemCoreClock (sablon, degistirilmeden)
	${CMSIS_DEV}/Source/Templates/system_stm32f4xx.c

	# HAL: sadece kullanilan moduller
	${HAL_DIR}/Src/stm32f4xx_hal.c
	${HAL_DIR}/Src/stm32f4xx_hal_rcc.c
	${HAL_DIR}/Src/stm32f4xx_hal_rcc_ex.c
	${HAL_DIR}/Src/stm32f4xx_hal_gpio.c
	${HAL_DIR}/Src/stm32f4xx_hal_cortex.c
	${HAL_DIR}/Src/stm32f4xx_hal_uart.c
	${HAL_DIR}/Src/stm32f4xx_hal_dma.c
	${HAL_DIR}/Src/stm32f4xx_hal_tim.c
	${HAL_DIR}/Src/stm32f4xx_hal_tim_ex.c
	${HAL_DIR}/Src/stm32f4xx_hal_pwr.c
	${HAL_DIR}/Src/stm32f4xx_hal_flash.c

	# FreeRTOS cekirdegi (timers/event_groups/stream_buffer kullanilmiyor)
	${RTOS_DIR}/tasks.c
	${RTOS_DIR}/queue.c
	${RTOS_DIR}/list.c
	${RTOS_DIR}/portable/GCC/ARM_CM4F/port.c
	${RTOS_DIR}/portable/MemMang/heap_4.c
	)

set (PROJECT_DEFINES
	# LIST COMPILER DEFINITIONS HERE
	STM32F407xx
	USE_HAL_DRIVER
	HSE_VALUE=8000000
    )

set (PROJECT_INCLUDES
	# LIST INCLUDE DIRECTORIES HERE
	Core/Inc
	App/Inc
	${HAL_DIR}/Inc
	${CMSIS_DEV}/Include
	${CMSIS_CORE}/Core/Include
	${RTOS_DIR}/include
	${RTOS_DIR}/portable/GCC/ARM_CM4F
    )

############ MODIFY ACCORDING TO REQUIREMENTS) ########################

#######################################################################

################## PROJECT SETUP ######################################
project(${PROJECT_NAME})
enable_language(ASM)

if (${PROJECT_TYPE} MATCHES ${PROJECT_TYPE_EXECUTABLE})
  add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>)
elseif (${PROJECT_TYPE} MATCHES ${PROJECT_TYPE_STATIC_LIBRARY})
  add_library(${PROJECT_NAME} ${PROJECT_SOURCES})
endif()

add_compile_definitions (${PROJECT_DEFINES})
include_directories (${PROJECT_INCLUDES})

set (CMAKE_EXECUTABLE_SUFFIX ".elf")
set (CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set (CMAKE_C_FLAGS "${MCPU} -std=gnu11 ${MFPU} ${MFLOAT_ABI} ${RUNTIME_LIBRARY} -mthumb -Wall -Werror")
set (CMAKE_EXE_LINKER_FLAGS "-T${LINKER_SCRIPT} ${RUNTIME_LIBRARY_SYSCALLS} -Wl,-Map=test.map -Wl,--gc-sections -static -Wl,--start-group -lc -lm -Wl,--end-group")
set (CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp")
