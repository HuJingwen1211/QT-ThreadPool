#include "threadpool.h"
#include <QDebug>
#include <QTime>
#include <QTimer>
#include <QJsonArray>

/*
 * 说明：
 * 1. 原始C++用pthread_create/pthread_join，这里用QThread的start/quit/wait。
 * 2. 线程同步用QMutex和QWaitCondition，替代pthread_mutex_t和pthread_cond_t。
 * 3. 线程池和任务队列都支持信号槽，方便和UI联动。
 * 4. 线程退出用thread->quit()和thread->wait()，自动管理线程生命周期。
 */
ThreadPool::ThreadPool(int minNum, int maxNum)
    : m_minNum(minNum), m_maxNum(maxNum), m_busyNum(0), m_aliveNum(0), m_exitNum(0), m_shutdown(false)
{
    // 记录线程池开始时间
    m_poolStartTimestamp = QTime::currentTime().msecsSinceStartOfDay();
    // 实例化任务队列
    m_taskQ = new TaskQueue;

    // 创建最小数量的线程
    for (int i = 0; i < minNum; ++i)
    {
        WorkerThread* thread = new WorkerThread(this, i + 1);
        m_threads.append(thread);
        thread->start();
        m_aliveNum++;
        emitDelayedSignal(QString("[线程池]创建子线程, ID: %1").arg(i + 1), i + 1);
    }
    // 创建管理者线程
    m_managerThread = new ManagerThread(this);
    m_managerThread->start();
    emitDelayedSignal(QString("[线程池]创建管理者线程"));

    emitDelayedSignal(QString("[线程池]创建完成，最小线程数: %1，最大线程数: %2").arg(minNum).arg(maxNum));
    
    // 通信 - 固定路径
    QString statusFile = "C:\\Users\\hp\\Desktop\\threadpool_status.json";
    m_comm = new FileCommunication(statusFile);
    // 任务列表变化时，自动上报状态
    connect(this, &ThreadPool::taskListChanged, this, &ThreadPool::autoReportStatus);
    // 心跳机制：定时器
    m_reportTimer = new QTimer(this);
    connect(m_reportTimer, &QTimer::timeout, this, &ThreadPool::autoReportStatus);
    m_reportTimer->start(1000);
}

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
            // qDebug() << "线程池开启且任务队列为空，阻塞！thread" << m_id << "waiting...";
            m_pool->m_notEmpty.wait(&m_pool->m_lock);

            // 解除阻塞后,检查是否需要销毁线程
            if (m_pool->m_exitNum > 0)
            {
                m_pool->m_exitNum --;
                if (m_pool->m_aliveNum > m_pool->m_minNum)
                {
                    m_pool->m_aliveNum--;
                    setState(-1);   // 线程退出
                    emit m_pool->threadStateChanged(m_id);  // 先发射状态变化信号
                    emit m_pool->taskListChanged(); // 任务列表变化
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
            setState(-1);
            emit m_pool->threadStateChanged(m_id);
            emit m_pool->taskListChanged(); // 任务列表变化
            m_pool->m_lock.unlock();
            m_pool->threadExit(m_id);
            return;
        }

        // 程序执行到这:说明任务队列不为空,且线程池运行
        // 从任务队列取出任务
        Task task = m_pool->m_taskQ->takeTask();
        int curTaskId = task.id;
        int totalTimeMs = task.totalTimeMs;

        m_pool->m_busyNum++;
        setState(1);    // 设置忙碌状态
        setCurTaskId(curTaskId);
        setCurTimeMs(0);
        setCurMemSize(task.memSize);    // 设置正在处理的task的内存大小
        emit m_pool->threadStateChanged(m_id);    // 发送信号
        emit m_pool->taskListChanged(); // 任务列表变化
        m_pool->m_lock.unlock();

        // 执行任务
        // 分段sleep，定期更新curTimeMs
        int elapsedTimeMs = 0;  // 已耗时
        int stepTimeMs = 100;   // 刷新频率，每100ms刷新一次
        while (elapsedTimeMs < totalTimeMs) {
            QThread::msleep(stepTimeMs);
            elapsedTimeMs += stepTimeMs;
            if (elapsedTimeMs > totalTimeMs) elapsedTimeMs = totalTimeMs;  // 防止溢出
            setCurTimeMs(elapsedTimeMs);
            emit m_pool->threadStateChanged(m_id);  // 通知UI更新
        }
        setCurTimeMs(totalTimeMs);  // 任务结束时，已耗时=总耗时
        emit m_pool->threadStateChanged(m_id);  // 通知UI更新

        // 可以注释 因为目前没有用到执行函数
        // if (task.function) {
        //     task.function(task.arg);
        //     if (task.arg) {
        //         delete task.arg;
        //         task.arg = nullptr;
        //     }
        // }
        // 任务处理结束
        m_pool->m_lock.lock();
        m_pool->m_busyNum--;
        // 添加到已完成任务列表
        TaskVisualInfo info;
        info.taskId = curTaskId;
        info.state = 2; // finished
        info.curThreadId = m_id;
        info.totalTimeMs = totalTimeMs;
        info.priority = task.priority;
        info.arrivalTimestampMs = task.arrivalTimestampMs;
        info.finishTimestampMs = QTime::currentTime().msecsSinceStartOfDay();

        m_pool->m_finishedTasks.append(info);

        setState(0);    // 设置空闲状态
        emit m_pool->threadStateChanged(m_id);
        emit m_pool->taskListChanged(); // 任务列表变化
        emit m_pool->logMessage(QString("[线程池]任务 %1 已完成").arg(task.id));
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
                thread->setState(0);
                m_pool->m_aliveNum++;
                emit m_pool->logMessage(QString("[管理者线程]创建新工作线程, ID: %1").arg(m_pool->m_aliveNum));
                emit m_pool->threadStateChanged(m_pool->m_aliveNum);
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
            emit m_pool->logMessage(QString("[管理者线程]销毁%1个线程").arg(NUMBER));
        }
    }
    emit m_pool->logMessage("[管理者线程]退出");
}




