#include "BufferPool.hpp"
#include "ConfigManager.hpp"

std::shared_ptr<char[]> BufferPool::obtainPointer()
{
    std::lock_guard<std::mutex> lock(m_lock);
    if (m_freeBuffers.empty())
    {
        try
        {
            return std::make_shared<char[]>(ConfigManager::instance().getRequestBufferSize());
        }

        catch(const std::bad_alloc&)
        {
            std::cerr << "unable to allocate buffer, consider using smaller request buffer size\n";
            return nullptr;
        }
    }

    auto ptr{ m_freeBuffers.top() };
    m_freeBuffers.pop();
    return ptr;
}

void BufferPool::returnPointer(std::shared_ptr<char[]> ptr)
{
    std::lock_guard<std::mutex> lock(m_lock);
    m_freeBuffers.push(ptr);
    std::cout << "There are " << m_freeBuffers.size() << " buffers in the pool\n";
}
