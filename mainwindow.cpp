#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pool(nullptr)
    // , m_poolView(new PoolView(this))    //  在构造函数初始化列表里直接 new 了一个 PoolView 对象，并赋值给 m_poolView。
{
    ui->setupUi(this);

    // 初始化UI状态
    ui->stopButton->setEnabled(false);
    ui->addTaskButton->setEnabled(false);
    // ui->clearLogButton->setEnabled(false);


    // 初始化可视化UI
    // ui->poolGraphicsView->setScene(m_poolView->scene());
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


    // 连接信号
    connect(m_pool, &ThreadPool::logMessage, this, &MainWindow::onLogMessage);
    connect(m_pool, &ThreadPool::taskCompleted, this, &MainWindow::onTaskCompleted);
    connect(m_pool, &ThreadPool::taskRemoved, this, &MainWindow::onTaskRemoved);
    connect(m_pool, &ThreadPool::threadStateChanged, this, &MainWindow::onThreadStateChanged);
    

    // 更新统计信息
    connect(m_pool, &ThreadPool::threadStateChanged, this, &MainWindow::updateStatistics);
    connect(m_pool, &ThreadPool::taskCompleted, this, &MainWindow::updateStatistics);
    connect(m_pool, &ThreadPool::taskRemoved, this, &MainWindow::updateStatistics);

    // 更新UI状态
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->addTaskButton->setEnabled(true);
    ui->clearLogButton->setEnabled(true);

    // 清空线程区（不需要手动for循环初始化idleThreadList）
    ui->idleThreadList->clear();
    ui->workingThreadList->clear();

    updateStatistics();
    
}


void MainWindow::on_stopButton_clicked()
{
    // 1. 检查线程池是否存在
    if (m_pool) {
        delete m_pool;      // 安全析构线程池，等待所有线程退出
        m_pool = nullptr;
    }

    // 2. 禁用相关按钮
    ui->stopButton->setEnabled(false);
    ui->addTaskButton->setEnabled(false);
    // ui->clearLogButton->setEnabled(false);

    // 3. 启用“开始”按钮
    ui->startButton->setEnabled(true);

    // 4. 清空UI显示
    ui->waitingTaskList->clear();
    ui->runningTaskList->clear();
    ui->finishedTaskList->clear();

    ui->workingThreadList->clear();
    ui->idleThreadList->clear();

    
    updateStatistics();

}


