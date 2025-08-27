#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "scheduler.h"
#include <QRandomGenerator>
#include <QInputDialog>
#include <QTime>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pool(nullptr)
{
    ui->setupUi(this);

    // 初始化UI状态
    ui->stopButton->setEnabled(false);
    ui->addTaskToolButton->setEnabled(false);
    // 添加任务按钮菜单
    setAddTaskMenu();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_startButton_clicked()
{
    // 获取用户设置的最小和最大线程数
    int minThreads = ui->minThreadSpinBox->value();
    int maxThreads = ui->maxThreadSpinBox->value();

    // 创建线程池
    m_pool = new ThreadPool(minThreads, maxThreads);


    // 日志输出
    connect(m_pool, &ThreadPool::logMessage, this, &MainWindow::onLogMessage);
    // 统一UI刷新
    connect(m_pool, &ThreadPool::taskListChanged, this, &MainWindow::refreshAllUI);
    connect(m_pool, &ThreadPool::threadStateChanged, this, &MainWindow::refreshAllUI);

    // 调度策略选择
    m_pool->setSchedulePolicy(static_cast<SchedulePolicy>(ui->scheduleComboBox->currentIndex()));
    ui->poolGraphicsView->setCurrentPolicy(static_cast<SchedulePolicy>(ui->scheduleComboBox->currentIndex()));
    // 更新UI状态
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->addTaskToolButton->setEnabled(true);
    ui->clearLogButton->setEnabled(true);
}


void MainWindow::on_stopButton_clicked()
{

    // 1. 检查线程池是否存在
    if (m_pool) {
        disconnect(m_pool, &ThreadPool::taskListChanged, this, nullptr); // 断开taskListChanged信号槽
        disconnect(m_pool, &ThreadPool::threadStateChanged, this, nullptr); // 断开threadStateChanged信号槽
        delete m_pool;      // 安全析构线程池，等待所有线程退出
        m_pool = nullptr;
    }

    // 2. 禁用相关按钮
    ui->stopButton->setEnabled(false);
    ui->addTaskToolButton->setEnabled(false);


    // 3. 启用“开始”按钮和调度策略选择框
    ui->startButton->setEnabled(true);

    // 4. 清空UI显示
    ui->waitingTaskList->clear();
    ui->runningTaskList->clear();
    ui->finishedTaskList->clear();

    ui->workingThreadList->clear();
    ui->idleThreadList->clear();
    ui->poolGraphicsView->clear();

    // 5. 清空统计栏
    ui->totalThreadsLabel->setText("总线程: 0");
    ui->busyThreadsLabel->setText("忙线程: 0");
    ui->idleThreadsLabel->setText("空闲线程: 0");
    ui->waitingTaskLabel->setText("等待执行任务: 0");
    ui->runningTaskLabel->setText("正在执行任务: 0");
    ui->finishedTasksLabel->setText("已完成任务: 0");
    // 清空性能统计栏
    ui->avgWaitTimeLabel->setText("平均等待时间: 0.00s");
    ui->avgResponseRatioLabel->setText("平均响应比: 0.00");
    ui->throughputLabel->setText("吞吐量: 0.00 任务/秒");
    ui->cpuUtilizationLabel->setText("CPU利用率: 0.0%");

    // 重启线程池后任务ID归零
    m_totalTasks = 0;
    // 清空map
    m_taskIdToTotalTimeMs.clear();
}

void MainWindow::setAddTaskMenu()
{
    QMenu *addTaskMenu = new QMenu(this);

    // 创建菜单项并设置data属性
    QAction* action5 = addTaskMenu->addAction("添加5个任务");
    action5->setData(5);
    
    QAction* action10 = addTaskMenu->addAction("添加10个任务");
    action10->setData(10);
    
    QAction* action20 = addTaskMenu->addAction("添加20个任务");
    action20->setData(20);
    
    addTaskMenu->addSeparator();
    
    QAction* actionCustom = addTaskMenu->addAction("自定义数量...");
    actionCustom->setData(-1); // 特殊值表示自定义

    ui->addTaskToolButton->setMenu(addTaskMenu);
}

void MainWindow::on_addTaskToolButton_clicked()
{
    if (!m_pool) {
        QMessageBox::warning(this, "警告", "线程池未启动");
        return;
    }
    addSingleTask();
}

void MainWindow::on_addTaskToolButton_triggered(QAction *action)
{
    if (!m_pool) return;
    
    int count = action->data().toInt();
    if (count == -1) {
        bool ok;
        count = QInputDialog::getInt(this, "自定义数量", "请输入任务数量", 1, 1, 100, 1, &ok);
        if (!ok) return;
    }
    
    bool ok;
    int interval = QInputDialog::getInt(this, "设置间隔", "请输入添加间隔(毫秒):", 1000, 0, 10000, 500, &ok);
    if (!ok) return;
    
    // 立即添加第一个任务
    addSingleTask();
    
    // 使用QTimer::singleShot，每隔interval毫秒添加一个任务
    for (int i = 1; i < count; ++i) {
        QTimer::singleShot(interval * i, this, [this]() {
            if (m_pool) {
                addSingleTask();
            }
        });
    }
    
    emit m_pool->logMessage(QString("[批量添加]开始添加 %1 个任务，间隔 %2ms").arg(count).arg(interval));
}

void MainWindow::on_clearLogButton_clicked()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认清空", "确定要清空运行日志吗？",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        ui->logTextBrowser->clear();
    }
}


