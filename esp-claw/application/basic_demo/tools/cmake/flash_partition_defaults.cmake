set(BASIC_DEMO_PROJECT_LOG_PREFIX "[basic_demo]")
set(BASIC_DEMO_FLASH_SIZE "")
set(BASIC_DEMO_BOARD_MANAGER_DEFAULTS "${CMAKE_SOURCE_DIR}/components/gen_bmgr_codes/board_manager.defaults")
if(EXISTS "${BASIC_DEMO_BOARD_MANAGER_DEFAULTS}")
    file(STRINGS "${BASIC_DEMO_BOARD_MANAGER_DEFAULTS}" _flash_line REGEX "^CONFIG_ESPTOOLPY_FLASHSIZE_(4|8|16)MB=y$")
    if(_flash_line)
        list(GET _flash_line 0 _flash_line)
        string(REGEX REPLACE "^CONFIG_ESPTOOLPY_FLASHSIZE_((4|8|16)MB)=y$" "\\1" BASIC_DEMO_FLASH_SIZE "${_flash_line}")
    endif()
endif()

if(BASIC_DEMO_FLASH_SIZE)
    if(EXISTS "${CMAKE_SOURCE_DIR}/sdkconfig")
        file(READ "${CMAKE_SOURCE_DIR}/sdkconfig" BASIC_DEMO_SDKCONFIG_CONTENT)
        string(REGEX REPLACE "(^|\\n)CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"[^\\n]*\"\\n" "\\1" BASIC_DEMO_SDKCONFIG_CONTENT "${BASIC_DEMO_SDKCONFIG_CONTENT}")
        string(REGEX REPLACE "(^|\\n)CONFIG_PARTITION_TABLE_FILENAME=\"[^\\n]*\"\\n" "\\1" BASIC_DEMO_SDKCONFIG_CONTENT "${BASIC_DEMO_SDKCONFIG_CONTENT}")
        file(WRITE "${CMAKE_SOURCE_DIR}/sdkconfig" "${BASIC_DEMO_SDKCONFIG_CONTENT}")
    endif()

    set(BASIC_DEMO_PARTITION_DEFAULTS "${CMAKE_BINARY_DIR}/basic_demo_partition_auto.defaults")
    file(WRITE "${BASIC_DEMO_PARTITION_DEFAULTS}"
        "# Auto-generated from flash size selection. Do not edit.\n"
        "CONFIG_PARTITION_TABLE_CUSTOM=y\n"
        "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions_${BASIC_DEMO_FLASH_SIZE}.csv\"\n")

    if(SDKCONFIG_DEFAULTS)
        set(SDKCONFIG_DEFAULTS "${SDKCONFIG_DEFAULTS};${BASIC_DEMO_PARTITION_DEFAULTS}")
    elseif(NOT "$ENV{SDKCONFIG_DEFAULTS}" STREQUAL "")
        set(SDKCONFIG_DEFAULTS "$ENV{SDKCONFIG_DEFAULTS};${BASIC_DEMO_PARTITION_DEFAULTS}")
    else()
        set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults;${BASIC_DEMO_PARTITION_DEFAULTS}")
    endif()

    message(STATUS "${BASIC_DEMO_PROJECT_LOG_PREFIX} Partition table auto-selected: partitions_${BASIC_DEMO_FLASH_SIZE}.csv")
endif()
