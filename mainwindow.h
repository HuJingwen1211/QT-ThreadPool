#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include "threadpool.h"
#include "poolview.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 按钮事件
    void on_startButton_clicked();

    void on_stopButton_clicked();

    void on_addTaskButton_clicked();

    void on_clearLogButton_clicked();

    // 线程池信号处理
    void onTaskCompleted();

    void onTaskRemoved();

    void onThreadStateChanged(int threadId);

    void onLogMessage(const QString& msg);

    // 线程数量spinBox控制
    void on_minThreadSpinBox_valueChanged(int arg1);

    void on_maxThreadSpinBox_valueChanged(int arg1);

    // 更新统计信息
    void updateStatistics();

private:
    Ui::MainWindow *ui;
    ThreadPool *m_pool;
    
    // PoolView* m_poolView;
    // ui中，把poolGraphicsView提升为PoolView

    int m_totalTasks = 0;
};
#endif // MAINWINDOW_H
