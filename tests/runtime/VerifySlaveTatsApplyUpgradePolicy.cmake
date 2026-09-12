if(NOT DEFINED SOURCE_FILE)
    message(FATAL_ERROR "SOURCE_FILE is required")
endif()

file(READ "${SOURCE_FILE}" source)
string(REPLACE "\r" "" compact_source "${source}")
string(REPLACE "\n" "" compact_source "${compact_source}")
string(REPLACE "\t" "" compact_source "${compact_source}")
string(REPLACE " " "" compact_source "${compact_source}")
string(FIND "${compact_source}"
    "add_and_get_tattoo(actor,tattooHandle,request.slot,false,false,true)"
    upgrade_enabled_call)

if(upgrade_enabled_call EQUAL -1)
    message(FATAL_ERROR
        "applyToSlot must call add_and_get_tattoo with try_upgrade=true")
endif()
