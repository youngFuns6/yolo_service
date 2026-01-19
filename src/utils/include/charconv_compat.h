#pragma once

// 兼容 C++14 和 C++17 的 charconv 支持
// 用于支持旧版编译器（如 GCC 6.3.1）
#if __cplusplus >= 201703L
    // C++17 标准库支持
    #include <charconv>
    namespace std_compat {
        using std::from_chars;
        using std::to_chars;
        using std::from_chars_result;
        using std::to_chars_result;
        using std::chars_format;
    }
#else
    // C++14 或更早：提供简单的实现
    #include <string>
    #include <cstdlib>
    #include <cerrno>
    #include <limits>
    #include <type_traits>
    #include <system_error>  // 用于 std::errc
    
    namespace std_compat {
        enum class chars_format {
            scientific = 1,
            fixed = 2,
            hex = 4,
            general = fixed | scientific
        };
        
        struct from_chars_result {
            const char* ptr;
            std::errc ec;
            
            friend bool operator==(const from_chars_result& lhs, const from_chars_result& rhs) {
                return lhs.ptr == rhs.ptr && lhs.ec == rhs.ec;
            }
        };
        
        struct to_chars_result {
            char* ptr;
            std::errc ec;
            
            friend bool operator==(const to_chars_result& lhs, const to_chars_result& rhs) {
                return lhs.ptr == rhs.ptr && lhs.ec == rhs.ec;
            }
        };
        
        // 简单的 from_chars 实现（仅支持整数）
        template<typename T>
        typename std::enable_if<std::is_integral<T>::value, from_chars_result>::type
        from_chars(const char* first, const char* last, T& value, int base = 10) {
            from_chars_result result;
            result.ptr = first;
            result.ec = std::errc{};
            
            if (first >= last) {
                result.ec = std::errc::invalid_argument;
                return result;
            }
            
            // 跳过前导空格
            while (first < last && (*first == ' ' || *first == '\t')) {
                first++;
            }
            
            if (first >= last) {
                result.ec = std::errc::invalid_argument;
                return result;
            }
            
            // 使用 strtol/strtoll 进行转换
            char* end_ptr = nullptr;
            errno = 0;
            
            // 使用 SFINAE 和模板特化代替 if constexpr（C++14 兼容）
            if (std::is_same<T, long>::value || std::is_same<T, int>::value || 
                std::is_same<T, short>::value || std::is_same<T, char>::value) {
                long val = std::strtol(first, &end_ptr, base);
                if (errno == ERANGE || end_ptr == first) {
                    result.ec = std::errc::invalid_argument;
                    return result;
                }
                value = static_cast<T>(val);
            } else if (std::is_same<T, long long>::value) {
                long long val = std::strtoll(first, &end_ptr, base);
                if (errno == ERANGE || end_ptr == first) {
                    result.ec = std::errc::invalid_argument;
                    return result;
                }
                value = static_cast<T>(val);
            } else if (std::is_unsigned<T>::value) {
                unsigned long long val = std::strtoull(first, &end_ptr, base);
                if (errno == ERANGE || end_ptr == first) {
                    result.ec = std::errc::invalid_argument;
                    return result;
                }
                value = static_cast<T>(val);
            } else {
                result.ec = std::errc::not_supported;
                return result;
            }
            
            result.ptr = end_ptr;
            return result;
        }
        
        // 简单的 to_chars 实现（仅支持整数）
        template<typename T>
        typename std::enable_if<std::is_integral<T>::value, to_chars_result>::type
        to_chars(char* first, char* last, T value, int base = 10) {
            to_chars_result result;
            result.ptr = first;
            result.ec = std::errc{};
            
            if (first >= last) {
                result.ec = std::errc::value_too_large;
                return result;
            }
            
            // 使用 snprintf 进行转换
            int len = 0;
            if (std::is_signed<T>::value) {
                len = std::snprintf(first, last - first, 
                    base == 16 ? "%llx" : (base == 8 ? "%llo" : "%lld"),
                    static_cast<long long>(value));
            } else {
                len = std::snprintf(first, last - first,
                    base == 16 ? "%llx" : (base == 8 ? "%llo" : "%llu"),
                    static_cast<unsigned long long>(value));
            }
            
            if (len < 0 || len >= (last - first)) {
                result.ec = std::errc::value_too_large;
                return result;
            }
            
            result.ptr = first + len;
            return result;
        }
    }
#endif

// 为了兼容性，在 std 命名空间中提供 charconv（如果标准库中没有）
#if __cplusplus >= 201703L
    // C++17: std::charconv 已经存在，不需要额外操作
#else
    // C++14 或更早：在 std 命名空间中提供 charconv 别名
    namespace std {
        using std_compat::from_chars;
        using std_compat::to_chars;
        using std_compat::from_chars_result;
        using std_compat::to_chars_result;
        using std_compat::chars_format;
    }
#endif

