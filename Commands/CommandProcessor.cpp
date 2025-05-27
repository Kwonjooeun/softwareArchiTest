#include "CommandProcessor.h"
#include <iostream>
#include <algorithm>

CommandProcessor::CommandProcessor()
    : m_maxHistorySize(1000)
    , m_maxUndoStackSize(100)
    , m_running(false)
    , m_paused(false)
    , m_stopRequested(false)
{
}

CommandProcessor::~CommandProcessor()
{
    Stop();
}

void CommandProcessor::EnqueueCommand(CommandPtr command)
{
    if (!command)
    {
        std::cout << "Null command cannot be enqueued" << std::endl;
        return;
    }
    
    if (!command->IsValid())
    {
        std::cout << "Invalid command: " << command->GetCommandName() << std::endl;
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_commandQueue.push(command);
    }
    
    m_queueCondition.notify_one();
    
    std::cout << "Command enqueued: " << command->GetCommandName() << std::endl;
}

void CommandProcessor::EnqueueCommandWithPriority(CommandPtr command)
{
    if (!command)
    {
        std::cout << "Null priority command cannot be enqueued" << std::endl;
        return;
    }
    
    if (!command->IsValid())
    {
        std::cout << "Invalid priority command: " << command->GetCommandName() << std::endl;
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_priorityQueue.push(command);
    }
    
    m_queueCondition.notify_one();
    
    std::cout << "Priority command enqueued: " << command->GetCommandName() << std::endl;
}

void CommandProcessor::Start()
{
    if (m_running.load())
    {
        std::cout << "CommandProcessor is already running" << std::endl;
        return;
    }
    
    m_running.store(true);
    m_stopRequested.store(false);
    m_paused.store(false);
    
    m_processingThread = std::thread(&CommandProcessor::ProcessingLoop, this);
    
    std::cout << "CommandProcessor started" << std::endl;
}

void CommandProcessor::Stop()
{
    if (!m_running.load())
    {
        return;
    }
    
    m_stopRequested.store(true);
    m_queueCondition.notify_all();
    
    if (m_processingThread.joinable())
    {
        m_processingThread.join();
    }
    
    m_running.store(false);
    
    // 남은 명령들 정리
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_commandQueue.empty())
        {
            m_commandQueue.pop();
        }
        while (!m_priorityQueue.empty())
        {
            m_priorityQueue.pop();
        }
    }
    
    std::cout << "CommandProcessor stopped" << std::endl;
}

void CommandProcessor::Pause()
{
    m_paused.store(true);
    std::cout << "CommandProcessor paused" << std::endl;
}

void CommandProcessor::Resume()
{
    m_paused.store(false);
    m_queueCondition.notify_all();
    std::cout << "CommandProcessor resumed" << std::endl;
}

CommandResult CommandProcessor::ExecuteImmediate(CommandPtr command)
{
    if (!command)
    {
        return CommandResult::Failure("Null command");
    }
    
    if (!command->IsValid())
    {
        return CommandResult::Failure("Invalid command: " + command->GetCommandName());
    }
    
    std::cout << "Executing immediate command: " << command->GetCommandName() << std::endl;
    
    CommandResult result = command->Execute();
    AddToHistory(command, result);
    
    // 콜백 호출
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (result.success && m_commandExecutedCallback)
        {
            m_commandExecutedCallback(command, result);
        }
        else if (!result.success && m_commandFailedCallback)
        {
            m_commandFailedCallback(command, result);
        }
    }
    
    return result;
}

std::vector<CommandHistoryItem> CommandProcessor::GetCommandHistory(size_t maxCount) const
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    if (m_history.size() <= maxCount)
    {
        return m_history;
    }
    
    return std::vector<CommandHistoryItem>(
        m_history.end() - maxCount, m_history.end());
}

void CommandProcessor::ClearHistory()
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    m_history.clear();
}

CommandResult CommandProcessor::UndoLastCommand()
{
    std::lock_guard<std::mutex> lock(m_undoRedoMutex);
    
    if (m_undoStack.empty())
    {
        return CommandResult::Failure("No commands to undo");
    }
    
    CommandPtr command = m_undoStack.back();
    m_undoStack.pop_back();
    
    CommandResult result = command->Undo();
    
    if (result.success)
    {
        m_redoStack.push_back(command);
        
        // Redo 스택 크기 제한
        if (m_redoStack.size() > m_maxUndoStackSize)
        {
            m_redoStack.erase(m_redoStack.begin());
        }
    }
    else
    {
        // Undo 실패시 다시 스택에 추가
        m_undoStack.push_back(command);
    }
    
    return result;
}

