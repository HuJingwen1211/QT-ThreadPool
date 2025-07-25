#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <QObject>
#include <QMutex>
#include <QThread>
#include <QList>
#include <QWaitCondition>
#include "taskqueue.h"
#include "visualinfo.h"

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
    void addTask(int id, callback func, void* arg, int totalTimeMs);
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
    // 获得线程可视化信息
    QList<ThreadVisualInfo> getThreadVisualInfo() const;
    // 获得任务可视化信息
    QList<TaskVisualInfo> getWaitingTaskVisualInfo() const;
    QList<TaskVisualInfo> getFinishedTaskVisualInfo() const;

signals:
    // 线程状态变化、任务完成、日志输出（方便UI联动）
    void threadStateChanged(int threadId); 
    // 任务列表变化
    void taskListChanged();
    // 日志输出
    void logMessage(const QString& message);

private:
    void threadExit(int threadId);
    void emitDelayedSignal(const QString& logMsg = "", int threadId = -1);

/*
    QThread 线程类核心说明
    - start()    // 启动线程，自动调用 run()
    - run()      // 线程主函数，写线程要做的事（本项目：取任务并执行）
    - exec()     // 启动事件循环（本项目未用）
    - quit()     // 退出事件循环（本项目未用）
    - wait()     // 等待线程结束（析构时用）
    - 信号槽     // 线程与主线程通信（如任务完成、状态变化）

    【本项目流程简述】
    - WorkerThread 继承 QThread，重写 run()，实现线程池工作循环。
    - 线程池用 start() 启动线程，run() 里不断取任务、执行任务、更新状态。
    - 线程状态和当前任务ID通过成员变量和信号槽与主线程/UI联动。
    - 析构时用 wait() 等待所有线程安全退出。
*/
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
        // 新增curTaskId字段：线程忙碌时正在处理的task的id
        int curTaskId() const { return m_curTaskId; }
        void setCurTaskId(int curTaskId) { m_curTaskId = curTaskId; }
        // 新增curTimeMs字段：线程忙碌时正在处理的task的已耗时
        int curTimeMs() const { return m_curTimeMs; }
        void setCurTimeMs(int curTimeMs) { m_curTimeMs = curTimeMs; }

    private:
        ThreadPool* m_pool;
        int m_id;
        int m_state = 0; // 0=空闲, 1=忙碌, -1=退出
        int m_curTaskId = -1;
        int m_curTimeMs = 0;
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

    QList<TaskVisualInfo> m_finishedTasks;
    bool m_shutdown = false;
};

#endif // THREADPOOL_H
// 定义任务结构体
