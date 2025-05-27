#pragma once

#include <thread>
#include <atomic>

class LoopingThread
{
public:
	~LoopingThread()
	{
		StopLoopingThread();
	}

	template <typename Callable>
	void StartLoopingThread(Callable callable)
	{
		if (!_isStarted)
		{
			_isStarted = true;
			_thread = std::thread([=, this]
				{
					while (_isStarted)
					{
						callable();
				}
		});
	}
}

	void StopLoopingThread()
	{
		if (_isStarted)
		{
			_isStarted = false;
			_thread.join();
		}
	}

	bool IsStarted()
	{
		return _isStarted;
	}

private:
	std::thread _thread;
	std::atomic<bool> _isStarted;
};

