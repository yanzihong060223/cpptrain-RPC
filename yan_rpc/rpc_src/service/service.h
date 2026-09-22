#pragma once 
#include "../../log_manager.h"

#include <string>
#include <mutex>
namespace yan_rpc {
class Service {
public:
virtual ~Service() = default;
virtual std::string GetService() const = 0;
virtual bool HandleService(std::string  method_name, std::string& result, std::string args) = 0;
};
}   //namespace yan_rpc