ThreadPool::~ThreadPool()
{
    emit logMessage("[线程池]开始析构，准备关闭...");

    m_shutdown = true;

    // 唤醒所有等待线程
    m_notEmpty.wakeAll();

    if (m_managerThread)
    {
        m_managerThread->wait();
        delete m_managerThread;
        m_managerThread = nullptr;
        emit logMessage("[线程池]管理者线程已安全退出");
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

    emit logMessage("[线程池]已正常关闭。");

    // 通信
    if (m_comm) {
        delete m_comm;
        m_comm = nullptr;
    }
    // 心跳机制
    if (m_reportTimer) {
        m_reportTimer->stop();
        delete m_reportTimer;
        m_reportTimer = nullptr;
    }
}

void ThreadPool::addTask(Task task)
{
    if (m_shutdown) return;
    // 添加任务，不需要加锁，任务队列中有锁
    m_taskQ->addTask(task);
    // 唤醒一个等待的线程
    m_notEmpty.wakeOne();
    // emit logMessage(QString("[线程池]添加任务 %1 到队列").arg(task.id));
    emit logMessage(
        QString("[线程池]添加任务 %1 到队列 (耗时:%2s, 优先级:%3, 内存:%4B)")
            .arg(task.id)
            .arg(task.totalTimeMs / 1000.0, 0, 'f', 1)
            .arg(task.priority)
            .arg(task.memSize)
    );
    emit taskListChanged();
}

/// 任务相关/////////
// 获取任务队列中等待任务个数
int ThreadPool::getWaitingTaskNumber() const
{
    return m_taskQ->taskNumber();
}

// 获取任务队列中正在执行任务个数
int ThreadPool::getRunningTaskNumber() const
{
    QMutexLocker locker(&m_lock);
    return m_busyNum;   // 忙碌的线程个数 = 正在执行任务的个数
}

// 获取任务队列中已完成任务个数
int ThreadPool::getFinishedTaskNumber() const
{
    QMutexLocker locker(&m_lock);
    return m_finishedTasks.size();
}

QList<TaskVisualInfo> ThreadPool::getWaitingTaskVisualInfo() const
{
    QList<TaskVisualInfo> waitingTaskInfos;
    QMutexLocker locker(&m_lock);
    for (auto task : m_taskQ->getTasks())
    {
        TaskVisualInfo info;
        info.taskId = task.id;
        info.state = 0; // waiting
        info.curThreadId = -1;
        info.totalTimeMs = task.totalTimeMs;
        info.priority = task.priority;
        info.arrivalTimestampMs = task.arrivalTimestampMs;  // 用于统计HR响应比
        info.finishTimestampMs = 0;  // 等待任务不参与性能统计
        waitingTaskInfos.append(info);
    }
    return waitingTaskInfos;
}
QList<TaskVisualInfo> ThreadPool::getFinishedTaskVisualInfo() const
{
    QMutexLocker locker(&m_lock);
    return m_finishedTasks;
}

int ThreadPool::getTotalWaitingTimeMs()
{
    QMutexLocker locker(&m_lock);
    int totalWaitingTimeMs = 0;
    for (const auto& finishedTask : m_finishedTasks)
    {
        totalWaitingTimeMs += finishedTask.finishTimestampMs - finishedTask.arrivalTimestampMs;
    }
    return totalWaitingTimeMs;
}

// 获取总响应比 = (等待时间 + 服务时间) / 服务时间
double ThreadPool::getTotalResponseRatio()
{
    QMutexLocker locker(&m_lock);
    double totalResponseRatio = 0.0;
    int validTasks = 0;
    
    for (const auto& finishedTask : m_finishedTasks)
    {
        int waitTime = finishedTask.finishTimestampMs - finishedTask.arrivalTimestampMs;
        int executionTime = finishedTask.totalTimeMs;
        
        if (executionTime > 0) {
            double responseRatio = (waitTime + executionTime) / (double)executionTime;
            totalResponseRatio += responseRatio;
            validTasks++;
        }
    }
    return validTasks > 0 ? totalResponseRatio : 0.0;
}

int ThreadPool::getTotalTimeMs()
{
    return QTime::currentTime().msecsSinceStartOfDay() - m_poolStartTimestamp;
}



/// 线程相关/////////

int ThreadPool::getAliveNumber() const
{
    QMutexLocker locker(&m_lock);
    return m_aliveNum;
}

int ThreadPool::getBusyNumber() const
{
    QMutexLocker locker(&m_lock);
    return m_busyNum;
}

void ThreadPool::threadExit(int threadId)
{
    emit logMessage(QString("[线程池]线程 %1 退出").arg(threadId));
}


// 构造函数中，延迟广播线程状态变化
void ThreadPool::emitDelayedSignal(const QString& logMsg, int threadId)
{
    QTimer::singleShot(0, this, [this, logMsg, threadId]() {
        if (threadId != -1) {
            emit threadStateChanged(threadId);
        }
        if (!logMsg.isEmpty()) {
            emit logMessage(logMsg);
        }
    });
}

int ThreadPool::getThreadState(int threadId) const
{
    QMutexLocker locker(&m_lock);
    for (auto thread : m_threads)
    {
        if (thread->id() == threadId)
        {
            return thread->state();
        }
    }
    return -1;
}

QList<ThreadVisualInfo> ThreadPool::getThreadVisualInfo() const
{
    QList<ThreadVisualInfo> threadInfos;
    QMutexLocker locker(&m_lock);
    for (auto thread : m_threads)
    {
        // 线程退出后不显示
        if (thread->state() == -1) continue;
        ThreadVisualInfo info;
        info.threadId = thread->id();
        info.state = thread->state();
        info.curTaskId = thread->curTaskId();
        info.curTimeMs = thread->curTimeMs();
        // 更多字段待补充
        threadInfos.append(info);   
    }
    return threadInfos;
}


void ThreadPool::setSchedulePolicy(SchedulePolicy policy) {
    TaskScheduler* scheduler = nullptr;
    QString policyName;
    switch (policy) {
        case SchedulePolicy::FIFO:
            scheduler = new FIFOScheduler();
            policyName = "FIFO";
            break;
        case SchedulePolicy::LIFO:
            scheduler = new LIFOScheduler();
            policyName = "LIFO";
            break;
        case SchedulePolicy::SJF:
            scheduler = new SJFScheduler();
            policyName = "SJF";
            break;
        case SchedulePolicy::LJF:
            scheduler = new LJFScheduler();
            policyName = "LJF";
            break;
        case SchedulePolicy::PRIO:
            scheduler = new PRIOScheduler();
            policyName = "PRIO";
            break;
        case SchedulePolicy::HRRN:
            scheduler = new HRRNScheduler();
            policyName = "HRRN";
            break;
        default:
            scheduler = new FIFOScheduler();
            policyName = "FIFO";
            break;
    }
    m_taskQ->setScheduler(scheduler);
    emit logMessage(QString("[线程池]当前调度策略: %1").arg(policyName));
}

/// 通信相关/////////
void ThreadPool::autoReportStatus()
{
    if (!m_comm || m_shutdown) return;
    QJsonObject data;
    QJsonArray activeTasks;

    QMutexLocker locker(&m_lock);
    for (auto thread : m_threads)
    {
        if (thread->state() == 1)//running
        {
            QJsonObject task;
            task["id"] = thread->curTaskId();
            task["memSize"] = static_cast<qint64>(thread->curMemSize());
            activeTasks.append(task);
        }
    }
    
    data["activeTasks"] = activeTasks;  // 活跃线程
    m_comm->send(data);
}
