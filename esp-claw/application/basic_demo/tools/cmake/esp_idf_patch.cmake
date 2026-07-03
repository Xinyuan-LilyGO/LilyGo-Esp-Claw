set(BASIC_DEMO_PROJECT_LOG_PREFIX "[basic_demo]")
set(BASIC_DEMO_ESP_IDF_PATCH "${CMAKE_SOURCE_DIR}/tools/esp-idf.patch")

if(NOT DEFINED ENV{IDF_PATH} OR "$ENV{IDF_PATH}" STREQUAL "")
    message(FATAL_ERROR "${BASIC_DEMO_PROJECT_LOG_PREFIX} IDF_PATH environment variable is not set")
endif()

find_program(GIT_EXECUTABLE git REQUIRED)

if(EXISTS "${BASIC_DEMO_ESP_IDF_PATCH}")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --reverse --check "${BASIC_DEMO_ESP_IDF_PATCH}"
        WORKING_DIRECTORY "$ENV{IDF_PATH}"
        RESULT_VARIABLE BASIC_DEMO_PATCH_ALREADY_APPLIED
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(BASIC_DEMO_PATCH_ALREADY_APPLIED EQUAL 0)
        message(STATUS "${BASIC_DEMO_PROJECT_LOG_PREFIX} ESP-IDF patch already applied: ${BASIC_DEMO_ESP_IDF_PATCH}")
    else()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} apply "${BASIC_DEMO_ESP_IDF_PATCH}"
            WORKING_DIRECTORY "$ENV{IDF_PATH}"
            RESULT_VARIABLE BASIC_DEMO_PATCH_APPLY_RESULT
            OUTPUT_VARIABLE BASIC_DEMO_PATCH_APPLY_STDOUT
            ERROR_VARIABLE BASIC_DEMO_PATCH_APPLY_STDERR
        )

        if(NOT BASIC_DEMO_PATCH_APPLY_RESULT EQUAL 0)
            message(FATAL_ERROR
                "${BASIC_DEMO_PROJECT_LOG_PREFIX} Failed to apply ESP-IDF patch: ${BASIC_DEMO_ESP_IDF_PATCH}\n"
                "stdout:\n${BASIC_DEMO_PATCH_APPLY_STDOUT}\n"
                "stderr:\n${BASIC_DEMO_PATCH_APPLY_STDERR}")
        endif()

        message(STATUS "${BASIC_DEMO_PROJECT_LOG_PREFIX} Applied ESP-IDF patch: ${BASIC_DEMO_ESP_IDF_PATCH}")
    endif()
else()
    message(FATAL_ERROR "${BASIC_DEMO_PROJECT_LOG_PREFIX} ESP-IDF patch file not found: ${BASIC_DEMO_ESP_IDF_PATCH}")
endif()
