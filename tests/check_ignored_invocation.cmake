# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

execute_process(COMMAND "${BIN}" invocation RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 10)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Qt invocation replay did not continue normally: ${result}: ${errors}")
endif()
if(NOT output STREQUAL "")
    message(FATAL_ERROR "Qt ignored invocation commands wrote to stdout: ${output}")
endif()
