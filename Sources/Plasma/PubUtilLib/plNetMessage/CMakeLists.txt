set(plNetMessage_SOURCES
    plNetMessage.cpp
    plNetMsgHelpers.cpp
)

set(plNetMessage_HEADERS
    plNetCommonMessage.h
    plNetMessage.h
    plNetMessageCreatable.h
    plNetMsgHelpers.h
    plNetMsgVersion.h
)

add_library(plNetMessage STATIC ${plNetMessage_SOURCES} ${plNetMessage_HEADERS})
target_link_libraries(
    plNetMessage
    PUBLIC
        CoreLib
        pnFactory
        pnKeyedObject
        pnNetBase
        pnNetCommon
        plNetCommon
        plStreamLogger
        plUnifiedTime
    PRIVATE
        pnMessage
        pnNucleusInc
        plCompression
        plSDL
)

source_group("Source Files" FILES ${plNetMessage_SOURCES})
source_group("Header Files" FILES ${plNetMessage_HEADERS})
