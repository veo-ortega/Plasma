set(plCrashHandler_SOURCES
    main.cpp
)

add_executable(plCrashHandler ${plCrashHandler_SOURCES})
target_link_libraries(
    plCrashHandler
    PRIVATE
        CoreLib
        pfCrashHandler
)

if(PLASMA_EXTERNAL_RELEASE)
    set_target_properties(plCrashHandler PROPERTIES OUTPUT_NAME "UruCrashHandler")
endif(PLASMA_EXTERNAL_RELEASE)


install(
    TARGETS plCrashHandler
    DESTINATION client
)