void MainWindow::on_minThreadSpinBox_valueChanged(int arg1)
{
    // 保证最大线程数不能小于最小线程数
    ui->maxThreadSpinBox->setMinimum(arg1);
    if (ui->maxThreadSpinBox->value() < arg1) {
        ui->maxThreadSpinBox->setValue(arg1);
    }
}


void MainWindow::on_maxThreadSpinBox_valueChanged(int arg1)
{
    // 保证最大线程数不能小于最小线程数
    ui->minThreadSpinBox->setMaximum(arg1);
    if (ui->minThreadSpinBox->value() > arg1) {
        ui->minThreadSpinBox->setValue(arg1);
    }
}

void MainWindow::refreshAllUI() {
    if (!m_pool) return;
    
    // 0. 更新map, 用于绘制进度条
    ui->poolGraphicsView->setTaskIdToTotalTimeMs(m_taskIdToTotalTimeMs);

    // 1. 刷新任务列表
    // 1.1. waitingTaskList
    ui->waitingTaskList->clear();
    for (const auto& waitingTask : m_pool->getWaitingTaskVisualInfo()) {
        ui->waitingTaskList->addItem(
            QString("任务%1 (%2s,★%3)")
                .arg(waitingTask.taskId)
                .arg(waitingTask.totalTimeMs / 1000.0, 0, 'f', 1)
                .arg(waitingTask.priority)
        );
    }

    // 1.2. runningTaskList
    ui->runningTaskList->clear();
    for (const auto& threadInfo : m_pool->getThreadVisualInfo()) {
        if (threadInfo.state == 1 && threadInfo.curTaskId != -1) { // 1=忙碌
            ui->runningTaskList->addItem(
                QString("任务%1 (T:%2)").arg(threadInfo.curTaskId).arg(threadInfo.threadId));
        }
    }

    // 1.3. finishedTaskList    
    ui->finishedTaskList->clear();
    for (const auto& finishedTask : m_pool->getFinishedTaskVisualInfo()) {
        ui->finishedTaskList->addItem(
            QString("任务%1 (T:%2)").arg(finishedTask.taskId).arg(finishedTask.curThreadId));
    }

    // 2. 刷新线程列表
    // 2.1. idleThreadList
    ui->idleThreadList->clear();


    // 2.2. workingThreadList
    ui->workingThreadList->clear();

    for (const auto& threadInfo : m_pool->getThreadVisualInfo()) {
        if (threadInfo.state == 1) {    // 1=忙碌
            ui->workingThreadList->addItem(
                QString("线程%1 (#%2)").arg(threadInfo.threadId).arg(threadInfo.curTaskId));
        } else if (threadInfo.state == 0) { // 0=空闲
            ui->idleThreadList->addItem(
                QString("线程%1").arg(threadInfo.threadId));
        }
    }

    // 3. 更新统计栏
    // 3.1. 从线程池获取真实数据
    int poolTotalThreads = m_pool->getAliveNumber();
    int poolBusyThreads = m_pool->getBusyNumber();
    int poolIdleThreads = poolTotalThreads - poolBusyThreads;

    int poolWaitingTasks = m_pool->getWaitingTaskNumber();
    int poolRunningTasks = m_pool->getRunningTaskNumber();
    int poolFinishedTasks = m_pool->getFinishedTaskNumber();

    // 使用线程池的真实数据更新显示
    ui->totalThreadsLabel->setText(QString("总线程:%1").arg(poolTotalThreads));
    ui->busyThreadsLabel->setText(QString("忙线程:%1").arg(poolBusyThreads));
    ui->idleThreadsLabel->setText(QString("空闲线程:%1").arg(poolIdleThreads));
    ui->waitingTaskLabel->setText(QString("等待执行任务:%1").arg(poolWaitingTasks));
    ui->runningTaskLabel->setText(QString("正在执行任务:%1").arg(poolRunningTasks));
    ui->finishedTasksLabel->setText(QString("已完成任务:%1").arg(poolFinishedTasks));

    // 3.2. 性能统计
    double avgWaitingTimeS = 0.0;  // 直接使用秒为单位
    double avgResponseRatio = 0.0;
    double throughput = 0.0;
    double cpuUtilization = 0.0;

    // 3.2.1. 平均等待时间 和 平均响应比
    double totalWaitingTimeMs = m_pool->getTotalWaitingTimeMs();
    if (poolFinishedTasks > 0) {
        avgWaitingTimeS = totalWaitingTimeMs / (double)poolFinishedTasks / 1000.0;
        avgResponseRatio = m_pool->getTotalResponseRatio() / (double)poolFinishedTasks;
    }
    
    // 3.2.2. 吞吐量 除零保护
    double totalTimeMs = m_pool->getTotalTimeMs();
    throughput = totalTimeMs > 0 ? poolFinishedTasks / (totalTimeMs / 1000.0) : 0.0;

    // 3.2.3. CPU利用率计算（独立于已完成任务）
    cpuUtilization = poolTotalThreads > 0 ? (poolBusyThreads / (double)poolTotalThreads) * 100.0 : 0.0;

    // 更新UI显示
    ui->avgWaitTimeLabel->setText(QString("平均等待时间: %1s").arg(avgWaitingTimeS, 0, 'f', 2));
    ui->avgResponseRatioLabel->setText(QString("平均响应比: %1").arg(avgResponseRatio, 0, 'f', 2));
    ui->throughputLabel->setText(QString("吞吐量: %1 任务/秒").arg(throughput, 0, 'f', 2));
    ui->cpuUtilizationLabel->setText(QString("CPU利用率: %1%").arg(cpuUtilization, 0, 'f', 1));


    // 4. 更新可视化UI
    QList<ThreadVisualInfo> threadInfos = m_pool->getThreadVisualInfo();
    QList<TaskVisualInfo> waitingTasks = m_pool->getWaitingTaskVisualInfo();
    QList<TaskVisualInfo> finishedTasks = m_pool->getFinishedTaskVisualInfo();
    ui->poolGraphicsView->visualizeAll(threadInfos, waitingTasks, finishedTasks);
}


