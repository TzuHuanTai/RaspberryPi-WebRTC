project(ipc)

aux_source_directory(${PROJECT_SOURCE_DIR} IPC_FILES)

add_library(${PROJECT_NAME} ${IPC_FILES})
