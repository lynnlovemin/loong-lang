#ifndef LOONG_PARSER_HPP
#define LOONG_PARSER_HPP

#include "ast.hpp"
#include "lexer/token.hpp"
#include "lexer/lexer.hpp"
#include <vector>
#include <memory>

namespace loong {

class Parser {
private:
    std::vector<Token> tokens_;
    size_t current_;
    
    // 错误处理
    bool hadError_;
    std::string errorMessage_;
    
    // 辅助方法
    Token peek() const { return tokens_[current_]; }
    Token previous() const { return tokens_[current_ - 1]; }
    bool isAtEnd() const { return peek().type == TokenType::END_OF_FILE; }
    Token advance();
    Token consume(TokenType type, const std::string& message);
    bool check(TokenType type) const;
    bool match(TokenType type);
    
    void error(const Token& token, const std::string& message);
    void synchronize();
    
    // 语句解析
    StmtPtr parseStatement();
    StmtPtr parseValStmt();
    StmtPtr parseFnStmt();
    StmtPtr parseIfStmt();
    StmtPtr parseWhileStmt();
    StmtPtr parseForStmt();
    StmtPtr parseReturnStmt();
    StmtPtr parseImportStmt();
    StmtPtr parseTryStmt();
    StmtPtr parseThrowStmt();
    StmtPtr parseClassStmt();
    StmtPtr parseSwitchStmt();
    
    // 表达式解析
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseBitOr();
    ExprPtr parseBitXor();
    ExprPtr parseBitAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseShift();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parseUnary();
    ExprPtr parseCall();
    ExprPtr parsePrimary();
    
    // Lambda/匿名函数解析
    ExprPtr parseLambdaExpr();
    ExprPtr parseAnonymousFn();
    ExprPtr parseParenOrLambda();  // 解析括号表达式或 Lambda
    ExprPtr parseLambdaBody(const std::vector<std::string>& params,
                           const std::vector<std::pair<std::string, ExprPtr>>& defaultParams,
                           int line, int column);
    std::pair<std::vector<std::string>, std::vector<std::pair<std::string, ExprPtr>>> parseLambdaParams();
    
    // 辅助解析
    std::vector<StmtPtr> parseBlock();
    std::vector<ExprPtr> parseArguments();
    
public:
    explicit Parser(const std::vector<Token>& tokens);
    
    Program parse();
    bool hadError() const { return hadError_; }
    const std::string& getErrorMessage() const { return errorMessage_; }
};

} // namespace loong

#endif // LOONG_PARSER_HPP
