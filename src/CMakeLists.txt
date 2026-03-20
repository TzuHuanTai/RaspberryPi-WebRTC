project(src)

add_library(${PROJECT_NAME}
    parser.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC signaling recorder boost_program_options)
