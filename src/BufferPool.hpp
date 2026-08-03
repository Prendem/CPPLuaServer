#pragma once
#include <stack>
#include <memory>
#include <mutex>

class BufferPool
{
private:
	std::stack<std::shared_ptr<char[]>> m_freeBuffers;
	std::mutex m_lock;

public:
	BufferPool() = default;
	std::shared_ptr<char[]> obtainPointer();
	void returnPointer(std::shared_ptr<char[]> ptr);
};