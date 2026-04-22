if(NOT DEFINED INPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE is required")
endif()

if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

if(NOT DEFINED RAYTHM_APP_ICON_ICO)
    message(FATAL_ERROR "RAYTHM_APP_ICON_ICO is required")
endif()

file(READ "${INPUT_FILE}" raythm_app_icon_rc)
string(REPLACE "@RAYTHM_APP_ICON_ICO@" "${RAYTHM_APP_ICON_ICO}" raythm_app_icon_rc "${raythm_app_icon_rc}")
file(WRITE "${OUTPUT_FILE}" "${raythm_app_icon_rc}")
