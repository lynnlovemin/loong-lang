#ifndef LOONG_LEXER_HPP
#define LOONG_LEXER_HPP

#include "token.hpp"
#include <vector>
#include <string>

namespace loong {

class Lexer {
private:
    std::string source_;
    std::vector<Token> tokens_;
    
    size_t start_;      // 当前token起始位置
    size_t current_;    // 当前字符位置
    int line_;          // 当前行号
    int column_;        // 当前列号
    int tokenColumn_;   // token起始列号

    // 辅助方法
    bool isAtEnd() const { return current_ >= source_.length(); }
    char peek() const { return isAtEnd() ? '\0' : source_[current_]; }
    char peekNext() const;
    char advance();
    bool match(char expected);
    
    void addToken(TokenType type);
    void addToken(TokenType type, const std::string& value);
    
    // 扫描方法
    void scanToken();
    void scanString();
    void scanNumber();
    void scanIdentifier();
    void skipWhitespace();
    void skipComment();

public:
    explicit Lexer(const std::string& source);
    
    std::vector<Token> tokenize();
    std::string getSourceLine(int line) const;
};

} // namespace loong

#endif // LOONG_LEXER_HPP
