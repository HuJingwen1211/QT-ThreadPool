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

    // 添加任务
    void addTask(Task task);
    // 获取忙线程的个数
    int getBusyNumber();
    // 获取活着的线程个数
    int getAliveNumber();

signals:
    // 线程状态变化、任务完成、日志输出（方便UI联动）
    void threadStateChanged(int threadId, bool isBusy);
    void taskCompleted();
    void logMessage(const QString& message);

private slots:
    void onTaskAdded();
    void onTaskRemoved();

private:
    // // 工作的线程的任务函数
    // static void* worker(void* arg);
    // // 管理者线程的任务函数
    // static void* manager(void* arg);
    // void threadExit();

    // 工作线程类，继承QThread，重写run方法
    class WorkerThread : public QThread
    {
    public:
        WorkerThread(ThreadPool* pool, int id);
        void run() override;
    private:
        ThreadPool* m_pool;
        int m_id;
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

    void threadExit(int threadId);

private:

    QMutex m_lock;                  // Qt互斥锁，替代pthread_mutex_t
    QWaitCondition m_notEmpty;      // Qt条件变量，替代pthread_cond_t
    QList<WorkerThread*> m_threads; // Qt线程对象列表，替代pthread_t数组
    TaskQueue* m_taskQ;
    ManagerThread* m_managerThread; // 管理者线程

    int m_minNum;
    int m_maxNum;
    int m_busyNum;
    int m_aliveNum;
    int m_exitNum;
    bool m_shutdown = false;
};

#endif // THREADPOOL_H
// 定义任务结构体
