#include "threadpool.h"
#include <QDebug>
#include <QTime>

/*
 * 说明：
 * 1. 原始C++用pthread_create/pthread_join，这里用QThread的start/quit/wait。
 * 2. 线程同步用QMutex和QWaitCondition，替代pthread_mutex_t和pthread_cond_t。
 * 3. 线程池和任务队列都支持信号槽，方便和UI联动。
 * 4. 线程退出用thread->quit()和thread->wait()，自动管理线程生命周期。
 */

// WorkerThread实现
ThreadPool::WorkerThread::WorkerThread(ThreadPool* pool, int id)
    : m_pool(pool), m_id(id)
{
}

void ThreadPool::WorkerThread::run()
{
    // 一直不停的工作
    while(m_pool && !m_pool->m_shutdown)
    {
        // 访问任务队列(共享资源)加锁
        m_pool->m_lock.lock();

        // 如果任务队列为空且线程池未关闭，阻塞等待
        while (m_pool->m_taskQ->taskNumber() == 0 && !m_pool->m_shutdown)
        {
            // 阻塞线程
            qDebug() << "线程池开启且任务队列为空，阻塞！thread" << m_id << "waiting...";
            m_pool->m_notEmpty.wait(&m_pool->m_lock);

            // 解除阻塞后,检查是否需要销毁线程
            if (m_pool->m_exitNum > 0)
            {
                m_pool->m_exitNum --;
                if (m_pool->m_aliveNum > m_pool->m_minNum)
                {
                    m_pool->m_aliveNum--;
                    m_pool->m_lock.unlock();
                    m_pool->threadExit(m_id);
                    return;
                }
            }
        }
        // 程序执行到这:说明任务队列不为空
        // 检查线程池是否关闭
        if (m_pool->m_shutdown) // 若关闭
        {
            m_pool->m_lock.unlock();
            m_pool->threadExit(m_id);
            return;
        }

        // 程序执行到这:说明任务队列不为空,且线程池运行
        // 从任务队列取出任务
        Task task = m_pool->m_taskQ->takeTask();
        m_pool->m_busyNum++;
        emit m_pool->threadStateChanged(m_id, true);    // 发送信号
        m_pool->m_lock.unlock();

        // 执行任务
        qDebug() << "执行任务，thread" << m_id << "start working...";
        if (task.function) {
            task.function(task.arg);
            if (task.arg) {
                delete task.arg;
                task.arg = nullptr;
            }
        }
        // 任务处理结束
        qDebug() << "任务处理结束，thread" << m_id << "end working...";
        m_pool->m_lock.lock();
        m_pool->m_busyNum--;
        emit m_pool->threadStateChanged(m_id, false);
        emit m_pool->taskCompleted();
        m_pool->m_lock.unlock();
    }
}

// 管理者线程实现
ThreadPool::ManagerThread::ManagerThread(ThreadPool* pool)
    : m_pool(pool)
{
}
void ThreadPool::ManagerThread::run()
{
    while(m_pool && !m_pool->m_shutdown)
    {
        // 每隔5s检测一次
        QThread::sleep(5);
        // 取出线程池中的任务数和线程数量
        m_pool->m_lock.lock();
        int queueSize = m_pool->m_taskQ->taskNumber();
        int liveNum = m_pool->m_aliveNum;
        int busyNum = m_pool->m_busyNum;
        m_pool->m_lock.unlock();

        // 控制线程池扩容速度的参数。每次最多创建2个线程
        const int NUMBER = 2;
        // 当前任务个数>存活的线程数 && 存活的线程数<最大线程个数
        if (queueSize > liveNum && liveNum < m_pool->m_maxNum)
        {
            // 线程池加锁
            m_pool->m_lock.lock();
            // 有NUMBER限制，每次只创建2个线程；5s后再检查，如果有需要就再创建2个
            for (int i = 0; i < NUMBER && m_pool->m_aliveNum < m_pool->m_maxNum; ++i)
            {
                // 创建新线程
                WorkerThread* thread = new WorkerThread(m_pool, m_pool->m_aliveNum + 1);
                m_pool->m_threads.append(thread);
                thread->start();
                m_pool->m_aliveNum++;
                emit m_pool->logMessage(QString("管理者线程创建新工作线程, ID: %1").arg(m_pool->m_aliveNum));
            }
            m_pool->m_lock.unlock();
        }

        // 销毁多余的线程
        // 忙线程*2 < 存活的线程数目 && 存活的线程数 > 最小线程数量
        if (busyNum * 2 < liveNum && liveNum > m_pool->m_minNum)
        {
            m_pool->m_lock.lock();
            m_pool->m_exitNum = NUMBER;
            m_pool->m_lock.unlock();
            // 唤醒NUMBER个等待的线程，让他们退出
            for (int i = 0; i < NUMBER; ++i)
            {
                m_pool->m_notEmpty.wakeOne();
            }
            emit m_pool->logMessage("管理者线程销毁多余的线程");
        }
    }
    emit m_pool->logMessage("管理者线程退出");
}



