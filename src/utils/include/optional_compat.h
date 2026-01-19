#pragma once

// 兼容 C++14 和 C++17 的 optional 支持
// 用于支持旧版编译器（如 GCC 6.3.1）
#if __cplusplus >= 201703L
    // C++17 标准库支持
    #include <optional>
    namespace std_compat {
        using std::optional;
        using std::nullopt;
        using std::nullopt_t;
    }
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
    // GCC 4.9+ 支持 experimental::optional（C++14 TS）
    #include <experimental/optional>
    namespace std_compat {
        using std::experimental::optional;
        using std::experimental::nullopt;
        using std::experimental::nullopt_t;
    }
#elif defined(__clang__) && __clang_major__ >= 3
    // Clang 3.0+ 支持 experimental::optional
    #include <experimental/optional>
    namespace std_compat {
        using std::experimental::optional;
        using std::experimental::nullopt;
        using std::experimental::nullopt_t;
    }
#else
    // 如果都不支持，提供一个简单的实现
    // 注意：这是一个最小实现，可能不完整
    #include <memory>
    #include <stdexcept>
    namespace std_compat {
        struct nullopt_t {
            explicit nullopt_t() = default;
        };
        constexpr nullopt_t nullopt{};
        
        template<typename T>
        class optional {
        private:
            bool has_value_;
            union {
                T value_;
            };
            
        public:
            optional() : has_value_(false) {}
            optional(nullopt_t) : has_value_(false) {}
            optional(const T& v) : has_value_(true), value_(v) {}
            optional(T&& v) : has_value_(true), value_(std::move(v)) {}
            optional(const optional& other) : has_value_(other.has_value_) {
                if (has_value_) {
                    new (&value_) T(other.value_);
                }
            }
            optional(optional&& other) : has_value_(other.has_value_) {
                if (has_value_) {
                    new (&value_) T(std::move(other.value_));
                }
            }
            ~optional() {
                if (has_value_) {
                    value_.~T();
                }
            }
            
            optional& operator=(const optional& other) {
                if (this != &other) {
                    if (has_value_ && other.has_value_) {
                        value_ = other.value_;
                    } else if (has_value_) {
                        value_.~T();
                        has_value_ = false;
                    } else if (other.has_value_) {
                        new (&value_) T(other.value_);
                        has_value_ = true;
                    }
                }
                return *this;
            }
            
            optional& operator=(optional&& other) {
                if (this != &other) {
                    if (has_value_ && other.has_value_) {
                        value_ = std::move(other.value_);
                    } else if (has_value_) {
                        value_.~T();
                        has_value_ = false;
                    } else if (other.has_value_) {
                        new (&value_) T(std::move(other.value_));
                        has_value_ = true;
                    }
                }
                return *this;
            }
            
            bool has_value() const { return has_value_; }
            explicit operator bool() const { return has_value_; }
            
            T& value() {
                if (!has_value_) {
                    throw std::runtime_error("optional has no value");
                }
                return value_;
            }
            
            const T& value() const {
                if (!has_value_) {
                    throw std::runtime_error("optional has no value");
                }
                return value_;
            }
            
            T& operator*() { return value_; }
            const T& operator*() const { return value_; }
            
            T* operator->() { return &value_; }
            const T* operator->() const { return &value_; }
            
            template<typename U>
            T value_or(U&& default_value) const {
                return has_value_ ? value_ : static_cast<T>(std::forward<U>(default_value));
            }
        };
    }
#endif

// 为了兼容性，在 std 命名空间中提供 optional（如果标准库中没有）
#if __cplusplus >= 201703L
    // C++17: std::optional 已经存在，不需要额外操作
#else
    // C++14 或更早：在 std 命名空间中提供 optional 别名
    namespace std {
        using std_compat::optional;
        using std_compat::nullopt;
        using std_compat::nullopt_t;
    }
#endif

