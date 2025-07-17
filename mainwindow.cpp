#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pool(nullptr)
{
    ui->setupUi(this);

    // 初始化UI状态
    ui->stopButton->setEnabled(false);
    ui->addTaskButton->setEnabled(false);
    ui->clearQueueButton->setEnabled(false);
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
    ui->clearQueueButton->setEnabled(true);

    // 清空线程区（不需要手动for循环初始化idleThreadList）
    ui->idleThreadList->clear();
    ui->workingThreadList->clear();

    //在构造函数里已经有了 m_pool->broadcastThreadStateChanged();

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
    ui->clearQueueButton->setEnabled(false);

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


void MainWindow::on_clearQueueButton_clicked()
{
    if (!m_pool) {
        QMessageBox::warning(this, "警告", "线程池未启动");
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认清空", "确定要清空等待任务队列吗？",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        m_pool->clearTaskQueue();
        ui->waitingTaskList->clear();
    }
    updateStatistics();
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

void MainWindow::onTaskCompleted(int taskId)
{
    // 移除执行队列最前面的任务
    if (ui->runningTaskList->count() > 0) {
        QListWidgetItem* item = ui->runningTaskList->takeItem(0);
        ui->finishedTaskList->addItem(item);
    }
}

void MainWindow::onTaskRemoved()
{
    // 从等待队列移除最前面的任务
    if (ui->waitingTaskList->count() > 0) {
        QListWidgetItem* item = ui->waitingTaskList->takeItem(0);
        ui->runningTaskList->addItem(item);
    }
}

void MainWindow::onThreadStateChanged(int threadId, int state)
{
    QString threadName = QString("线程 %1").arg(threadId);

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
}

void MainWindow::updateStatistics()
{
    // 线程相关
    int totalThreads = ui->workingThreadList->count() + ui->idleThreadList->count();
    int busyThreads = ui->workingThreadList->count();
    int idleThreads = ui->idleThreadList->count();

    // 任务相关
    int waitingTasks = ui->waitingTaskList->count();
    int runningTasks = ui->runningTaskList->count();
    int finishedTasks = ui->finishedTaskList->count();

    // 更新线程相关标签
    ui->totalThreadsLabel->setText(QString("总线程:%1").arg(totalThreads));
    ui->busyThreadsLabel->setText(QString("忙线程:%1").arg(busyThreads));
    ui->idleThreadsLabel->setText(QString("空闲线程:%1").arg(idleThreads));

    // 更新任务相关标签
    ui->waitingTaskLabel->setText(QString("等待执行任务:%1").arg(waitingTasks));
    ui->runningTaskLabel->setText(QString("正在执行任务:%1").arg(runningTasks));
    ui->finishedTasksLabel->setText(QString("已完成任务:%1").arg(finishedTasks));
}

void MainWindow::onLogMessage(const QString& msg)
{
    ui->logTextBrowser->append(msg);
}