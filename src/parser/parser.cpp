#include "parser.hpp"
#include "utils/error.hpp"
#include <sstream>

namespace loong {

Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(tokens), current_(0), hadError_(false) {}

Token Parser::advance() {
    if (!isAtEnd()) {
        current_++;
    }
    return previous();
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    error(peek(), message);
    return peek();
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

void Parser::error(const Token& token, const std::string& message) {
    hadError_ = true;
    std::stringstream ss;
    if (token.type == TokenType::END_OF_FILE) {
        ss << "[ParseError] 文件末尾: " << message;
    } else {
        ss << "[ParseError] 行 " << token.line << ", 列 " << token.column << ": " << message;
    }
    errorMessage_ = ss.str();
    LOONG_PARSE_ERROR(message, token.line, token.column);
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) return;
        
        switch (peek().type) {
            case TokenType::VAL:
            case TokenType::FN:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::FOR:
            case TokenType::RETURN:
            case TokenType::IMPORT:
            case TokenType::TRY:
            case TokenType::THROW:
            case TokenType::CLASS:
                return;
            default:
                break;
        }
        advance();
    }
}

Program Parser::parse() {
    Program program;
    while (!isAtEnd()) {
        try {
            auto stmt = parseStatement();
            if (stmt) {
                program.addStatement(stmt);
            }
        } catch (const LoongError&) {
            synchronize();
        }
    }
    return program;
}

// ==================== 语句解析 ====================

StmtPtr Parser::parseStatement() {
    if (match(TokenType::VAL)) return parseValStmt();
    if (match(TokenType::FN)) return parseFnStmt();
    if (match(TokenType::IF)) return parseIfStmt();
    if (match(TokenType::WHILE)) return parseWhileStmt();
    if (match(TokenType::FOR)) return parseForStmt();
    if (match(TokenType::RETURN)) return parseReturnStmt();
    if (match(TokenType::IMPORT)) return parseImportStmt();
    if (match(TokenType::TRY)) return parseTryStmt();
    if (match(TokenType::THROW)) return parseThrowStmt();
    if (match(TokenType::CLASS)) return parseClassStmt();
    if (match(TokenType::BREAK)) {
        return BreakStmt::create(previous().line, previous().column);
    }
    if (match(TokenType::CONTINUE)) {
        return ContinueStmt::create(previous().line, previous().column);
    }
    
    // 表达式语句
    ExprPtr expr = parseExpression();
    match(TokenType::SEMICOLON); // 分号可选
    return ExprStmt::create(expr, expr->line, expr->column);
}

StmtPtr Parser::parseValStmt() {
    Token name = consume(TokenType::IDENTIFIER, "期望变量名");
    consume(TokenType::EQUAL, "期望 '='");
    ExprPtr initializer = parseExpression();
    match(TokenType::SEMICOLON); // 分号可选
    
    return ValStmt::create(name.value, initializer, name.line, name.column);
}

