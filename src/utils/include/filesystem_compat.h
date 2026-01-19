#pragma once

// 兼容 C++14 和 C++17 的 filesystem 支持
// 用于支持旧版编译器（如 GCC 6.3.1）
#if __cplusplus >= 201703L
    // C++17 标准库支持
    #include <filesystem>
    namespace std_compat {
        namespace fs = std::filesystem;
    }
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
    // GCC 4.9+ 支持 experimental::filesystem（C++14 TS）
    #include <experimental/filesystem>
    namespace std_compat {
        namespace fs = std::experimental::filesystem;
    }
#elif defined(__clang__) && __clang_major__ >= 3
    // Clang 3.0+ 支持 experimental::filesystem
    #include <experimental/filesystem>
    namespace std_compat {
        namespace fs = std::experimental::filesystem;
    }
#else
    // 如果都不支持，提供一个简单的实现
    // 注意：这是一个最小实现，可能不完整
    #include <string>
    #include <fstream>
    namespace std_compat {
        namespace fs {
            class path {
            private:
                std::string path_str_;
            public:
                path() {}
                path(const std::string& p) : path_str_(p) {}
                path(const char* p) : path_str_(p ? p : "") {}
                
                std::string string() const { return path_str_; }
                const char* c_str() const { return path_str_.c_str(); }
                
                path operator/(const path& other) const {
                    if (path_str_.empty()) return other;
                    if (other.path_str_.empty()) return *this;
                    return path(path_str_ + "/" + other.path_str_);
                }
                
                bool exists() const {
                    std::ifstream f(path_str_);
                    return f.good();
                }
            };
        }
    }
#endif

// 为了兼容性，在 std 命名空间中提供 filesystem（如果标准库中没有）
#if __cplusplus >= 201703L
    // C++17: std::filesystem 已经存在，不需要额外操作
#else
    // C++14 或更早：在 std 命名空间中提供 filesystem 别名
    namespace std {
        namespace filesystem = std_compat::fs;
    }
#endif

