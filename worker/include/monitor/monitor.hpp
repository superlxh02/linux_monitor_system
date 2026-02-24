#pragma once

#include <string>
#include "monitor_info.pb.h"
namespace monitor
{
    /// 监控接口类
    class Monitor{
        public:
            Monitor(){}
            virtual ~Monitor(){}
            //更新监控信息
            virtual void update(monitor::proto::MonitorInfo *monitor_info) = 0;
            //停止监控
            virtual void stop() = 0;
    };
    
} // namespace monitor
