find_package(gRPC CONFIG QUIET)

set(MANAGER_SOURCES
    manager.cpp
    src/rpc/grpc_server.cpp
    src/rpc/query_service.cpp
    src/rpc/rpc_client.cpp
    src/host_manager.cpp
    src/query_manager.cpp
    
)

add_executable(manager ${MANAGER_SOURCES})

target_include_directories(manager PUBLIC
    ${PROJECT_SOURCE_DIR}/manager/include
)

set(MANAGER_LIBS monitor_proto gRPC::grpc++)

find_package(PkgConfig QUIET)

set(MYSQL_LINK_LIBS)

if(PkgConfig_FOUND)
    pkg_check_modules(MYSQL mysqlclient)
endif()

if(MYSQL_FOUND)
    list(APPEND MYSQL_LINK_LIBS ${MYSQL_LIBRARIES})
    target_include_directories(manager PUBLIC ${MYSQL_INCLUDE_DIRS})
    if(MYSQL_LIBRARY_DIRS)
        target_link_directories(manager PUBLIC ${MYSQL_LIBRARY_DIRS})
    endif()
else()
    find_library(MYSQLCLIENT_LIBRARY NAMES mysqlclient mysql)
    find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)

    if(MYSQLCLIENT_LIBRARY)
        list(APPEND MYSQL_LINK_LIBS ${MYSQLCLIENT_LIBRARY})
    endif()

    if(MYSQL_INCLUDE_DIR)
        target_include_directories(manager PUBLIC ${MYSQL_INCLUDE_DIR})
    endif()

    if(NOT MYSQL_LINK_LIBS)
        message(FATAL_ERROR "MySQL client library not found, manager requires mysqlclient")
    endif()
endif()

list(APPEND MANAGER_LIBS ${MYSQL_LINK_LIBS})

target_link_libraries(manager PUBLIC ${MANAGER_LIBS})