void MainWindow::onLogMessage(const QString& msg)
{
    ui->logTextBrowser->append(msg);
}


void MainWindow::on_scheduleComboBox_currentIndexChanged(int index)
{
    if (!m_pool) return;
    // 弹窗提示
    QMessageBox::information(
        this,
        "调度策略切换",
        QString("调度策略已切换为：%1\n等待队列将按新策略重新排序，正在执行的任务不受影响。")
            .arg(ui->scheduleComboBox->currentText())
    );
    m_pool->setSchedulePolicy(static_cast<SchedulePolicy>(index));
    ui->poolGraphicsView->setCurrentPolicy(static_cast<SchedulePolicy>(index));
    // 刷新UI
    refreshAllUI();
}


void MainWindow::addSingleTask()
{
    // 生成任务参数
    int taskId = ++m_totalTasks;
    int totalTimeMs = QRandomGenerator::global()->bounded(1000, 10001);
    int priority = QRandomGenerator::global()->bounded(1, 11);
    size_t memSize = QRandomGenerator::global()->bounded(1,65);    // 1~64B
    void* memPtr = nullptr;
    // 维护map
    m_taskIdToTotalTimeMs[taskId] = totalTimeMs;

    // 添加到线程池
    Task task;
    task.id = taskId;
    task.function = nullptr;
    task.arg = nullptr;
    task.totalTimeMs = totalTimeMs;
    task.priority = priority;
    task.memSize = memSize;
    task.memPtr = memPtr;
    task.arrivalTimestampMs = QTime::currentTime().msecsSinceStartOfDay();
    m_pool->addTask(task);
}



