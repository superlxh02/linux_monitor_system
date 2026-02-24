# 尝试 CONFIG 模式，如果失败则使用 MODULE 模式
find_package(protobuf CONFIG QUIET)
find_package(gRPC CONFIG QUIET)
find_package(c-ares CONFIG QUIET)


set(PROTO_FILES
    monitor_info.proto
    cpu_load.proto
    cpu_softirq.proto
    cpu_stat.proto
    mem_info.proto
    net_info.proto
    disk_info.proto
    query_api.proto
)
add_library(monitor_proto ${PROTO_FILES}) 


# 根据找到的包配置链接库
if(TARGET protobuf::libprotobuf)
    target_link_libraries(monitor_proto PUBLIC protobuf::libprotobuf)
else()
    target_link_libraries(monitor_proto PUBLIC ${Protobuf_LIBRARIES})
    target_include_directories(monitor_proto PUBLIC ${Protobuf_INCLUDE_DIRS})
endif()

if(TARGET gRPC::grpc++)
    target_link_libraries(monitor_proto PUBLIC gRPC::grpc gRPC::grpc++)
else()
    target_link_libraries(monitor_proto PUBLIC ${GRPC_LIBRARY} ${GRPCPP_LIBRARY})
endif()

target_include_directories(monitor_proto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ) 


if(TARGET gRPC::grpc_cpp_plugin)
    get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
else()
    set(grpc_cpp_plugin_location ${GRPC_CPP_PLUGIN})
endif()

# 生成 Protobuf 序列化代码
protobuf_generate(
    TARGET monitor_proto
    LANGUAGE cpp
    )

# 生成 gRPC 服务代码
protobuf_generate(
    TARGET monitor_proto 
    LANGUAGE grpc 
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc 
    PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}"
    )
