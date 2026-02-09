add_library(orderflow_c SHARED
    src/orderbook_c.cpp
    ${PROJECT_SOURCE_DIR}/src/OrderBook.cpp
)

target_include_directories(orderflow_c PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(orderflow_c PRIVATE cxx_std_17)
target_compile_options(orderflow_c PRIVATE -Wall -Wextra -Wpedantic)

# Test executable (compiled as C to prove C compatibility)
enable_language(C)
add_executable(test_c_wrapper test/test_c_wrapper.c)
set_target_properties(test_c_wrapper PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(test_c_wrapper orderflow_c)
target_include_directories(test_c_wrapper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
