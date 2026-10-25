# Wine recognizes this exact 32-byte marker immediately after IMAGE_DOS_HEADER.
file( READ "${MODULE}" SIGNATURE OFFSET 64 LIMIT 32 HEX )
if( NOT SIGNATURE STREQUAL "57696e65206275696c74696e20444c4c00000000000000000000000000000000" )
    message( FATAL_ERROR "Missing Wine builtin DLL marker: ${MODULE}" )
endif()