StmtPtr Parser::parseFnStmt() {
    Token name = consume(TokenType::IDENTIFIER, "期望函数名");
    consume(TokenType::LPAREN, "期望 '('");
    
    std::vector<std::string> params;
    std::vector<std::pair<std::string, ExprPtr>> defaultParams;
    
    if (!check(TokenType::RPAREN)) {
        do {
            Token param = consume(TokenType::IDENTIFIER, "期望参数名");
            
            // 检查是否有默认值
            if (match(TokenType::EQUAL)) {
                ExprPtr defaultValue = parseExpression();
                defaultParams.push_back({param.value, defaultValue});
            } else {
                if (!defaultParams.empty()) {
                    error(param, "有默认值的参数后不能有无默认值的参数");
                }
                params.push_back(param.value);
            }
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RPAREN, "期望 ')'");
    consume(TokenType::LBRACE, "期望 '{'");
    
    std::vector<StmtPtr> body = parseBlock();
    
    return FnStmt::create(name.value, params, defaultParams, body, name.line, name.column);
}

StmtPtr Parser::parseIfStmt() {
    ExprPtr condition = parseExpression();
    consume(TokenType::LBRACE, "期望 '{'");
    std::vector<StmtPtr> thenBranch = parseBlock();
    
    std::vector<StmtPtr> elseBranch;
    
    // 收集所有elif条件和分支
    std::vector<std::pair<ExprPtr, std::vector<StmtPtr>>> elifChain;
    
    while (match(TokenType::ELIF)) {
        ExprPtr elifCond = parseExpression();
        consume(TokenType::LBRACE, "期望 '{'");
        auto elifBody = parseBlock();
        elifChain.push_back({elifCond, elifBody});
    }
    
    // 处理 else
    if (match(TokenType::ELSE)) {
        if (match(TokenType::IF)) {
            // else if
            StmtPtr elseIfStmt = parseIfStmt();
            elseBranch = {elseIfStmt};
        } else {
            consume(TokenType::LBRACE, "期望 '{'");
            elseBranch = parseBlock();
        }
    }
    
    // 从后往前构建elif嵌套链（将elif转换为嵌套的if-else）
    // if A {} elif B {} elif C {} else {} → if A {} else { if B {} else { if C {} else {} } }
    for (int i = static_cast<int>(elifChain.size()) - 1; i >= 0; --i) {
        auto innerIf = IfStmt::create(elifChain[i].first, elifChain[i].second, elseBranch,
                                       elifChain[i].first->line, elifChain[i].first->column);
        elseBranch = {innerIf};
    }
    
    return IfStmt::create(condition, thenBranch, elseBranch, condition->line, condition->column);
}

StmtPtr Parser::parseWhileStmt() {
    ExprPtr condition = parseExpression();
    consume(TokenType::LBRACE, "期望 '{'");
    std::vector<StmtPtr> body = parseBlock();
    
    return WhileStmt::create(condition, body, condition->line, condition->column);
}

StmtPtr Parser::parseForStmt() {
    // 检查是 for-in 还是传统 for
    if (match(TokenType::VAL)) {
        // 可能是 for-in 或传统 for 的变量初始化
        Token varName = consume(TokenType::IDENTIFIER, "期望变量名");
        
        if (match(TokenType::IN)) {
            // for-in 循环
            ExprPtr iterable = parseExpression();
            consume(TokenType::LBRACE, "期望 '{'");
            std::vector<StmtPtr> body = parseBlock();
            
            return ForInStmt::create(varName.value, iterable, body, varName.line, varName.column);
        }
        
        // 传统 for 循环: for val i = init; cond; update { body }
        consume(TokenType::EQUAL, "期望 '='");
        ExprPtr initValue = parseExpression();
        consume(TokenType::SEMICOLON, "期望 ';'");
        
        ExprPtr condition = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            condition = parseExpression();
        }
        consume(TokenType::SEMICOLON, "期望 ';'");
        
        ExprPtr increment = nullptr;
        if (!check(TokenType::LBRACE)) {
            increment = parseExpression();
        }
        
        consume(TokenType::LBRACE, "期望 '{'");
        std::vector<StmtPtr> body = parseBlock();
        
        // 创建变量声明语句
        StmtPtr initializer = ValStmt::create(varName.value, initValue, varName.line, varName.column);
        
        return ForStmt::create(initializer, condition, increment, body, varName.line, varName.column);
    }
    
    // 传统 for 循环（无变量声明）: for expr; cond; update { body }
    StmtPtr initializer = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        ExprPtr expr = parseExpression();
        initializer = ExprStmt::create(expr, expr->line, expr->column);
    }
    consume(TokenType::SEMICOLON, "期望 ';'");
    
    ExprPtr condition = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        condition = parseExpression();
    }
    consume(TokenType::SEMICOLON, "期望 ';'");
    
    ExprPtr increment = nullptr;
    if (!check(TokenType::LBRACE)) {
        increment = parseExpression();
    }
    
    consume(TokenType::LBRACE, "期望 '{'");
    std::vector<StmtPtr> body = parseBlock();
    
    return ForStmt::create(initializer, condition, increment, body, 
                           peek().line, peek().column);
}

StmtPtr Parser::parseReturnStmt() {
    ExprPtr value = nullptr;
    if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE)) {
        value = parseExpression();
    }
    match(TokenType::SEMICOLON);
    
    return ReturnStmt::create(value, previous().line, previous().column);
}

StmtPtr Parser::parseImportStmt() {
    Token moduleName = consume(TokenType::IDENTIFIER, "期望模块名");
    std::string filePath = "";
    
    if (match(TokenType::FROM)) {
        Token path = consume(TokenType::STRING, "期望文件路径");
        filePath = path.value;
    }
    
    match(TokenType::SEMICOLON);
    
    return ImportStmt::create(moduleName.value, filePath, moduleName.line, moduleName.column);
}

std::vector<StmtPtr> Parser::parseBlock() {
    std::vector<StmtPtr> statements;
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        statements.push_back(parseStatement());
    }
    
    consume(TokenType::RBRACE, "期望 '}'");
    return statements;
}

// ==================== 表达式解析 ====================

ExprPtr Parser::parseExpression() {
    return parseAssignment();
}

