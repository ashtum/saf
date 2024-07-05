set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wfatal-errors -Wextra -Wnon-virtual-dtor -pedantic)

add_executable(unit_test main.cpp promise_test.cpp future_test.cpp shared_future_test.cpp)

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
target_link_libraries(unit_test saf Boost::headers Boost::unit_test_framework)

add_test(unit_test unit_test)
