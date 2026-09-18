#include "compress_data.h"
#include "../../log_manager.h"

#include <vector>
#include <string>

namespace yan_rpc {
bool Compress::CompressString(const std::string& src, std::string& des, Level l) {
    if (src.empty()) {
        des.clear();
        return true;
    }
    size_t bound = GetBound(src.size());
    des.resize(bound); //预留空间
    size_t const actual_size = ZSTD_compress(des.data(), bound, src.data(), src.size(), static_cast<int>(l));
    if (ZSTD_isError(actual_size)) {
        LOGGER_ERROR("Compress Error");
        return false;
    }
    des.resize( actual_size);
    return true;
}
bool Compress::DepressString(const std::string& src, std::string& des) {
    if (src.empty()) {
        des.clear();
        return true;
    }
    unsigned long long const decompressed_size = ZSTD_getFrameContentSize(src.data(), src.size());
    //获取解压的原始数据大小
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        return false;
    }
    des.resize(decompressed_size);
    size_t const actual_size = ZSTD_decompress(des.data(), decompressed_size, src.data(), src.size());
      if (ZSTD_isError(actual_size)) {
        LOGGER_ERROR("Compress Error");
        return false;
    }
    des.resize(actual_size);
    return true;
}
bool Compress::CompressData(const char* src, size_t len, std::vector<char>& des, Level l) {
    if (! src || len == 0) {
        des.clear();
        return true;
    }
    size_t bound = GetBound(len);
    des.resize(bound);
     size_t const actual_size = ZSTD_compress(des.data(), bound, src, len, static_cast<int>(l));
        if (ZSTD_isError(actual_size)) {
        LOGGER_ERROR("Compress Error");
        return false;
    }
    des.resize(actual_size);
    return true;
}
bool Compress:: DepressData (const char* src, size_t len, std::vector<char>& des) {
     unsigned long long const decompressed_size = ZSTD_getFrameContentSize(src, len);
     if ( decompressed_size ==  ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        return false;
     }
     des.resize(decompressed_size);
     size_t const actual_size = ZSTD_decompress(des.data(), decompressed_size, src, len);
      if (ZSTD_isError(actual_size)) {
        LOGGER_ERROR("Compress Error");
        return false;
    }
     des.resize(actual_size);
     return true;
}
} //namespace yan_rpc