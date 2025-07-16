#include "taskqueue.h"


/*
 * 说明：
 * 1. 原始C++用pthread_mutex_init/destroy，这里QMutex自动管理，无需手动初始化和销毁。
 * 2. QMutexLocker用于RAII自动加解锁，防止死锁和异常泄漏。
 * 3. QQueue的enqueue/dequeue替代了std::queue的push/pop/front。
 */

TaskQueue::TaskQueue()
{
    // QMutex不需要手动初始化
    // pthread_mutex_init(&m_mutex, NULL);
}

TaskQueue::~TaskQueue()
{
    // QMutex不需要手动销毁
    // pthread_mutex_destroy(&m_mutex);
}

void TaskQueue::addTask(Task& task)
{
    // pthread_mutex_lock(&m_mutex);
    QMutexLocker locker(&m_mutex);   // 自动加锁
    m_queue.enqueue(task);
    // pthread_mutex_unlock(&m_mutex);

    emit taskAdded();   // 发送信号,方便UI联动
}

void TaskQueue::addTask(callback func, void* arg)
{
    QMutexLocker locker(&m_mutex);
    Task task(func, arg);
    m_queue.enqueue(task);

    emit taskAdded();
}

Task TaskQueue::takeTask()
{
    Task t;
    QMutexLocker locker(&m_mutex);
    if (!m_queue.isEmpty())
    {
        t = m_queue.dequeue();
        emit taskRemoved();
    }
    return t;
}

