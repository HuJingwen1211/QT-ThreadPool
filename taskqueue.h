#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include <QObject>
#include <QMutex>
#include <QList>
#include "scheduler.h"

/*
 * 说明：
 * 1. 原始C++版本用的是pthread_mutex_t和std::queue，这里全部换成了Qt的QMutex和QQueue。
 * 2. QMutexLocker用于RAII自动加解锁，防止死锁和异常泄漏。
 * 3. 继承QObject是为了后续可以用Qt信号槽机制（比如和UI联动）。
 */

using callback = void(*)(void*);

struct Task
{
    Task() : id(0), function(nullptr), arg(nullptr), totalTimeMs(0) {}
    Task(int tid, callback f, void* a, int tt, int prio) : id(tid), function(f), arg(a), totalTimeMs(tt), priority(prio) {}
    int id;
    callback function;
    void* arg;
    int totalTimeMs;    // 总耗时
    int priority;       // 优先级
    // 这里不需要加state字段，因为taskQueue里的task状态一定是waiting
};

// 任务队列
class TaskQueue : public QObject
{
    Q_OBJECT
public:
    TaskQueue();
    ~TaskQueue();

    // 添加任务
    void addTask(const Task& task);
    void addTask(int id, callback func, void* arg, int totalTimeMs, int priority);

    // 取出一个任务
    Task takeTask();
    // 获取所有任务
    QList<Task> getTasks() const;
    // 获取当前队列中任务个数
    inline int taskNumber() const
    {
        // QMutexLocker自动加锁解锁，防止死锁
        QMutexLocker locker(&m_mutex);
        return m_queue.size();
    }

    // 清空队列
    void clearQueue();

    // 设置调度策略
    /*
    为什么要 delete m_scheduler？
    每次切换调度策略时，都会 new 一个新的调度器对象（如 new FIFOScheduler()）。
    如果不释放旧的调度器，内存会一直增长，造成内存泄漏。
    所以切换前要先 delete 掉旧的，再保存新的。
    */
    void setScheduler(TaskScheduler* scheduler) { 
        QMutexLocker locker(&m_mutex);
        if (m_scheduler) delete m_scheduler;
        m_scheduler = scheduler;
        m_scheduler->sortQueue(m_queue);
    }

private:
    mutable QMutex m_mutex;        // Qt互斥锁，替代pthread_mutex_t
    QList<Task> m_queue;
    TaskScheduler* m_scheduler = nullptr;   // 调度策略
};

#endif // TASKQUEUE_H
