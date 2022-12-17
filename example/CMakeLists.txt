set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wfatal-errors -Wextra -Wnon-virtual-dtor -pedantic)

add_executable(example example.cpp)

find_package(Boost 1.80 REQUIRED)
find_package(fmt REQUIRED)
target_link_libraries(example saf fmt::fmt Boost::headers)
