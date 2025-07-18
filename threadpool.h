#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <QObject>
#include <QMutex>
#include <QThread>
#include <QList>
#include <QWaitCondition>
#include "taskqueue.h"

/*
 * 说明：
 * 1. 原始C++用pthread_t数组和pthread_cond_t，这里用QThread子类和QWaitCondition。
 * 2. QMutex/QWaitCondition配合使用，替代pthread_mutex_t/pthread_cond_t。
 * 3. 线程池本身继承QObject，方便信号槽和UI联动。
 */

class ThreadPool : public QObject
{
    Q_OBJECT
public:
    ThreadPool(int min, int max);
    ~ThreadPool();
    
    /// 任务相关/////////
    // 添加任务
    void addTask(Task task);
    void addTask(int id, callback func, void* arg);
    // 获取任务队列中等待任务个数
    int getWaitingTaskNumber() const;
    // 获取任务队列中正在执行任务个数
    int getRunningTaskNumber() const;
    // 获取任务队列中已完成任务个数
    int getFinishedTaskNumber() const;

    /// 线程相关/////////
    // 获取忙线程的个数
    int getBusyNumber() const;
    // 获取活着的线程个数
    int getAliveNumber() const;
    // 获取线程状态
    int getThreadState(int threadId) const;
    // 清空任务队列
    void clearTaskQueue();



signals:
    // 线程状态变化、任务完成、日志输出（方便UI联动）
    void threadStateChanged(int threadId); 
    void taskCompleted(int taskId);
    void taskRemoved();
    void logMessage(const QString& message);

private slots:
    void onTaskAdded();

private:
    void threadExit(int threadId);
    void emitDelayedSignal(const QString& logMsg = "", int threadId = -1);

    // 工作线程类，继承QThread，重写run方法
    class WorkerThread : public QThread
    {
    public:
        WorkerThread(ThreadPool* pool, int id);
        void run() override;
        int id() const { return m_id; }
        // 新增state字段，线程状态：0=空闲, 1=忙碌, -1=退出
        int state() const { return m_state; }   // 内部使用，如果外部使用需要用getThreadState()
        void setState(int state) { m_state = state; }
    private:
        ThreadPool* m_pool;
        int m_id;
        int m_state = 0; // 0=空闲, 1=忙碌, -1=退出
    };

    // 管理者线程类，继承QThread，重写run方法
    class ManagerThread : public QThread
    {
    public:
        ManagerThread(ThreadPool* pool);
        void run() override;
    private:
        ThreadPool* m_pool;
    };

   

private:

    mutable QMutex m_lock;          // Qt互斥锁，替代pthread_mutex_t
    QWaitCondition m_notEmpty;      // Qt条件变量，替代pthread_cond_t
    QList<WorkerThread*> m_threads; // Qt线程对象列表，替代pthread_t数组
    TaskQueue* m_taskQ;
    ManagerThread* m_managerThread; // 管理者线程

    int m_minNum;
    int m_maxNum;
    int m_busyNum;  // 正在执行任务的线程个数
    int m_aliveNum;
    int m_exitNum;

    int m_finishedTasks = 0;    // 已完成任务个数, 用于统计栏显示
    bool m_shutdown = false;
};

#endif // THREADPOOL_H
// 定义任务结构体
