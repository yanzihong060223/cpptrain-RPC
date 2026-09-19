#include "aes_encrypt.h"
#include "../../log_manager.h"

#include <string>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <random>
#include <vector>
namespace yan_rpc {
  const std::string BASE64_CHARS =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; //标准base64 字符表

bool AesEncrypt:: Init(const nlohmann::json& config) {
    try {
    main_key_ = config.value("main_key", "RPC_Secret_Key_2024_Production!@#$%^&*");
    } catch(const std::exception& e) {
        LOGGER_ERROR("Encryption Instance Init Error {}", e.what());
        return false;
    }
    return true;
}
std::string AesEncrypt::Base64Encode(std::string& input) const {
    std::string ret;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3]; // 存储3个原始字节
        unsigned char char_array_4[4]; // 存储4个编码后的6位值

        const unsigned char *bytes_to_encode =
            reinterpret_cast<const unsigned char *>(input.data());
        size_t in_len = input.length();

        // 主编码循环：每次处理3个字节
        while (in_len--)
        {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3)
            {
                // 将3个字节(24位)转换为4个6位值
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                // 将6位值转换为Base64字符
                for (i = 0; i < 4; i++)
                {
                    ret += BASE64_CHARS[char_array_4[i]];
                }
                i = 0;
            }
        }

        // 处理不足3字节的剩余数据
        if (i)
        {
            // 用'\0'补齐到3字节
            for (j = i; j < 3; j++)
            {
                char_array_3[j] = '\0';
            }

            // 转换为6位值
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            // 只编码有效的部分
            for (j = 0; j < i + 1; j++)
            {
                ret += BASE64_CHARS[char_array_4[j]];
            }

            // 用'='填充到4的倍数
            while ((i++ < 3))
            {
                ret += '=';
            }
        }
        return ret;
    }
std::string AesEncrypt::Base64Decode (std::string& input) const {
        std::string ret;
        // 建立Base64字符到索引值的映射表
        std::vector<int> base64_map(256, -1);
        for (size_t i = 0; i < BASE64_CHARS.size(); i++)
        {
            base64_map[BASE64_CHARS[i]] = i;
        }

        size_t in_len = input.size();
        int i = 0;
        int j = 0;
        int in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];

        // 主解码循环：每次处理4个Base64字符
        while (in_len-- && input[in_] != '=')
        {
            char_array_4[i++] = input[in_];
            in_++;
            if (i == 4)
            {
                // 将Base64字符转换为6位值
                for (i = 0; i < 4; i++)
                {
                    char_array_4[i] = base64_map[char_array_4[i]];
                }

                // 将4个6位值转换为3个8位字节
                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; i < 3; i++)
                {
                    ret += char_array_3[i];
                }
                i = 0;
            }
        }

        // 处理剩余的字符（考虑填充）
        if (i)
        {
            for (j = i; j < 4; j++)
            {
                char_array_4[j] = 0;
            }

            for (j = 0; j < 4; j++)
            {
                char_array_4[j] = base64_map[char_array_4[j]];
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            // 只取有效的字节（排除填充）
            for (j = 0; j < i - 1; j++)
            {
                ret += char_array_3[j];
            }
        }
        return ret;
}

std::string AesEncrypt:: GetSession(size_t length) {
    std::string result;
    result.resize(length);
    std::random_device rd; 
    std::mt19937 gen_(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < length; i++) {
       result[i] = static_cast<char>(dis(gen_));
    }    
    return result;
}
std::string AesEncrypt::Enalgorithm(std::string& input, std::string& key) const{
    std::string result;
    result.reserve(input.size());
    for (int i = 0; i < input.size(); i++) {
        unsigned char a = input[i];
        unsigned char b =  key[i % key.size()];
        unsigned char c = (i % 256);
        result.push_back(static_cast<char>((a - b - c + 512) % 256));
    }
    return result;
}
std::string AesEncrypt::Dealgorithm (std::string& input, std::string& key) const{
    std::string result;
    result.reserve(input.size());
    for (int i = 0; i < input.size(); i++) {
         unsigned char a = input[i];
         unsigned char b =  key[i % key.size()];
         unsigned char c = (i % 256);
         result.push_back(static_cast<char>((a + b + c) % 256));
    }
    return result;
}
bool AesEncrypt::Encrypt(std::string& input, std::string& ciptext) {
   try {
        if (input.size() == 0) {
            return false;
        }
       std::string session = GetSession(KEY_LENGTH_);
       //会话加密数据
       std::string first_data = Enalgorithm(input, session);
       if (first_data.size() == 0) {
            LOGGER_ERROR("First Encryption Failed");
            return false;
       }
       //加密会话
       std::string second_data = Enalgorithm(session, main_key_);
       if (second_data.size() == 0) {
          LOGGER_ERROR("Second Encryption Failed");
          return false;
       }
       std::string result = second_data + first_data;
       ciptext = Base64Encode(result);
       return true;
   } catch(const std::exception& e) {
        LOGGER_ERROR("Encryption Failed {}", e.what());
        return false;
   }
}
bool AesEncrypt::Decrypt(std::string& input, std::string& planexpt) {
    try {
        std::string data = Base64Decode(input);
        if (data.size() < KEY_LENGTH_) {
            LOGGER_ERROR ("No Current Size");
            return false;
        }
        std::string first_data = data.substr(0, KEY_LENGTH_);
        std::string real_data = data.substr(KEY_LENGTH_);
        std::string session = Dealgorithm(first_data, main_key_);
        planexpt = Dealgorithm(real_data, session);
        return true;
    } catch(std::exception& e) {
        LOGGER_ERROR("Decryption Failed {}", e.what());
        return  false;
    }
}
}; //namespace yan_rpc
