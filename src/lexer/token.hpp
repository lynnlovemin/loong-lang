#ifndef LOONG_TOKEN_HPP
#define LOONG_TOKEN_HPP

#include <string>
#include <unordered_map>
#include <map>

namespace loong {

enum class TokenType {
    // 字面量
    NUMBER,         // 数字 (整数和浮点数)
    BIGINT,         // 大整数
    STRING,         // 字符串
    TRUE,           // true
    FALSE,          // false
    NIL,            // nil

    // 标识符和关键字
    IDENTIFIER,     // 标识符
    VAL,            // val
    FN,             // fn
    IF,             // if
    ELSE,           // else
    ELIF,           // elif
    FOR,            // for
    WHILE,          // while
    IN,             // in
    RETURN,         // return
    BREAK,          // break
    CONTINUE,       // continue
    IMPORT,         // import
    FROM,           // from

    // 异常处理
    TRY,            // try
    CATCH,          // catch
    FINALLY,        // finally
    THROW,          // throw

    // 类和对象
    CLASS,          // class
    THIS,           // this
    SUPER,          // super

    // 运算符
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    
    // 比较运算符
    EQUAL_EQUAL,    // ==
    BANG_EQUAL,     // !=
    LESS,           // <
    LESS_EQUAL,     // <=
    GREATER,        // >
    GREATER_EQUAL,  // >=

    // 逻辑运算符
    AND,            // and
    OR,             // or
    NOT,            // not

    // 赋值
    EQUAL,          // =

    // 分隔符
    LPAREN,         // (
    RPAREN,         // )
    LBRACE,         // {
    RBRACE,         // }
    LBRACKET,       // [
    RBRACKET,       // ]
    COMMA,          // ,
    DOT,            // .
    SEMICOLON,      // ;
    COLON,          // :

    // 特殊
    END_OF_FILE,    // 文件结束
    ERROR           // 错误token
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;

    Token(TokenType t, const std::string& v, int l = 1, int c = 1)
        : type(t), value(v), line(l), column(c) {}

    Token() : type(TokenType::END_OF_FILE), value(""), line(1), column(1) {}

    static std::string typeToString(TokenType type) {
        switch (type) {
            case TokenType::NUMBER: return "NUMBER";
            case TokenType::BIGINT: return "BIGINT";
            case TokenType::STRING: return "STRING";
            case TokenType::TRUE: return "TRUE";
            case TokenType::FALSE: return "FALSE";
            case TokenType::NIL: return "NIL";
            case TokenType::IDENTIFIER: return "IDENTIFIER";
            case TokenType::VAL: return "VAL";
            case TokenType::FN: return "FN";
            case TokenType::IF: return "IF";
            case TokenType::ELSE: return "ELSE";
            case TokenType::ELIF: return "ELIF";
            case TokenType::FOR: return "FOR";
            case TokenType::WHILE: return "WHILE";
            case TokenType::IN: return "IN";
            case TokenType::RETURN: return "RETURN";
            case TokenType::BREAK: return "BREAK";
            case TokenType::CONTINUE: return "CONTINUE";
            case TokenType::IMPORT: return "IMPORT";
            case TokenType::FROM: return "FROM";
            case TokenType::TRY: return "TRY";
            case TokenType::CATCH: return "CATCH";
            case TokenType::FINALLY: return "FINALLY";
            case TokenType::THROW: return "THROW";
            case TokenType::CLASS: return "CLASS";
            case TokenType::THIS: return "THIS";
            case TokenType::SUPER: return "SUPER";
            case TokenType::PLUS: return "PLUS";
            case TokenType::MINUS: return "MINUS";
            case TokenType::STAR: return "STAR";
            case TokenType::SLASH: return "SLASH";
            case TokenType::PERCENT: return "PERCENT";
            case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
            case TokenType::BANG_EQUAL: return "BANG_EQUAL";
            case TokenType::LESS: return "LESS";
            case TokenType::LESS_EQUAL: return "LESS_EQUAL";
            case TokenType::GREATER: return "GREATER";
            case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
            case TokenType::AND: return "AND";
            case TokenType::OR: return "OR";
            case TokenType::NOT: return "NOT";
            case TokenType::EQUAL: return "EQUAL";
            case TokenType::LPAREN: return "LPAREN";
            case TokenType::RPAREN: return "RPAREN";
            case TokenType::LBRACE: return "LBRACE";
            case TokenType::RBRACE: return "RBRACE";
            case TokenType::LBRACKET: return "LBRACKET";
            case TokenType::RBRACKET: return "RBRACKET";
            case TokenType::COMMA: return "COMMA";
            case TokenType::DOT: return "DOT";
            case TokenType::SEMICOLON: return "SEMICOLON";
            case TokenType::COLON: return "COLON";
            case TokenType::END_OF_FILE: return "END_OF_FILE";
            case TokenType::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

// 关键字映射表 - 声明
extern const std::unordered_map<std::string, TokenType> keywords;

} // namespace loong

#endif // LOONG_TOKEN_HPP
