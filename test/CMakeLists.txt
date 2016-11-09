project("json_test")

set(SRC "test.c")
add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} json)
