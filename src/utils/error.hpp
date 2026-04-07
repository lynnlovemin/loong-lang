#ifndef LOONG_ERROR_HPP
#define LOONG_ERROR_HPP

#include <string>
#include <stdexcept>

namespace loong {

class LoongError : public std::runtime_error {
public:
    enum class ErrorType {
        LexerError,
        ParseError,
        RuntimeError,
        TypeError,
        NameError,
        IndexError,
        ImportError
    };

private:
    ErrorType type_;
    int line_;
    int column_;
    std::string message_;

public:
    LoongError(ErrorType type, const std::string& message, int line = -1, int column = -1)
        : std::runtime_error(formatMessage(type, message, line, column))
        , type_(type), line_(line), column_(column), message_(message) {}

    ErrorType getType() const { return type_; }
    int getLine() const { return line_; }
    int getColumn() const { return column_; }
    const std::string& getMessage() const { return message_; }

    static std::string errorTypeToString(ErrorType type) {
        switch (type) {
            case ErrorType::LexerError: return "LexerError";
            case ErrorType::ParseError: return "ParseError";
            case ErrorType::RuntimeError: return "RuntimeError";
            case ErrorType::TypeError: return "TypeError";
            case ErrorType::NameError: return "NameError";
            case ErrorType::IndexError: return "IndexError";
            case ErrorType::ImportError: return "ImportError";
            default: return "UnknownError";
        }
    }

private:
    static std::string formatMessage(ErrorType type, const std::string& msg, int line, int col) {
        std::string result = "[" + errorTypeToString(type) + "]";
        if (line >= 0) {
            result += " 行 " + std::to_string(line);
            if (col >= 0) {
                result += ", 列 " + std::to_string(col);
            }
        }
        result += ": " + msg;
        return result;
    }
};

// 便捷宏定义
#define LOONG_LEXER_ERROR(msg, line, col) \
    throw loong::LoongError(loong::LoongError::ErrorType::LexerError, msg, line, col)

#define LOONG_PARSE_ERROR(msg, line, col) \
    throw loong::LoongError(loong::LoongError::ErrorType::ParseError, msg, line, col)

#define LOONG_RUNTIME_ERROR(msg) \
    throw loong::LoongError(loong::LoongError::ErrorType::RuntimeError, msg)

#define LOONG_TYPE_ERROR(msg) \
    throw loong::LoongError(loong::LoongError::ErrorType::TypeError, msg)

#define LOONG_NAME_ERROR(msg) \
    throw loong::LoongError(loong::LoongError::ErrorType::NameError, msg)

#define LOONG_INDEX_ERROR(msg) \
    throw loong::LoongError(loong::LoongError::ErrorType::IndexError, msg)

#define LOONG_IMPORT_ERROR(msg) \
    throw loong::LoongError(loong::LoongError::ErrorType::ImportError, msg)

} // namespace loong

#endif // LOONG_ERROR_HPP
