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
            case TokenType::SWITCH:
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
    if (match(TokenType::SWITCH)) {
        return parseSwitchStmt();
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
    std::string fullModuleName = moduleName.value;
    
    // 支持点分模块名，如 dragonfire.drivers.driver
    while (match(TokenType::DOT)) {
        Token nextPart = consume(TokenType::IDENTIFIER, "期望模块名");
        fullModuleName += "." + nextPart.value;
    }
    
    std::string filePath = "";
    
    if (match(TokenType::FROM)) {
        Token path = consume(TokenType::STRING, "期望文件路径");
        filePath = path.value;
    }
    
    match(TokenType::SEMICOLON);
    
    return ImportStmt::create(fullModuleName, filePath, moduleName.line, moduleName.column);
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
    ExprPtr expr = parseTernary();
    
    if (match(TokenType::EQUAL)) {
        ExprPtr value = parseAssignment();
        return AssignExpr::create(expr, value, previous().line, previous().column);
    }
    
    return expr;
}

ExprPtr Parser::parseTernary() {
    ExprPtr expr = parseOr();
    
    if (match(TokenType::QUESTION)) {
        Token questionToken = previous();
        ExprPtr thenExpr = parseOr();
        consume(TokenType::COLON, "期望 ':' 在三元运算符中");
        ExprPtr elseExpr = parseTernary();
        return TernaryExpr::create(expr, thenExpr, elseExpr, questionToken.line, questionToken.column);
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
    // 匿名函数: fn(params) { body }
    if (check(TokenType::FN) && !isAtEnd()) {
        return parseAnonymousFn();
    }
    
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
        // 检查是否是单参数省略括号的 Lambda: x -> body
        if (match(TokenType::ARROW)) {
            Token ident = tokens_[current_ - 2];  // 获取标识符 token
            std::vector<std::string> params = {ident.value};
            std::vector<std::pair<std::string, ExprPtr>> defaultParams;
            return parseLambdaBody(params, defaultParams, ident.line, ident.column);
        }
        return IdentifierExpr::create(previous().value, previous().line, previous().column);
    }

    // this关键字
    if (match(TokenType::THIS)) {
        return ThisExpr::create(previous().line, previous().column);
    }

    // super关键字
    if (match(TokenType::SUPER)) {
        consume(TokenType::DOT, "期望 \'.\' 访问父类方法");
        Token method = consume(TokenType::IDENTIFIER, "期望方法名");
        return SuperExpr::create(method.value, previous().line, previous().column);
    }

    // 分组表达式 或 Lambda: (expr) 或 (params) -> body
    if (match(TokenType::LPAREN)) {
        return parseParenOrLambda();
    }

    // 列表
    if (match(TokenType::LBRACKET)) {
        std::vector<ExprPtr> elements;
        if (!check(TokenType::RBRACKET)) {
            do {
                elements.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RBRACKET, "期望 \']\'");
        return ListExpr::create(elements, previous().line, previous().column);
    }

    // 字典
    if (match(TokenType::LBRACE)) {
        std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
        if (!check(TokenType::RBRACE)) {
            do {
                ExprPtr key = parseExpression();
                consume(TokenType::COLON, "期望 \':\'");
                ExprPtr value = parseExpression();
                pairs.push_back({key, value});
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RBRACE, "期望 \'}\'");
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
    consume(TokenType::LBRACE, "期望 '{' 开始类体");
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
    consume(TokenType::RBRACE, "期望 \' }\' 结束类体");
    
    return ClassStmt::create(className, superClass, body, classToken.line, classToken.column);
}

// ==================== Lambda 和匿名函数解析 ====================

// 解析括号表达式或 Lambda
// (expr) 或 (params) -> body 或 () -> body
ExprPtr Parser::parseParenOrLambda() {
    int line = previous().line;
    int column = previous().column;
    
    // 检查是否是空参数列表的 Lambda: () -> body
    if (match(TokenType::RPAREN)) {
        // 必须跟着 -> 才是 Lambda
        if (match(TokenType::ARROW)) {
            return parseLambdaBody({}, {}, line, column);
        }
        // 否则是错误
        error(previous(), "期望表达式或 '->'");
        return nullptr;
    }
    
    // 检查是否可能是 Lambda 参数列表
    // Lambda 参数列表的特征：
    // 1. 全是标识符（可能带默认值）
    // 2. 后面跟着 ) ->
    // 我们需要前瞻判断
    
    // 保存当前位置用于回溯
    size_t savedPos = current_;
    bool isLambda = false;
    std::vector<std::string> params;
    std::vector<std::pair<std::string, ExprPtr>> defaultParams;
    
    // 尝试解析为参数列表
    if (check(TokenType::IDENTIFIER)) {
        // 可能是单参数 Lambda（无括号参数）或表达式
        // 继续解析看看
        Token first = advance();
        
        if (match(TokenType::RPAREN)) {
            // (identifier)
            if (match(TokenType::ARROW)) {
                // (identifier) -> body : 单参数 Lambda
                params.push_back(first.value);
                return parseLambdaBody(params, defaultParams, line, column);
            }
            // 否则是分组表达式 (identifier)，需要回溯
            current_ = savedPos;
            ExprPtr expr = parseExpression();
            consume(TokenType::RPAREN, "期望 ')'");
            return GroupedExpr::create(expr, line, column);
        }
        
        if (match(TokenType::COMMA)) {
            // (identifier, ... 可能是多参数 Lambda
            params.push_back(first.value);
            
            // 解析剩余参数
            do {
                Token param = consume(TokenType::IDENTIFIER, "期望参数名");
                params.push_back(param.value);
            } while (match(TokenType::COMMA));
            
            consume(TokenType::RPAREN, "期望 ')'");
            
            if (match(TokenType::ARROW)) {
                return parseLambdaBody(params, defaultParams, line, column);
            }
            
            // 不是 Lambda，报错
            error(previous(), "期望 '->'");
            return nullptr;
        }
        
        if (match(TokenType::EQUAL)) {
            // (identifier = defaultValue, ... 带默认值的 Lambda
            ExprPtr defaultValue = parseExpression();
            defaultParams.push_back({first.value, defaultValue});
            
            while (match(TokenType::COMMA)) {
                Token param = consume(TokenType::IDENTIFIER, "期望参数名");
                if (match(TokenType::EQUAL)) {
                    ExprPtr defVal = parseExpression();
                    defaultParams.push_back({param.value, defVal});
                } else {
                    error(param, "有默认值的参数后不能有无默认值的参数");
                }
            }
            
            consume(TokenType::RPAREN, "期望 ')'");
            
            if (match(TokenType::ARROW)) {
                return parseLambdaBody(params, defaultParams, line, column);
            }
            
            error(previous(), "期望 '->'");
            return nullptr;
        }
        
        // 其他情况，回溯并解析为分组表达式
        current_ = savedPos;
        ExprPtr expr = parseExpression();
        consume(TokenType::RPAREN, "期望 ')'");
        return GroupedExpr::create(expr, line, column);
    }
    
    // 不是以标识符开头，解析为分组表达式
    ExprPtr expr = parseExpression();
    consume(TokenType::RPAREN, "期望 ')'");
    return GroupedExpr::create(expr, line, column);
}

// 解析 Lambda 函数体
ExprPtr Parser::parseLambdaBody(
    const std::vector<std::string>& params,
    const std::vector<std::pair<std::string, ExprPtr>>& defaultParams,
    int line, int column) {
    
    // 检查是否是块体 { ... } 还是表达式体
    if (match(TokenType::LBRACE)) {
        // 块体 Lambda
        std::vector<StmtPtr> body = parseBlock();
        return LambdaExpr::createBlock(params, defaultParams, body, line, column);
    } else {
        // 表达式体 Lambda（自动返回）
        ExprPtr expr = parseExpression();
        return LambdaExpr::createExpression(params, defaultParams, expr, line, column);
    }
}

// 解析匿名函数: fn(params) { body }
ExprPtr Parser::parseAnonymousFn() {
    Token fnToken = consume(TokenType::FN, "期望 'fn'");
    int line = fnToken.line;
    int column = fnToken.column;
    
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
    consume(TokenType::LBRACE, "期望 '{' 开始函数体");
    
    std::vector<StmtPtr> body = parseBlock();
    
    return LambdaExpr::createBlock(params, defaultParams, body, line, column);
}

// 解析 Lambda 参数列表
std::pair<std::vector<std::string>, std::vector<std::pair<std::string, ExprPtr>>> 
Parser::parseLambdaParams() {
    std::vector<std::string> params;
    std::vector<std::pair<std::string, ExprPtr>> defaultParams;
    
    if (!check(TokenType::RPAREN)) {
        do {
            Token param = consume(TokenType::IDENTIFIER, "期望参数名");
            
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
    
    return {params, defaultParams};
}

// 解析 Lambda 表达式（用于单参数省略括号的情况）
// x -> body
ExprPtr Parser::parseLambdaExpr() {
    int line = peek().line;
    int column = peek().column;
    
    // 解析参数
    Token param = consume(TokenType::IDENTIFIER, "期望参数名");
    std::vector<std::string> params = {param.value};
    std::vector<std::pair<std::string, ExprPtr>> defaultParams;
    
    // 期望 ->
    consume(TokenType::ARROW, "期望 '->'");
    
    return parseLambdaBody(params, defaultParams, line, column);
}

// ==================== Switch/Case解析 ====================

StmtPtr Parser::parseSwitchStmt() {
    Token switchToken = previous();
    
    // 解析switch表达式
    ExprPtr expression = parseExpression();
    consume(TokenType::LBRACE, "期望 '{' 开始switch块");
    
    std::vector<CaseClause> cases;
    bool hasDefault = false;
    
    // 解析case和default分支
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        if (match(TokenType::CASE)) {
            // 收集连续的case值（支持多case共享同一代码块）
            std::vector<ExprPtr> caseValues;
            
            while (true) {
                // 解析case值
                ExprPtr caseValue = parseExpression();
                caseValues.push_back(caseValue);
                
                // 消耗冒号
                consume(TokenType::COLON, "期望 ':' 开始case体");
                
                // 检查是否有下一个case（fall-through）
                if (check(TokenType::CASE)) {
                    advance(); // 消耗CASE关键字
                    // 继续收集下一个case值
                } else {
                    // 没有下一个case，退出循环
                    break;
                }
            }
            
            // 解析共享的case体
            std::vector<StmtPtr> caseBody;
            while (!check(TokenType::CASE) && !check(TokenType::DEFAULT) && !check(TokenType::RBRACE) && !isAtEnd()) {
                caseBody.push_back(parseStatement());
            }
            
            // 为每个case值创建一个CaseClause，共享同一个body
            for (const auto& value : caseValues) {
                cases.push_back(CaseClause::create(value, caseBody, false, value->line, value->column));
            }
        } else if (match(TokenType::DEFAULT)) {
            if (hasDefault) {
                error(previous(), "switch语句只能有一个default分支");
            }
            hasDefault = true;
            consume(TokenType::COLON, "期望 ':' 开始default体");
            
            // 解析default体
            std::vector<StmtPtr> defaultBody;
            while (!check(TokenType::RBRACE) && !isAtEnd()) {
                defaultBody.push_back(parseStatement());
            }
            
            cases.push_back(CaseClause::create(nullptr, defaultBody, true, previous().line, previous().column));
        } else {
            error(peek(), "switch块内只能包含case或default分支");
            break;
        }
    }
    
    consume(TokenType::RBRACE, "期望 '}' 结束switch块");
    
    return SwitchStmt::create(expression, cases, switchToken.line, switchToken.column);
}

} // namespace loong
