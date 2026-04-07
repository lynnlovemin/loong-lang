#ifndef LOONG_INTERPRETER_HPP
#define LOONG_INTERPRETER_HPP

#include "value.hpp"
#include "environment.hpp"
#include "parser/ast.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <set>

namespace loong {

// 控制流异常
class ReturnException {
public:
    LoongValue value;
    explicit ReturnException(const LoongValue& v) : value(v) {}
};

class BreakException {};
class ContinueException {};

// Loong异常处理
class LoongException {
public:
    LoongValue value;
    int line;
    int column;
    
    LoongException(const LoongValue& v, int l = 0, int c = 0)
        : value(v), line(l), column(c) {}
};

class Interpreter {
private:
    std::shared_ptr<Environment> globals_;
    std::shared_ptr<Environment> environment_;
    std::set<std::string> loadedModules_;
    std::string currentDirectory_;
    std::string currentSourceFile_;  // 当前执行的源文件路径
    std::string executablePath_;     // loong.exe所在目录
    std::vector<std::string> importStack_;  // 导入栈，用于追踪导入链
    
    // 当前实例（用于this关键字）
    LoongValue currentInstance_;
    // 当前类（用于super关键字）
    std::shared_ptr<LoongClass> currentClass_;
    // 当前父类（用于super调用）
    std::shared_ptr<LoongClass> currentSuperClass_;
    
    // 表达式求值
    LoongValue evaluate(ExprPtr expr);
    
    // 语句执行
    void execute(StmtPtr stmt);
    void executeBlock(const std::vector<StmtPtr>& statements, 
                      std::shared_ptr<Environment> environment);
    
    // 具体表达式求值
    LoongValue visitLiteralExpr(LiteralExpr* expr);
    LoongValue visitIdentifierExpr(IdentifierExpr* expr);
    LoongValue visitBinaryExpr(BinaryExpr* expr);
    LoongValue visitUnaryExpr(UnaryExpr* expr);
    LoongValue visitGroupedExpr(GroupedExpr* expr);
    LoongValue visitListExpr(ListExpr* expr);
    LoongValue visitDictExpr(DictExpr* expr);
    LoongValue visitIndexExpr(IndexExpr* expr);
    LoongValue visitMemberExpr(MemberExpr* expr);
    LoongValue visitCallExpr(CallExpr* expr);
    LoongValue visitAssignExpr(AssignExpr* expr);
    LoongValue visitThisExpr(ThisExpr* expr);
    LoongValue visitSuperExpr(SuperExpr* expr);
    
    // 具体语句执行
    void visitExprStmt(ExprStmt* stmt);
    void visitValStmt(ValStmt* stmt);
    void visitFnStmt(FnStmt* stmt);
    void visitReturnStmt(ReturnStmt* stmt);
    void visitIfStmt(IfStmt* stmt);
    void visitWhileStmt(WhileStmt* stmt);
    void visitForStmt(ForStmt* stmt);
    void visitForInStmt(ForInStmt* stmt);
    void visitBreakStmt(BreakStmt* stmt);
    void visitContinueStmt(ContinueStmt* stmt);
    void visitImportStmt(ImportStmt* stmt);
    void visitTryStmt(TryStmt* stmt);
    void visitThrowStmt(ThrowStmt* stmt);
    void visitClassStmt(ClassStmt* stmt);
    
    // 内置函数注册
    void registerBuiltinFunctions();
    
    // 模块加载
    LoongValue loadModule(const std::string& moduleName, const std::string& filePath);
    
    // 模块寻址辅助方法
    std::string resolveModulePath(const std::string& moduleName, const std::string& filePath);
    std::string searchModuleInDirectory(const std::string& dir, const std::string& moduleName);
    std::string getParentDirectory(const std::string& path);
    
public:
    Interpreter();
    
    void interpret(const Program& program);
    std::shared_ptr<Environment> getGlobals() const { return globals_; }
    void setCurrentDirectory(const std::string& dir) { currentDirectory_ = dir; }
    void setSourceFile(const std::string& file) { currentSourceFile_ = file; }
    void setExecutablePath(const std::string& path) { executablePath_ = path; }
};

} // namespace loong

#endif // LOONG_INTERPRETER_HPP
