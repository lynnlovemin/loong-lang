#ifndef LOONG_BIGINT_HPP
#define LOONG_BIGINT_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace loong {

// 大整数类 - 使用字符串存储，支持任意精度整数运算
class BigInteger {
private:
    std::string digits;  // 存储数字，digits[0]是最低位
    bool negative;       // 是否为负数
    
    // 移除前导零
    void trim() {
        while (digits.size() > 1 && digits.back() == '0') {
            digits.pop_back();
        }
        if (digits == "0") {
            negative = false;
        }
    }
    
public:
    BigInteger() : digits("0"), negative(false) {}
    
    BigInteger(long long num) : negative(num < 0) {
        if (num < 0) num = -num;
        if (num == 0) {
            digits = "0";
        } else {
            digits = "";
            while (num > 0) {
                digits += char('0' + (num % 10));
                num /= 10;
            }
        }
    }
    
    BigInteger(const std::string& s) : negative(false) {
        int start = 0;
        if (s[0] == '-') {
            negative = true;
            start = 1;
        } else if (s[0] == '+') {
            start = 1;
        }
        
        digits = "";
        for (int i = s.length() - 1; i >= start; i--) {
            if (s[i] >= '0' && s[i] <= '9') {
                digits += s[i];
            }
        }
        
        if (digits.empty()) digits = "0";
        trim();
    }
    
    // 比较绝对值大小
    int compareAbs(const BigInteger& other) const {
        if (digits.size() != other.digits.size()) {
            return digits.size() < other.digits.size() ? -1 : 1;
        }
        for (int i = digits.size() - 1; i >= 0; i--) {
            if (digits[i] != other.digits[i]) {
                return digits[i] < other.digits[i] ? -1 : 1;
            }
        }
        return 0;
    }
    
    // 比较运算
    bool operator<(const BigInteger& other) const {
        if (negative != other.negative) return negative;
        int cmp = compareAbs(other);
        return negative ? cmp > 0 : cmp < 0;
    }
    
    bool operator>(const BigInteger& other) const {
        return other < *this;
    }
    
    bool operator<=(const BigInteger& other) const {
        return !(other < *this);
    }
    
    bool operator>=(const BigInteger& other) const {
        return !(*this < other);
    }
    
    bool operator==(const BigInteger& other) const {
        return negative == other.negative && digits == other.digits;
    }
    
    bool operator!=(const BigInteger& other) const {
        return !(*this == other);
    }
    
    // 加法
    BigInteger operator+(const BigInteger& other) const {
        if (negative != other.negative) {
            if (negative) {
                return other - (-(*this));
            } else {
                return *this - (-other);
            }
        }
        
        BigInteger result;
        result.negative = negative;
        result.digits = "";
        
        int carry = 0;
        size_t maxLen = std::max(digits.size(), other.digits.size());
        
        for (size_t i = 0; i < maxLen || carry; i++) {
            int sum = carry;
            if (i < digits.size()) sum += digits[i] - '0';
            if (i < other.digits.size()) sum += other.digits[i] - '0';
            result.digits += char('0' + (sum % 10));
            carry = sum / 10;
        }
        
        result.trim();
        return result;
    }
    
    // 减法
    BigInteger operator-(const BigInteger& other) const {
        if (negative != other.negative) {
            return *this + (-other);
        }
        
        if (compareAbs(other) < 0) {
            return -(other - *this);
        }
        
        BigInteger result;
        result.negative = negative;
        result.digits = "";
        
        int borrow = 0;
        for (size_t i = 0; i < digits.size(); i++) {
            int diff = digits[i] - '0' - borrow;
            if (i < other.digits.size()) diff -= (other.digits[i] - '0');
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result.digits += char('0' + diff);
        }
        
        result.trim();
        return result;
    }
    
    // 乘法
    BigInteger operator*(const BigInteger& other) const {
        BigInteger result;
        result.negative = negative != other.negative;
        result.digits = std::string(digits.size() + other.digits.size(), '0');
        
        for (size_t i = 0; i < digits.size(); i++) {
            int carry = 0;
            for (size_t j = 0; j < other.digits.size() || carry; j++) {
                int prod = result.digits[i + j] - '0' + carry;
                if (j < other.digits.size()) {
                    prod += (digits[i] - '0') * (other.digits[j] - '0');
                }
                result.digits[i + j] = char('0' + (prod % 10));
                carry = prod / 10;
            }
        }
        
        result.trim();
        return result;
    }
    
    // 除法（整数除法）
    BigInteger operator/(const BigInteger& other) const {
        if (other == BigInteger(0)) {
            throw std::runtime_error("Division by zero");
        }
        
        BigInteger abs_this = *this;
        abs_this.negative = false;
        BigInteger abs_other = other;
        abs_other.negative = false;
        
        if (abs_this < abs_other) {
            return BigInteger(0);
        }
        
        BigInteger result;
        result.negative = negative != other.negative;
        
        BigInteger current;
        std::string resultDigits;
        
        for (int i = digits.size() - 1; i >= 0; i--) {
            current = current * BigInteger(10) + BigInteger(long long(digits[i] - '0'));
            int count = 0;
            while (current >= abs_other) {
                current = current - abs_other;
                count++;
            }
            resultDigits += char('0' + count);
        }
        
        std::reverse(resultDigits.begin(), resultDigits.end());
        result.digits = "";
        for (int i = resultDigits.size() - 1; i >= 0; i--) {
            result.digits += resultDigits[i];
        }
        
        result.trim();
        return result;
    }
    
    // 取模
    BigInteger operator%(const BigInteger& other) const {
        return *this - (*this / other) * other;
    }
    
    // 一元负号
    BigInteger operator-() const {
        BigInteger result = *this;
        if (result != BigInteger(0)) {
            result.negative = !result.negative;
        }
        return result;
    }
    
    // 转换为字符串
    std::string toString() const {
        std::string result = negative ? "-" : "";
        for (int i = digits.size() - 1; i >= 0; i--) {
            result += digits[i];
        }
        return result;
    }
    
    // 转换为double（可能丢失精度）
    double toDouble() const {
        double result = 0;
        double power = 1;
        for (size_t i = 0; i < digits.size(); i++) {
            result += (digits[i] - '0') * power;
            power *= 10;
        }
        return negative ? -result : result;
    }
    
    // 是否为负数
    bool isNegative() const { return negative; }
    
    // 是否为零
    bool isZero() const { return digits == "0"; }
    
    // 是否能精确转换为double
    bool canFitInDouble() const {
        return digits.size() <= 16;  // double约有15-17位有效数字
    }
    
    // 复合赋值运算符
    BigInteger& operator+=(const BigInteger& other) {
        *this = *this + other;
        return *this;
    }
    
    BigInteger& operator-=(const BigInteger& other) {
        *this = *this - other;
        return *this;
    }
    
    BigInteger& operator*=(const BigInteger& other) {
        *this = *this * other;
        return *this;
    }
    
    BigInteger& operator/=(const BigInteger& other) {
        *this = *this / other;
        return *this;
    }
    
    // 输出流
    friend std::ostream& operator<<(std::ostream& os, const BigInteger& num) {
        os << num.toString();
        return os;
    }
};

} // namespace loong

#endif // LOONG_BIGINT_HPP
