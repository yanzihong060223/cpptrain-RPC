#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <atomic>
namespace yan_rpc {
inline nlohmann::json AesEncryptConfig = {{"main_key", "RPC_Secret_Key_2024_Production!@#$%^&*"}};
class AesEncrypt {
public:
static AesEncrypt& GetInstance() {
    static AesEncrypt instance;
    return instance;
}
bool Init(const nlohmann::json& config);
bool Encrypt(std::string& input, std::string& ciptext);
bool Decrypt(std::string& input, std::string& planexpt);
AesEncrypt(const AesEncrypt&) = delete;
AesEncrypt& operator= (const AesEncrypt&) = delete;
AesEncrypt(AesEncrypt&&) = delete;
AesEncrypt& operator= (AesEncrypt&&) = delete;
private:
AesEncrypt() = default;
~AesEncrypt() = default;
std::string Base64Encode(std::string& input) const;
std::string Base64Decode(std::string& input) const;
std::string GetSession(size_t length); //获得会话密钥
std::string Enalgorithm(std::string& input, std::string& key) const; //加密算法实现
std::string Dealgorithm(std::string& input, std::string& key) const ; //解密算法实现


private:
static constexpr size_t KEY_LENGTH_ = 32; //会话密钥长度
std::string main_key_; // 主密钥
static std::atomic<bool> is_inited_;


};
}// namespace yan_rpc