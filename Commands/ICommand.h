#pragma once

#include <memory>
#include <string>

// 명령 실행 결과
struct CommandResult
{
    bool success;
    std::string message;
    int errorCode;
    
    CommandResult(bool s = false, const std::string& msg = "", int code = 0)
        : success(s), message(msg), errorCode(code) {}
    
    static CommandResult Success(const std::string& msg = "")
    {
        return CommandResult(true, msg, 0);
    }
    
    static CommandResult Failure(const std::string& msg, int code = -1)
    {
        return CommandResult(false, msg, code);
    }
};

// 명령 인터페이스
class ICommand
{
public:
    virtual ~ICommand() = default;
    
    // 명령 실행
    virtual CommandResult Execute() = 0;
    
    // 명령 되돌리기 (선택적 구현)
    virtual CommandResult Undo() { return CommandResult::Failure("Undo not supported"); }
    
    // 명령 정보
    virtual std::string GetCommandName() const = 0;
    virtual std::string GetDescription() const = 0;
    
    // 명령 유효성 검사
    virtual bool IsValid() const = 0;
};

// 명령 스마트 포인터 타입
using CommandPtr = std::shared_ptr<ICommand>;

// 명령 기반 클래스
class CommandBase : public ICommand
{
public:
    CommandBase(const std::string& name, const std::string& description)
        : m_commandName(name), m_description(description) {}
    
    std::string GetCommandName() const override { return m_commandName; }
    std::string GetDescription() const override { return m_description; }
    
protected:
    std::string m_commandName;
    std::string m_description;
};