void MainWindow::on_addTaskButton_clicked()
{
    if (!m_pool) {
        QMessageBox::warning(this, "警告", "线程池未启动");
        return;
    }
    // 生成任务参数
    int taskId = ++m_totalTasks;
    int* arg = new int(taskId);

    // 任务函数
    auto taskFunc = [](void* arg) {
        int id = *(int*)arg;
        qDebug() << "[任务]" << id << "开始";
        QThread::msleep(5000 + (id % 3) * 500); // 模拟任务耗时
        qDebug() << "[任务]" << id << "完成";
        // delete arg; // 释放参数内存 这里千万不能写 不然就重复释放了
    };

    // 添加到线程池
    m_pool->addTask(taskId, taskFunc, arg);

    // 更新UI显示
    ui->waitingTaskList->addItem(QString("任务 %1").arg(taskId));
    updateStatistics();
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

void MainWindow::onTaskCompleted()
{
    // 移除任务执行队列最前面的任务
    if (ui->runningTaskList->count() > 0) {
        QListWidgetItem* item = ui->runningTaskList->takeItem(0);
        ui->finishedTaskList->addItem(item);
    }
}

void MainWindow::onTaskRemoved()
{
    // 移除任务等待队列最前面的任务
    if (ui->waitingTaskList->count() > 0) {
        QListWidgetItem* item = ui->waitingTaskList->takeItem(0);
        ui->runningTaskList->addItem(item);
    }
}

void MainWindow::onThreadStateChanged(int threadId)
{
    if (!m_pool) {
        return;
    }
    QString threadName = QString("线程 %1").arg(threadId);
    // 获得线程状态
    int state = m_pool->getThreadState(threadId);

    /// 1.文字UI更新
    // 先从两个列表中移除该线程（防止重复）
    auto removeFromList = [&](QListWidget* list) {
        QList<QListWidgetItem*> items = list->findItems(threadName, Qt::MatchExactly);
        for (auto item : items) {
            delete list->takeItem(list->row(item));
        }
    };
    removeFromList(ui->idleThreadList);
    removeFromList(ui->workingThreadList);

    // 根据状态添加到对应列表
    if (state == 1) {
        ui->workingThreadList->addItem(threadName);
    } else if (state == 0) {
        ui->idleThreadList->addItem(threadName);
    }
    // state == -1 线程退出 不添加到任何列表


    /// 2.可视化UI更新
    QList<ThreadVisualInfo> threadInfos = m_pool->getThreadVisualInfo();
    // m_poolView->visualizeThreads(threadInfos);
    ui->poolGraphicsView->visualizeThreads(threadInfos);
}

void MainWindow::updateStatistics()
{
    // 检查线程池是否还存在
    if (!m_pool) {
        // 线程池不存在时，所有计数为0
        ui->totalThreadsLabel->setText("总线程:0");
        ui->busyThreadsLabel->setText("忙线程:0");
        ui->idleThreadsLabel->setText("空闲线程:0");
        ui->waitingTaskLabel->setText("等待执行任务:0");
        ui->runningTaskLabel->setText("正在执行任务:0");
        ui->finishedTasksLabel->setText("已完成任务:0");
        return;
    }

    // 从线程池获取真实数据
    int poolTotalThreads = m_pool->getAliveNumber();
    int poolBusyThreads = m_pool->getBusyNumber();
    int poolIdleThreads = poolTotalThreads - poolBusyThreads;

    int poolWaitingTasks = m_pool->getWaitingTaskNumber();
    int poolRunningTasks = m_pool->getRunningTaskNumber();
    int poolFinishedTasks = m_pool->getFinishedTaskNumber();

    // 从UI获取显示数据
    int uiTotalThreads = ui->workingThreadList->count() + ui->idleThreadList->count();
    int uiBusyThreads = ui->workingThreadList->count();
    int uiIdleThreads = ui->idleThreadList->count();

    int uiWaitingTasks = ui->waitingTaskList->count();
    int uiRunningTasks = ui->runningTaskList->count();
    int uiFinishedTasks = ui->finishedTaskList->count();

    // 检查一致性
    bool threadsConsistent = (poolTotalThreads == uiTotalThreads) && 
        (poolBusyThreads == uiBusyThreads) && 
        (poolIdleThreads == uiIdleThreads);

    bool tasksConsistent = (poolWaitingTasks == uiWaitingTasks) && 
        (poolRunningTasks == uiRunningTasks) && 
        (poolFinishedTasks == uiFinishedTasks);

    // 如果数据不一致，使用线程池的真实数据，并记录错误
    if (!threadsConsistent || !tasksConsistent) {
        qDebug() << "[数据不一致] 线程池数据 vs UI数据:";
        qDebug() << "  线程总数:" << poolTotalThreads << "vs" << uiTotalThreads;
        qDebug() << "  忙碌线程:" << poolBusyThreads << "vs" << uiBusyThreads;
        qDebug() << "  空闲线程:" << poolIdleThreads << "vs" << uiIdleThreads;
        qDebug() << "  等待任务:" << poolWaitingTasks << "vs" << uiWaitingTasks;
        qDebug() << "  执行任务:" << poolRunningTasks << "vs" << uiRunningTasks;
        qDebug() << "  完成任务:" << poolFinishedTasks << "vs" << uiFinishedTasks;
    }
    // // 更新线程相关标签
    // ui->totalThreadsLabel->setText(QString("总线程:%1").arg(totalThreads));
    // ui->busyThreadsLabel->setText(QString("忙线程:%1").arg(busyThreads));
    // ui->idleThreadsLabel->setText(QString("空闲线程:%1").arg(idleThreads));

    // // 更新任务相关标签
    // ui->waitingTaskLabel->setText(QString("等待执行任务:%1").arg(waitingTasks));
    // ui->runningTaskLabel->setText(QString("正在执行任务:%1").arg(runningTasks));
    // ui->finishedTasksLabel->setText(QString("已完成任务:%1").arg(finishedTasks));

    // 使用线程池的真实数据更新显示
    ui->totalThreadsLabel->setText(QString("总线程:%1").arg(poolTotalThreads));
    ui->busyThreadsLabel->setText(QString("忙线程:%1").arg(poolBusyThreads));
    ui->idleThreadsLabel->setText(QString("空闲线程:%1").arg(poolIdleThreads));
    ui->waitingTaskLabel->setText(QString("等待执行任务:%1").arg(poolWaitingTasks));
    ui->runningTaskLabel->setText(QString("正在执行任务:%1").arg(poolRunningTasks));
    ui->finishedTasksLabel->setText(QString("已完成任务:%1").arg(poolFinishedTasks));
}

void MainWindow::onLogMessage(const QString& msg)
{
    ui->logTextBrowser->append(msg);
}
