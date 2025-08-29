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
    m_taskQ = std::make_unique<TaskQueue>();

    // 创建最小数量的线程
    for (int i = 0; i < minNum; ++i)
    {
        auto thread = std::make_unique<WorkerThread>(this, m_nextThreadId++);
        int threadId = thread->id();
        thread->start();
        m_threads.emplace_back(std::move(thread));
        
        m_aliveNum++;
        emitDelayedSignal(QString("[线程池]创建子线程, ID: %1").arg(threadId), threadId);
    }
    // 创建管理者线程
    m_managerThread = std::make_unique<ManagerThread>(this);
    m_managerThread->start();
    emitDelayedSignal(QString("[线程池]创建管理者线程"));

    emitDelayedSignal(QString("[线程池]创建完成，最小线程数: %1，最大线程数: %2").arg(minNum).arg(maxNum));
    
    // 通信 - 固定路径
    QString statusFile = "C:\\Users\\hp\\Desktop\\threadpool_status.json";
    m_comm = std::make_unique<FileCommunication>(statusFile);
    // 任务列表变化时，自动上报状态
    connect(this, &ThreadPool::taskListChanged, this, &ThreadPool::autoReportStatus);
    // 心跳机制：定时器
    m_reportTimer = std::make_unique<QTimer>(this);
    connect(m_reportTimer.get(), &QTimer::timeout, this, &ThreadPool::autoReportStatus);
    m_reportTimer->start(1000);
}

// WorkerThread实现
ThreadPool::WorkerThread::WorkerThread(ThreadPool* pool, int id)
    : m_pool(pool), m_id(id)
{
}