ThreadPool::ThreadPool(int minNum, int maxNum)
    : m_minNum(minNum), m_maxNum(maxNum), m_busyNum(0), m_aliveNum(0), m_exitNum(0), m_shutdown(false)
{
    // 实例化任务队列
    m_taskQ = new TaskQueue;

    // 创建最小数量的线程
    for (int i = 0; i < minNum; ++i)
    {
        WorkerThread* thread = new WorkerThread(this, i + 1);
        m_threads.append(thread);
        thread->start();
        m_aliveNum++;
        emit logMessage(QString("创建子线程, ID: %1").arg(i + 1));
    }
    // 创建管理者线程
    m_managerThread = new ManagerThread(this);
    m_managerThread->start();
    emit logMessage("创建管理者线程");

    connect(m_taskQ, &TaskQueue::taskAdded, this, &ThreadPool::onTaskAdded);
    connect(m_taskQ, &TaskQueue::taskRemoved, this, &ThreadPool::onTaskRemoved);

    emit logMessage(QString("线程池创建完成，最小线程数: %1，最大线程数: %2").arg(minNum).arg(maxNum));
}

ThreadPool::~ThreadPool()
{
    emit logMessage("线程池开始析构，准备关闭...");

    m_shutdown = true;

    // 唤醒所有等待线程
    m_notEmpty.wakeAll();

    if (m_managerThread)
    {
        m_managerThread->wait();
        delete m_managerThread;
        m_managerThread = nullptr;
        emit logMessage("管理者线程已安全退出");
    }
    // 等待所有线程结束
    for (WorkerThread* thread : m_threads)
    {
        // thread->quit(); // 通知线程退出
        qDebug() << "[线程池] 等待线程" << thread << "退出...";
        thread->wait();
        qDebug() << "[线程池] 线程" << thread << "已安全退出";
        delete thread;
    }

    if (m_taskQ) {
        delete m_taskQ;
        m_taskQ = nullptr;
    }

    emit logMessage("线程池已正常关闭。");
}

void ThreadPool::addTask(Task task)
{
    if (m_shutdown)
    {
        return;
    }
    // 添加任务，不需要加锁，任务队列中有锁
    m_taskQ->addTask(task);
    emit logMessage("添加任务到队列");

    // 唤醒工作的线程
    // pthread_cond_signal(&m_notEmpty);
}

int ThreadPool::getAliveNumber()
{
    QMutexLocker locker(&m_lock);
    return m_aliveNum;
}

int ThreadPool::getBusyNumber()
{
    QMutexLocker locker(&m_lock);
    return m_busyNum;
}


void ThreadPool::onTaskAdded()
{
    // 唤醒一个等待的线程
    m_notEmpty.wakeOne();
    emit logMessage("任务已添加到队列");
}

void ThreadPool::onTaskRemoved()
{
    emit logMessage("任务已从队列取出");
}

void ThreadPool::threadExit(int threadId)
{
    emit logMessage(QString("线程 %1 退出").arg(threadId));
}