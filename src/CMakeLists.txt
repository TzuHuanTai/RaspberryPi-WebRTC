project(src)

add_library(${PROJECT_NAME}
    parser.cpp
)

find_package(yaml-cpp REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC signaling recorder boost_program_options yaml-cpp)
