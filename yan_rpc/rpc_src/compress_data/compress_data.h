#pragma once
#include <zstd.h>
#include <vector>
#include <string>
// 1 3 7 9  22
namespace yan_rpc {
enum class Level {
    FASTEST = 1,//压缩最快 压缩率最低
    DEFAULT = 3, //平衡时间和压缩率 默认level
    BETTER  = 7,
    BEST    = 9,
    MAX     = 22
};
class Compress {
public:
static Compress& GetInstance() {
        static Compress instance_;
        return instance_;
    }
    //压缩字符串
bool CompressString(const std::string& src, std::string& des, Level l = Level::DEFAULT);
    //解压字符串
bool DepressString(const std::string& src, std::string& des);
    //压缩二进制
bool CompressData(const char* src, size_t len, std::vector<char>& des, Level l = Level::DEFAULT);
    //解压二进制
bool DepressData (const char* src, size_t len, std::vector<char>& des);
size_t GetBound(size_t src_size) {
        size_t bound = ZSTD_compressBound(src_size);
        return bound;
    }
Compress(const Compress&) = delete;
Compress& operator= (const Compress&) = delete;
Compress(Compress&&) = delete;
Compress& operator= (Compress&&) = delete;
private:
Compress() = default;
~Compress() = default;
};
} //namespace yan_rpc