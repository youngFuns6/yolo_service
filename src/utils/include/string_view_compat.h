#pragma once

// 兼容 C++14 和 C++17 的 string_view 支持
// 用于支持旧版编译器（如 GCC 6.3.1）
#if __cplusplus >= 201703L
    // C++17 标准库支持
    #include <string_view>
    namespace std_compat {
        using std::string_view;
        using std::basic_string_view;
    }
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
    // GCC 4.9+ 支持 experimental::string_view（C++14 TS）
    #include <experimental/string_view>
    namespace std_compat {
        using std::experimental::string_view;
        using std::experimental::basic_string_view;
    }
#elif defined(__clang__) && __clang_major__ >= 3
    // Clang 3.0+ 支持 experimental::string_view
    #include <experimental/string_view>
    namespace std_compat {
        using std::experimental::string_view;
        using std::experimental::basic_string_view;
    }
#else
    // 如果都不支持，提供一个简单的实现
    // 注意：这是一个最小实现，可能不完整
    #include <string>
    #include <cstring>
    namespace std_compat {
        class string_view {
        private:
            const char* data_;
            size_t size_;
            
        public:
            string_view() : data_(nullptr), size_(0) {}
            string_view(const char* str) : data_(str), size_(str ? std::strlen(str) : 0) {}
            string_view(const std::string& str) : data_(str.data()), size_(str.size()) {}
            string_view(const char* data, size_t size) : data_(data), size_(size) {}
            
            const char* data() const { return data_; }
            size_t size() const { return size_; }
            bool empty() const { return size_ == 0; }
            
            const char* begin() const { return data_; }
            const char* end() const { return data_ + size_; }
            
            const char& operator[](size_t pos) const { return data_[pos]; }
            
            std::string to_string() const {
                return std::string(data_, size_);
            }
            
            operator std::string() const {
                return to_string();
            }
        };
        
        template<typename CharT>
        using basic_string_view = string_view;  // 简化实现
    }
#endif

// 为了兼容性，在 std 命名空间中提供 string_view（如果标准库中没有）
#if __cplusplus >= 201703L
    // C++17: std::string_view 已经存在，不需要额外操作
#else
    // C++14 或更早：在 std 命名空间中提供 string_view 别名
    namespace std {
        using std_compat::string_view;
        using std_compat::basic_string_view;
    }
#endif

