#pragma once

#include "ICommand.h"
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>

// 명령 실행 히스토리 항목
struct CommandHistoryItem
{
    CommandPtr command;
    CommandResult result;
    std::chrono::steady_clock::time_point executionTime;
    
    CommandHistoryItem(CommandPtr cmd, CommandResult res)
        : command(cmd), result(res), executionTime(std::chrono::steady_clock::now()) {}
};

// 명령 처리기
class CommandProcessor
{
public:
    CommandProcessor();
    ~CommandProcessor();
    
    // 명령 큐 관리
    void EnqueueCommand(CommandPtr command);
    void EnqueueCommandWithPriority(CommandPtr command); // 우선순위 명령 (비상정지 등)
    
    // 명령 처리 제어
    void Start();
    void Stop();
    void Pause();
    void Resume();
    bool IsRunning() const { return m_running.load(); }
    bool IsPaused() const { return m_paused.load(); }
    
    // 즉시 실행 (동기식)
    CommandResult ExecuteImmediate(CommandPtr command);
    
    // 히스토리 관리
    std::vector<CommandHistoryItem> GetCommandHistory(size_t maxCount = 100) const;
    void ClearHistory();
    
    // Undo/Redo 기능
    CommandResult UndoLastCommand();
    CommandResult RedoCommand();
    bool CanUndo() const;
    bool CanRedo() const;
    
    // 상태 정보
    size_t GetQueueSize() const;
    size_t GetHistorySize() const;
    
    // 콜백 등록
    void SetCommandExecutedCallback(std::function<void(CommandPtr, CommandResult)> callback);
    void SetCommandFailedCallback(std::function<void(CommandPtr, CommandResult)> callback);
    
private:
    void ProcessingLoop();
    void ExecuteCommand(CommandPtr command);
    void AddToHistory(CommandPtr command, CommandResult result);
    
    // 명령 큐
    std::queue<CommandPtr> m_commandQueue;
    std::queue<CommandPtr> m_priorityQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    
    // 실행 히스토리
    std::vector<CommandHistoryItem> m_history;
    mutable std::mutex m_historyMutex;
    size_t m_maxHistorySize;
    
    // Undo/Redo 스택
    std::vector<CommandPtr> m_undoStack;
    std::vector<CommandPtr> m_redoStack;
    mutable std::mutex m_undoRedoMutex;
    size_t m_maxUndoStackSize;
    
    // 스레드 관리
    std::thread m_processingThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    std::atomic<bool> m_stopRequested;
    
    // 콜백
    std::function<void(CommandPtr, CommandResult)> m_commandExecutedCallback;
    std::function<void(CommandPtr, CommandResult)> m_commandFailedCallback;
    mutable std::mutex m_callbackMutex;
};
