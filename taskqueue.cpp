#include "taskqueue.h"


/*
 * 说明：
 * 1. 原始C++用pthread_mutex_init/destroy，这里QMutex自动管理，无需手动初始化和销毁。
 * 2. QMutexLocker用于RAII自动加解锁，防止死锁和异常泄漏。
 * 3. QQueue的enqueue/dequeue替代了std::queue的push/pop/front。
 */

TaskQueue::TaskQueue() {}

TaskQueue::~TaskQueue() {}

void TaskQueue::addTask(const Task& task) {
    QMutexLocker locker(&m_mutex);   // 自动加锁
    m_queue.enqueue(task);
}


void TaskQueue::addTask(int id, callback func, void* arg, int totalTimeMs) {
    QMutexLocker locker(&m_mutex);
    Task task(id, func, arg, totalTimeMs);
    m_queue.enqueue(task);
}

Task TaskQueue::takeTask()
{
    Task t;
    QMutexLocker locker(&m_mutex);
    if (!m_queue.isEmpty()) t = m_queue.dequeue();
    return t;
}

QList<Task> TaskQueue::getTasks() const
{
    QMutexLocker locker(&m_mutex);
    // toList返回的列表顺序和队列一致
    return m_queue.toList();
}

void TaskQueue::clearQueue()
{
    QMutexLocker locker(&m_mutex);
    m_queue.clear();
}