void ThreadPool::WorkerThread::run()
{
    while(m_pool && !m_pool->m_shutdown)
    {
        Task task;
        bool shouldExit = false;
        {
            QMutexLocker locker(&m_pool->m_lock);
            /// 一、任务队列为空，阻塞等待任务
            while (m_pool->m_taskQ->taskNumber() == 0   //任务队列为空
                    && !m_pool->m_shutdown  //线程池未关闭
                    && m_pool->m_exitNum == 0)   //不需要缩容
            {
                // 等待条件变量，内部自动释放锁
                m_pool->m_notEmpty.wait(&m_pool->m_lock);
            }
            /// 二、任务队列里有了任务
            // 情况1：缩容退出
            if (m_pool->m_exitNum > 0
                && m_pool->m_aliveNum > m_pool->m_minNum)
            {
                m_pool->m_exitNum--;
                m_pool->m_aliveNum--;
                setState(THREAD_EXIT);
                shouldExit = true;
            }
            // 情况2：线程池关闭
            if (!shouldExit && m_pool->m_shutdown)
            {
                setState(THREAD_EXIT);
                shouldExit = true;
            }
            // 情况3：正常取任务
            if (!shouldExit)
            {
                task = m_pool->m_taskQ->takeTask();
                startTask(task);
            }
        }   // 释放锁
        if (shouldExit)
        {
            // 线程退出，发送信号
            emit m_pool->threadStateChanged(m_id);
            emit m_pool->taskListChanged();
            m_pool->threadExit(m_id);
            return;
        }
        // start的emit放在锁外，线程状态变化:IDLE->BUSY
        emit m_pool->threadStateChanged(m_id);
        emit m_pool->taskListChanged();
        // 执行任务
        executeTask(task);
        finishTask(task);
    }
}
void ThreadPool::WorkerThread::startTask(const Task& task)
{
    m_pool->m_busyNum++;

    setState(THREAD_BUSY);    // 设置忙碌状态
    setCurTaskId(task.id);
    setCurTimeMs(0);
    setCurMemSize(task.memSize);    // 设置正在处理的task的内存大小
}
void ThreadPool::WorkerThread::executeTask(const Task& task)
{
    // 分段sleep，定期更新curTimeMs
    int elapsedTimeMs = 0;  // 已耗时
    int stepTimeMs = STEP_TIME_MS;   // 刷新频率

    while (elapsedTimeMs < task.totalTimeMs) {
        QThread::msleep(stepTimeMs);
        elapsedTimeMs += stepTimeMs;
        if (elapsedTimeMs > task.totalTimeMs) elapsedTimeMs = task.totalTimeMs;  // 防止溢出
        {
            QMutexLocker locker(&m_pool->m_lock);
            setCurTimeMs(elapsedTimeMs);  
        }
        // 发送信号
        emit m_pool->threadStateChanged(m_id);
    }
    // 任务结束时，已耗时=总耗时
    {
        QMutexLocker locker(&m_pool->m_lock);
        setCurTimeMs(task.totalTimeMs);
    }
    // 发送信号
    emit m_pool->threadStateChanged(m_id);
}
void ThreadPool::WorkerThread::finishTask(const Task& task)
{
    {
        QMutexLocker locker(&m_pool->m_lock);
        m_pool->m_busyNum--;
        // 添加到已完成任务列表 （这里需要加锁，因为finishedTasks是共享资源）
        TaskVisualInfo info;
        info.taskId = task.id;
        info.state = TASK_FINISHED; // finished
        info.curThreadId = m_id;
        info.totalTimeMs = task.totalTimeMs;
        info.priority = task.priority;
        info.arrivalTimestampMs = task.arrivalTimestampMs;
        info.finishTimestampMs = QTime::currentTime().msecsSinceStartOfDay();

        m_pool->m_finishedTasks.append(info);

        // 设置空闲状态，重置所有字段
        setState(THREAD_IDLE);
        setCurTaskId(-1);
        setCurTimeMs(0);
        setCurMemSize(0);
    }



    // 发送信号
    emit m_pool->threadStateChanged(m_id);
    emit m_pool->taskListChanged(); // 任务列表变化
    emit m_pool->logMessage(QString("[线程池]任务 %1 已完成").arg(task.id));

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
        QThread::sleep(MANAGER_CHECK_INTERVAL_S);
        // 取出线程池中的任务数和线程数量
        int queueSize = 0;
        int liveNum = 0;
        int busyNum = 0;
        {
            QMutexLocker locker(&m_pool->m_lock);
            queueSize = m_pool->m_taskQ->taskNumber();
            liveNum = m_pool->m_aliveNum;
            busyNum = m_pool->m_busyNum;
        }

        // 控制线程池扩容速度的参数。每次最多创建2个线程
        const int NUMBER = THREAD_EXPAND_NUMBER;
        // 当前任务个数>存活的线程数 && 存活的线程数<最大线程个数
        if (queueSize > liveNum && liveNum < m_pool->m_maxNum)
        {
            std::vector<std::unique_ptr<WorkerThread>> newThreads;
            // 线程池加锁
            {
                QMutexLocker locker(&m_pool->m_lock);
            
                // 有NUMBER限制，每次只创建2个线程；5s后再检查，如果有需要就再创建2个
                for (int i = 0; i < NUMBER && m_pool->m_aliveNum < m_pool->m_maxNum; ++i)
                {
                    // 创建新线程
                    auto thread = std::make_unique<WorkerThread>(m_pool, m_pool->m_nextThreadId++);
                    thread->setState(THREAD_IDLE);
                    m_pool->m_aliveNum++;
                    // 先放到newThreads，再移动到m_threads，因为unique_ptr不能复制，必须移动
                    newThreads.emplace_back(std::move(thread));
                    
                }
            }// 释放锁

            for (auto& thread : newThreads)
            {
                int threadId = thread->id();
                thread->start();
                m_pool->m_threads.emplace_back(std::move(thread));
                emit m_pool->logMessage(QString("[管理者线程]创建新工作线程, ID: %1").arg(threadId));
                emit m_pool->threadStateChanged(threadId);
            }
        }

        // 销毁多余的线程
        // 忙线程*2 < 存活的线程数目 && 存活的线程数 > 最小线程数量
        if (busyNum * 2 < liveNum && liveNum > m_pool->m_minNum)
        {
            {
                QMutexLocker locker(&m_pool->m_lock);
                m_pool->m_exitNum = NUMBER;
            }
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
        m_managerThread = nullptr;
        emit logMessage("[线程池]管理者线程已安全退出");
    }
    // 等待所有线程结束
    for (const auto& thread : m_threads)
    {
        qDebug() << "[线程池] 等待线程" << thread->id() << "退出...";
        thread->wait();
        qDebug() << "[线程池] 线程" << thread->id() << "已安全退出";
    }

    if (m_taskQ) {
        m_taskQ = nullptr;
    }

    emit logMessage("[线程池]已正常关闭。");

    // 通信
    if (m_comm) {
        m_comm = nullptr;
    }
    // 心跳机制
    if (m_reportTimer) {
        m_reportTimer->stop();
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
        info.state = TASK_WAITING; // waiting
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

ThreadState ThreadPool::getThreadState(int threadId) const
{
    QMutexLocker locker(&m_lock);
    for (const auto& thread : m_threads)
    {
        if (thread->id() == threadId)
        {
            return thread->state();
        }
    }
    return THREAD_EXIT;
}

QList<ThreadVisualInfo> ThreadPool::getThreadVisualInfo() const
{
    QList<ThreadVisualInfo> threadInfos;
    QMutexLocker locker(&m_lock);
    for (const auto& thread : m_threads)
    {
        // 线程退出后不显示
        if (thread->state() == THREAD_EXIT) continue;
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
    for (const auto& thread : m_threads)
    {
        if (thread->state() == THREAD_BUSY)//running
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
