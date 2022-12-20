set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wfatal-errors -Wextra -Wnon-virtual-dtor -pedantic)

add_executable(future_example future_example.cpp)
add_executable(shared_future_example shared_future_example.cpp)

find_package(Boost 1.80 REQUIRED)
find_package(fmt REQUIRED)
target_link_libraries(future_example saf fmt::fmt Boost::headers)
target_link_libraries(shared_future_example saf fmt::fmt Boost::headers)