ExprPtr Parser::parseAssignment() {
    ExprPtr expr = parseOr();
    
    if (match(TokenType::EQUAL)) {
        ExprPtr value = parseAssignment();
        return AssignExpr::create(expr, value, previous().line, previous().column);
    }
    
    return expr;
}

ExprPtr Parser::parseOr() {
    ExprPtr expr = parseAnd();
    
    while (match(TokenType::OR) || match(TokenType::OR_OR)) {
        Token op = previous();
        ExprPtr right = parseAnd();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

ExprPtr Parser::parseAnd() {
    ExprPtr expr = parseBitOr();
    
    while (match(TokenType::AND) || match(TokenType::AND_AND)) {
        Token op = previous();
        ExprPtr right = parseBitOr();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

// 位或 |
ExprPtr Parser::parseBitOr() {
    ExprPtr expr = parseBitXor();
    
    while (match(TokenType::BITOR)) {
        Token op = previous();
        ExprPtr right = parseBitXor();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

// 异或 ^
ExprPtr Parser::parseBitXor() {
    ExprPtr expr = parseBitAnd();
    
    while (match(TokenType::XOR)) {
        Token op = previous();
        ExprPtr right = parseBitAnd();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

// 位与 &
ExprPtr Parser::parseBitAnd() {
    ExprPtr expr = parseEquality();
    
    while (match(TokenType::BITAND)) {
        Token op = previous();
        ExprPtr right = parseEquality();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

ExprPtr Parser::parseEquality() {
    ExprPtr expr = parseComparison();
    
    while (match(TokenType::EQUAL_EQUAL) || match(TokenType::BANG_EQUAL)) {
        Token op = previous();
        ExprPtr right = parseComparison();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

ExprPtr Parser::parseComparison() {
    ExprPtr expr = parseShift();
    
    while (match(TokenType::LESS) || match(TokenType::LESS_EQUAL) ||
           match(TokenType::GREATER) || match(TokenType::GREATER_EQUAL)) {
        Token op = previous();
        ExprPtr right = parseShift();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

// 位移 << >>
ExprPtr Parser::parseShift() {
    ExprPtr expr = parseTerm();
    
    while (match(TokenType::LSHIFT) || match(TokenType::RSHIFT)) {
        Token op = previous();
        ExprPtr right = parseTerm();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

ExprPtr Parser::parseTerm() {
    ExprPtr expr = parseFactor();
    
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        Token op = previous();
        ExprPtr right = parseFactor();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

ExprPtr Parser::parseFactor() {
    ExprPtr expr = parseUnary();
    
    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::PERCENT)) {
        Token op = previous();
        ExprPtr right = parseUnary();
        expr = BinaryExpr::create(expr, op, right);
    }
    
    return expr;
}

ExprPtr Parser::parseUnary() {
    if (match(TokenType::NOT) || match(TokenType::BANG) || 
        match(TokenType::MINUS) || match(TokenType::BITNOT)) {
        Token op = previous();
        ExprPtr right = parseUnary();
        return UnaryExpr::create(op, right);
    }
    
    return parseCall();
}

ExprPtr Parser::parseCall() {
    ExprPtr expr = parsePrimary();
    
    while (true) {
        if (match(TokenType::LPAREN)) {
            std::vector<ExprPtr> args = parseArguments();
            expr = CallExpr::create(expr, args, previous().line, previous().column);
        } else if (match(TokenType::LBRACKET)) {
            ExprPtr index = parseExpression();
            consume(TokenType::RBRACKET, "期望 ']'");
            expr = IndexExpr::create(expr, index, previous().line, previous().column);
        } else if (match(TokenType::DOT)) {
            Token member = consume(TokenType::IDENTIFIER, "期望成员名");
            expr = MemberExpr::create(expr, member.value, member.line, member.column);
        } else {
            break;
        }
    }
    
    return expr;
}

ExprPtr Parser::parsePrimary() {
    if (match(TokenType::NIL)) {
        return LiteralExpr::createNil(previous().line, previous().column);
    }
    if (match(TokenType::TRUE)) {
        return LiteralExpr::createBool(true, previous().line, previous().column);
    }
    if (match(TokenType::FALSE)) {
        return LiteralExpr::createBool(false, previous().line, previous().column);
    }
    if (match(TokenType::NUMBER)) {
        double value = std::stod(previous().value);
        return LiteralExpr::createNumber(value, previous().line, previous().column);
    }
    if (match(TokenType::BIGINT)) {
        return LiteralExpr::createBigint(previous().value, previous().line, previous().column);
    }
    if (match(TokenType::STRING)) {
        return LiteralExpr::createString(previous().value, previous().line, previous().column);
    }
    if (match(TokenType::CHAR)) {
        char ch = previous().value.empty() ? '\0' : previous().value[0];
        return LiteralExpr::createChar(ch, previous().line, previous().column);
    }
    if (match(TokenType::IDENTIFIER)) {
        return IdentifierExpr::create(previous().value, previous().line, previous().column);
    }
    
    // this关键字
    if (match(TokenType::THIS)) {
        return ThisExpr::create(previous().line, previous().column);
    }
    
    // super关键字
    if (match(TokenType::SUPER)) {
        consume(TokenType::DOT, "期望 \'\' 访问父类方法");
        Token method = consume(TokenType::IDENTIFIER, "期望方法名");
        return SuperExpr::create(method.value, previous().line, previous().column);
    }
    
    // 分组表达式
    if (match(TokenType::LPAREN)) {
        ExprPtr expr = parseExpression();
        consume(TokenType::RPAREN, "期望 ')'");
        return GroupedExpr::create(expr, previous().line, previous().column);
    }
    
    // 列表
    if (match(TokenType::LBRACKET)) {
        std::vector<ExprPtr> elements;
        if (!check(TokenType::RBRACKET)) {
            do {
                elements.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RBRACKET, "期望 ']'");
        return ListExpr::create(elements, previous().line, previous().column);
    }
    
    // 字典
    if (match(TokenType::LBRACE)) {
        std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
        if (!check(TokenType::RBRACE)) {
            do {
                ExprPtr key = parseExpression();
                consume(TokenType::COLON, "期望 ':'");
                ExprPtr value = parseExpression();
                pairs.push_back({key, value});
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RBRACE, "期望 '}'");
        return DictExpr::create(pairs, previous().line, previous().column);
    }
    
    error(peek(), "期望表达式");
    return nullptr;
}

std::vector<ExprPtr> Parser::parseArguments() {
    std::vector<ExprPtr> args;
    if (!check(TokenType::RPAREN)) {
        do {
            args.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "期望 \'\'");
    return args;
}

// ==================== 异常处理解析 ====================

StmtPtr Parser::parseTryStmt() {
    Token tryToken = previous();
    
    // 解析try块
    consume(TokenType::LBRACE, "期望 \'\' 开始try块");
    std::vector<StmtPtr> tryBlock = parseBlock();
    
    std::string catchVar = "";
    std::vector<StmtPtr> catchBlock;
    std::vector<StmtPtr> finallyBlock;
    
    // 解析catch块（可选）
    if (match(TokenType::CATCH)) {
        consume(TokenType::LPAREN, "期望 \'\' 开始catch参数");
        Token varToken = consume(TokenType::IDENTIFIER, "期望异常变量名");
        catchVar = varToken.value;
        consume(TokenType::RPAREN, "期望 \'\' 结束catch参数");
        consume(TokenType::LBRACE, "期望 \'\' 开始catch块");
        catchBlock = parseBlock();
    }
    
    // 解析finally块（可选）
    if (match(TokenType::FINALLY)) {
        consume(TokenType::LBRACE, "期望 \'\' 开始finally块");
        finallyBlock = parseBlock();
    }
    
    // 至少需要catch或finally之一
    if (catchBlock.empty() && finallyBlock.empty()) {
        error(tryToken, "try语句需要catch或finally块");
    }
    
    return TryStmt::create(tryBlock, catchVar, catchBlock, finallyBlock, tryToken.line, tryToken.column);
}

StmtPtr Parser::parseThrowStmt() {
    Token throwToken = previous();
    ExprPtr value = parseExpression();
    // 可选的分号
    match(TokenType::SEMICOLON);
    return ThrowStmt::create(value, throwToken.line, throwToken.column);
}

StmtPtr Parser::parseClassStmt() {
    Token classToken = previous();
    
    // 类名
    Token nameToken = consume(TokenType::IDENTIFIER, "期望类名");
    std::string className = nameToken.value;
    
    // 检查继承
    std::string superClass = "";
    if (match(TokenType::COLON)) {
        Token superToken = consume(TokenType::IDENTIFIER, "期望父类名");
        superClass = superToken.value;
    }
    
    // 类体
    consume(TokenType::LBRACE, "期望 \'\' 开始类体");
    std::vector<StmtPtr> body;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // 类体内只能是函数定义
        if (match(TokenType::FN)) {
            body.push_back(parseFnStmt());
        } else {
            error(peek(), "类体内只能定义方法");
            break;
        }
    }
    consume(TokenType::RBRACE, "期望 \'\' 结束类体");
    
    return ClassStmt::create(className, superClass, body, classToken.line, classToken.column);
}

} // namespace loong
