set(HEADERS
    constants.h
)

set(SOURCES
    main.cpp
    ${HEADERS}
)

add_executable(RuuviLogCollector ${SOURCES})
target_include_directories(RuuviLogCollector PUBLIC ${CMAKE_SOURCE_DIR}/libblepp)
target_link_libraries(RuuviLogCollector ${BLUEZ_LIBRARIES} ble++)

set_target_properties(RuuviLogCollector PROPERTIES
    CXX_STANDARD 17
    CMAKE_CXX_STANDARD_REQUIRED YES
    RUNTIME_OUTPUT_DIRECTORY ..)
