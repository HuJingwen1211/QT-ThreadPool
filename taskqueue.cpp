#include "taskqueue.h"
#include <QDebug>

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
    if (m_scheduler) {
        m_scheduler->insertByPolicy(m_queue, task);
    } else {
        m_queue.append(task);
    }
    // 打印当前队列内容
    QStringList ids;
    for (const auto& t : m_queue) ids << QString::number(t.id);
    qDebug() << "[TaskQueue] 当前队列内容:" << ids.join(" -> ");
}


void TaskQueue::addTask(int id, callback func, void* arg, int totalTimeMs, int priority) {
    QMutexLocker locker(&m_mutex);
    Task task(id, func, arg, totalTimeMs, priority);
    if (m_scheduler) {
        m_scheduler->insertByPolicy(m_queue, task);
    } else {
        m_queue.append(task);
    }
    // 打印当前队列内容
    QStringList ids;
    for (const auto& t : m_queue) ids << QString::number(t.id);
    qDebug() << "[TaskQueue] 当前队列内容:" << ids.join(" -> ");
}

Task TaskQueue::takeTask()
{
    Task t;
    QMutexLocker locker(&m_mutex);
    if (!m_queue.isEmpty()) t = m_queue.takeFirst();
    return t;
}

QList<Task> TaskQueue::getTasks() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue;
}

void TaskQueue::clearQueue()
{
    QMutexLocker locker(&m_mutex);
    m_queue.clear();
}
