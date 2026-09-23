#pragma once
#include "log_manager.h"

#include <stdint.h>
#include <string>
namespace yan_rpc {
struct RpcHead {
uint32_t magic_number; //魔数
uint32_t message_size; //长度 不包括head
uint32_t sequence_id; //序列号
static constexpr uint32_t MagicNumber = 0x12345678;

};
class RpcMessage {
public:
virtual ~RpcMessage() = default;
virtual bool Serialization(std::string& output) = 0;
virtual bool Deserialization(std::string& input) = 0;
void SetterSequenceId(uint32_t id) {
    sequence_id_ = id;
}
uint32_t GetSequenceid() const{
    return sequence_id_;
}
protected:
uint32_t sequence_id_;
};
class RpcRequest : public RpcMessage {
public: 
bool Serialization(std::string& output) override;
bool Deserialization(std::string& input) override;
void SetServiceName(const std::string& name) {
    service_name_ = name;
}
void SetMethodName(const std::string& name) {
    method_name_ = name;
}
void SetPayload(const std::string& load) {
    payload_ = load;
}
std::string GetServiceName() const {return service_name_;}
std::string GetMethodName() const {return method_name_;}
std::string GetPayload() const {return payload_;}
uint32_t GetSequenceid() const{
    return sequence_id_;
}
private:
std::string service_name_; //服务名
std::string method_name_; //措施名
std::string payload_; //参数
};
class RpcReponse : public RpcMessage {
public:
bool Serialization(std::string& output) override;
bool Deserialization(std::string& input) override;
void SetResultData(const std::string& result_data) {result_data_ = result_data;}
void SetErrorCode(uint32_t error_code) {error_code_ = error_code;}
void SetErrorMessage(std::string error_message) {error_message_ = error_message;}
std::string GetResultData() {return result_data_;}
std::string GetErrorMessage() {return error_message_;}
uint32_t GetErrorCode() {return error_code_;}


private:
std::string result_data_;
uint32_t error_code_{0};
std::string error_message_;   
};

}// namespace yan_rpc
