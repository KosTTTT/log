project(log C CXX)
set(CMAKE_CXX_STANDARD 20)#the minimum requred version of C++
set(sources
log.cpp
)
set(headers
log.hpp
)

add_library(${PROJECT_NAME} SHARED
${headers}
${sources}
)
