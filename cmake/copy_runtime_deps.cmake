if(NOT DEFINED APP_DIR)
    message(FATAL_ERROR "APP_DIR is required")
endif()

if(NOT DEFINED SEARCH_DIR)
    message(FATAL_ERROR "SEARCH_DIR is required")
endif()

if(NOT DEFINED OBJDUMP)
    set(OBJDUMP objdump)
endif()

set(system_dll_regex
    "^(KERNEL32|USER32|GDI32|SHELL32|WS2_32|ADVAPI32|OLE32|OLEAUT32|COMDLG32|COMCTL32|WINMM|IMM32|SETUPAPI|VERSION|DWMAPI|UXTHEME|NETAPI32|CRYPT32|SHLWAPI|MSVCRT|UCRTBASE|RPCRT4|SECHOST|DBGHELP|IPHLPAPI|DNSAPI|WINSPOOL\\.DRV|MPR|WTSAPI32|D3D11|DXGI|D3D12|D3DCOMPILER_47|AUTHZ|AVRT|BCRYPT|CRYPTSP|DWRITE|DXVA2|EVR|MF|MFPLAT|MFREADWRITE|NCRYPT|NTDLL|PROPSYS|SECUR32|SHCORE|USERENV|WINHTTP)\\.DLL$"
)

file(GLOB_RECURSE seed_bins LIST_DIRECTORIES false
    "${APP_DIR}/*.exe"
    "${APP_DIR}/*.dll"
)

set(queue ${seed_bins})
set(scanned_bins)
set(unresolved_deps)

while(queue)
    list(POP_FRONT queue current_bin)
    if(NOT EXISTS "${current_bin}")
        continue()
    endif()

    get_filename_component(current_bin "${current_bin}" ABSOLUTE)
    list(FIND scanned_bins "${current_bin}" already_scanned)
    if(NOT already_scanned EQUAL -1)
        continue()
    endif()
    list(APPEND scanned_bins "${current_bin}")

    execute_process(
        COMMAND "${OBJDUMP}" -p "${current_bin}"
        OUTPUT_VARIABLE dump_output
        ERROR_QUIET
    )

    string(REGEX MATCHALL "DLL Name: [^\r\n]+" dll_lines "${dump_output}")
    foreach(line IN LISTS dll_lines)
        string(REGEX REPLACE ".*DLL Name: *" "" dll_name "${line}")
        string(STRIP "${dll_name}" dll_name)
        if(dll_name STREQUAL "")
            continue()
        endif()

        string(TOUPPER "${dll_name}" dll_upper)
        if(dll_upper MATCHES "^API-MS-WIN-" OR dll_upper MATCHES "^EXT-MS-" OR dll_upper MATCHES "${system_dll_regex}")
            continue()
        endif()

        set(dep_in_app "${APP_DIR}/${dll_name}")
        set(dep_in_search "${SEARCH_DIR}/${dll_name}")

        if(EXISTS "${dep_in_app}")
            list(APPEND queue "${dep_in_app}")
        elseif(EXISTS "${dep_in_search}")
            file(COPY "${dep_in_search}" DESTINATION "${APP_DIR}")
            message(STATUS "Copied runtime dependency: ${dll_name}")
            list(APPEND queue "${APP_DIR}/${dll_name}")
        else()
            list(APPEND unresolved_deps "${dll_name}")
        endif()
    endforeach()
endwhile()

if(unresolved_deps)
    list(REMOVE_DUPLICATES unresolved_deps)
    message(STATUS "Unresolved runtime deps: ${unresolved_deps}")
endif()
