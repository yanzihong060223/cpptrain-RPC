#pragma once
#include "../../log_manager.h"

#include <string>
#include <vector>
#include <memory>
#include <mutex>
namespace yan_rpc {
class LogBalancer {
public:
virtual ~LogBalancer() = default;
virtual const std::string Select(const std::vector<std::string>& servers) = 0; //子类按照负载方法去重写
static  std::string SelectServer(const std::vector<std::string>& servergroups); //对外的select接口
static  std::shared_ptr<LogBalancer> GetInstance(); //获取对象 默认随机
static bool Init(const std::string type); //按需初始化
LogBalancer(const LogBalancer&) = delete;
LogBalancer& operator= (const LogBalancer&) = delete;
LogBalancer(LogBalancer&&) = delete;
LogBalancer& operator= (LogBalancer&&) = delete;
protected:
LogBalancer() = default; // protected 子类可以调用
private:
static std::shared_ptr<LogBalancer> instance_ ;
static std:: mutex mtx_;

}; //产品
}//yan_rpc