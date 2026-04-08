#include "lexer.hpp"
#include "utils/error.hpp"
#include <cctype>
#include <sstream>

namespace loong {

// 关键字映射表 - 定义
const std::unordered_map<std::string, TokenType> keywords = {
    {"val", TokenType::VAL},
    {"fn", TokenType::FN},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"elif", TokenType::ELIF},
    {"for", TokenType::FOR},
    {"while", TokenType::WHILE},
    {"in", TokenType::IN},
    {"return", TokenType::RETURN},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"import", TokenType::IMPORT},
    {"from", TokenType::FROM},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"nil", TokenType::NIL},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"finally", TokenType::FINALLY},
    {"throw", TokenType::THROW},
    {"class", TokenType::CLASS},
    {"this", TokenType::THIS},
    {"self", TokenType::THIS},  // self 作为 this 的别名
    {"super", TokenType::SUPER}
};

Lexer::Lexer(const std::string& source)
    : source_(source), start_(0), current_(0), line_(1), column_(1), tokenColumn_(1) {}

char Lexer::peekNext() const {
    if (current_ + 1 >= source_.length()) {
        return '\0';
    }
    return source_[current_ + 1];
}

char Lexer::advance() {
    char c = source_[current_];
    current_++;
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[current_] != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::addToken(TokenType type) {
    std::string value = source_.substr(start_, current_ - start_);
    tokens_.emplace_back(type, value, line_, tokenColumn_);
}

void Lexer::addToken(TokenType type, const std::string& value) {
    tokens_.emplace_back(type, value, line_, tokenColumn_);
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance();
                break;
            case '/':
                // 检查是否是注释
                if (peekNext() == '/' || peekNext() == '*') {
                    skipComment();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

void Lexer::skipComment() {
    advance(); // 消耗第一个 '/'
    if (peek() == '/') {
        // 单行注释
        while (!isAtEnd() && peek() != '\n') {
            advance();
        }
    } else if (peek() == '*') {
        // 多行注释
        advance(); // 消耗 '*'
        while (!isAtEnd()) {
            if (peek() == '*' && peekNext() == '/') {
                advance(); // 消耗 '*'
                advance(); // 消耗 '/'
                break;
            }
            advance();
        }
    }
}

void Lexer::scanString() {
    // 支持多行字符串
    std::string value;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (!isAtEnd()) {
                char escaped = peek();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"': value += '"'; break;
                    default: value += escaped; break;
                }
                advance();
            }
        } else {
            value += advance();
        }
    }

    if (isAtEnd()) {
        LOONG_LEXER_ERROR("未闭合的字符串", line_, tokenColumn_);
    }

    advance(); // 消耗闭合的 "
    addToken(TokenType::STRING, value);
}

void Lexer::scanNumber() {
    while (!isAtEnd() && std::isdigit(peek())) {
        advance();
    }

    // 保存整数部分的起始位置
    size_t intPartEnd = current_;
    
    // 处理小数部分
    if (peek() == '.' && std::isdigit(peekNext())) {
        advance(); // 消耗 '.'
        while (!isAtEnd() && std::isdigit(peek())) {
            advance();
        }
        addToken(TokenType::NUMBER);
    } else {
        // 纯整数，检查是否需要作为大整数处理
        std::string numStr = source_.substr(start_, intPartEnd - start_);
        
        // 如果数字超过15位，使用 BIGINT
        if (numStr.length() > 15) {
            addToken(TokenType::BIGINT, numStr);
        } else {
            addToken(TokenType::NUMBER);
        }
    }
}

void Lexer::scanIdentifier() {
    while (!isAtEnd()) {
        char c = peek();
        if (std::isalnum(c) || c == '_') {
            advance();
        } else {
            break;
        }
    }

    std::string text = source_.substr(start_, current_ - start_);
    
    // 检查是否是关键字
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        addToken(it->second);
    } else {
        addToken(TokenType::IDENTIFIER);
    }
}

void Lexer::scanToken() {
    tokenColumn_ = column_;
    char c = advance();

    switch (c) {
        // 单字符token
        case '(': addToken(TokenType::LPAREN); break;
        case ')': addToken(TokenType::RPAREN); break;
        case '{': addToken(TokenType::LBRACE); break;
        case '}': addToken(TokenType::RBRACE); break;
        case '[': addToken(TokenType::LBRACKET); break;
        case ']': addToken(TokenType::RBRACKET); break;
        case ',': addToken(TokenType::COMMA); break;
        case '.': addToken(TokenType::DOT); break;
        case ';': addToken(TokenType::SEMICOLON); break;
        case ':': addToken(TokenType::COLON); break;
        
        // 运算符
        case '+': addToken(TokenType::PLUS); break;
        case '-': addToken(TokenType::MINUS); break;
        case '*': addToken(TokenType::STAR); break;
        case '/': addToken(TokenType::SLASH); break;
        case '%': addToken(TokenType::PERCENT); break;
        case '^': addToken(TokenType::XOR); break;
        case '~': addToken(TokenType::BITNOT); break;
        
        // 可能是多字符的运算符
        case '=':
            if (match('=')) {
                addToken(TokenType::EQUAL_EQUAL);
            } else {
                addToken(TokenType::EQUAL);
            }
            break;
        case '!':
            if (match('=')) {
                addToken(TokenType::BANG_EQUAL);
            } else {
                addToken(TokenType::BANG);  // 逻辑非
            }
            break;
        case '<':
            if (match('=')) {
                addToken(TokenType::LESS_EQUAL);
            } else if (match('<')) {
                addToken(TokenType::LSHIFT);  // 左移
            } else {
                addToken(TokenType::LESS);
            }
            break;
        case '>':
            if (match('=')) {
                addToken(TokenType::GREATER_EQUAL);
            } else if (match('>')) {
                addToken(TokenType::RSHIFT);  // 右移
            } else {
                addToken(TokenType::GREATER);
            }
            break;
        case '&':
            if (match('&')) {
                addToken(TokenType::AND_AND);  // 逻辑与
            } else {
                addToken(TokenType::BITAND);  // 位与
            }
            break;
        case '|':
            if (match('|')) {
                addToken(TokenType::OR_OR);  // 逻辑或
            } else {
                addToken(TokenType::BITOR);  // 位或
            }
            break;
        
        // 字符串
        case '"': scanString(); break;
        
        // 空白字符处理（跳过）
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            break;
        
        default:
            if (std::isdigit(c)) {
                scanNumber();
            } else if (std::isalpha(c) || c == '_') {
                scanIdentifier();
            } else {
                std::string msg = "意外的字符 '";
                msg += c;
                msg += "'";
                LOONG_LEXER_ERROR(msg, line_, tokenColumn_);
            }
            break;
    }
}

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        start_ = current_;
        skipWhitespace();
        if (!isAtEnd()) {
            tokenColumn_ = column_;
            start_ = current_;
            scanToken();
        }
    }
    
    tokens_.emplace_back(TokenType::END_OF_FILE, "", line_, column_);
    return tokens_;
}

std::string Lexer::getSourceLine(int line) const {
    std::istringstream iss(source_);
    std::string result;
    int currentLine = 1;
    
    while (std::getline(iss, result)) {
        if (currentLine == line) {
            return result;
        }
        currentLine++;
    }
    return "";
}

} // namespace loong
