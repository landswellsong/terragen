cmake_minimum_required(VERSION 2.6)
project(terragen)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

include_directories(${GLUT_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR})

add_executable(terragen terragen.c)
target_link_libraries(terragen ${GLUT_LIBRARIES} ${OPENGL_LIBRARIES})