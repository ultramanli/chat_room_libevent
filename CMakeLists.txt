cmake_minimum_required (VERSION 2.8)
project (pubsub)
set (CMAKE_CXX_STANDARD 11)

set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories (include)
find_library (HIREDIS_LIB hiredis HINTS /usr/local/lib)
find_library (LIBEVENT_LIB event HINTS /usr/local/lib)
find_library (PTHREAD_LIB pthread HINTS /usr/local/lib)

# add_executable (publisher ${PROJECT_SOURCE_DIR}/src/publisher.cpp ${PROJECT_SOURCE_DIR}/src/redis_publisher.cpp)
# target_link_libraries (publisher ${HIREDIS_LIB} ${LIBEVENT_LIB} ${PTHREAD_LIB})

add_executable (client ${PROJECT_SOURCE_DIR}/src/subscriber.cpp ${PROJECT_SOURCE_DIR}/src/redis_subscriber.cpp)
target_link_libraries (client ${HIREDIS_LIB} ${LIBEVENT_LIB} ${PTHREAD_LIB})

add_executable (server ${PROJECT_SOURCE_DIR}/src/server.cpp)
target_link_libraries (server ${HIREDIS_LIB} ${LIBEVENT_LIB} ${PTHREAD_LIB})