CommandResult CommandProcessor::RedoCommand()
{
    std::lock_guard<std::mutex> lock(m_undoRedoMutex);
    
    if (m_redoStack.empty())
    {
        return CommandResult::Failure("No commands to redo");
    }
    
    CommandPtr command = m_redoStack.back();
    m_redoStack.pop_back();
    
    CommandResult result = command->Execute();
    
    if (result.success)
    {
        m_undoStack.push_back(command);
        
        // Undo 스택 크기 제한
        if (m_undoStack.size() > m_maxUndoStackSize)
        {
            m_undoStack.erase(m_undoStack.begin());
        }
    }
    else
    {
        // Redo 실패시 다시 스택에 추가
        m_redoStack.push_back(command);
    }
    
    return result;
}

bool CommandProcessor::CanUndo() const
{
    std::lock_guard<std::mutex> lock(m_undoRedoMutex);
    return !m_undoStack.empty();
}

bool CommandProcessor::CanRedo() const
{
    std::lock_guard<std::mutex> lock(m_undoRedoMutex);
    return !m_redoStack.empty();
}

size_t CommandProcessor::GetQueueSize() const
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_commandQueue.size() + m_priorityQueue.size();
}

size_t CommandProcessor::GetHistorySize() const
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    return m_history.size();
}

void CommandProcessor::SetCommandExecutedCallback(std::function<void(CommandPtr, CommandResult)> callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_commandExecutedCallback = callback;
}

void CommandProcessor::SetCommandFailedCallback(std::function<void(CommandPtr, CommandResult)> callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_commandFailedCallback = callback;
}

void CommandProcessor::ProcessingLoop()
{
    while (!m_stopRequested.load())
    {
        CommandPtr command = nullptr;
        
        // 큐에서 명령 가져오기
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            m_queueCondition.wait(lock, [this] {
                return m_stopRequested.load() || 
                       !m_priorityQueue.empty() || 
                       (!m_paused.load() && !m_commandQueue.empty());
            });
            
            if (m_stopRequested.load())
            {
                break;
            }
            
            // 우선순위 명령 먼저 처리
            if (!m_priorityQueue.empty())
            {
                command = m_priorityQueue.front();
                m_priorityQueue.pop();
            }
            else if (!m_paused.load() && !m_commandQueue.empty())
            {
                command = m_commandQueue.front();
                m_commandQueue.pop();
            }
        }
        
        if (command)
        {
            ExecuteCommand(command);
        }
    }
}

void CommandProcessor::ExecuteCommand(CommandPtr command)
{
    std::cout << "Executing command: " << command->GetCommandName() << std::endl;
    
    CommandResult result = command->Execute();
    AddToHistory(command, result);
    
    // Undo 스택에 추가 (성공한 명령만)
    if (result.success)
    {
        std::lock_guard<std::mutex> lock(m_undoRedoMutex);
        m_undoStack.push_back(command);
        
        // Undo 스택 크기 제한
        if (m_undoStack.size() > m_maxUndoStackSize)
        {
            m_undoStack.erase(m_undoStack.begin());
        }
        
        // 새 명령 실행시 Redo 스택 클리어
        m_redoStack.clear();
    }
    
    // 콜백 호출
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (result.success && m_commandExecutedCallback)
        {
            m_commandExecutedCallback(command, result);
        }
        else if (!result.success && m_commandFailedCallback)
        {
            m_commandFailedCallback(command, result);
        }
    }
    
    if (result.success)
    {
        std::cout << "Command executed successfully: " << command->GetCommandName() << std::endl;
    }
    else
    {
        std::cout << "Command failed: " << command->GetCommandName() 
                  << " - " << result.message << std::endl;
    }
}

void CommandProcessor::AddToHistory(CommandPtr command, CommandResult result)
{
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    m_history.emplace_back(command, result);
    
    // 히스토리 크기 제한
    if (m_history.size() > m_maxHistorySize)
    {
        m_history.erase(m_history.begin());
    }
}
