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

    // 日志输出
    void onLogMessage(const QString& msg);
    // 刷新所有UI
    void refreshAllUI();

    // 线程数量spinBox控制
    void on_minThreadSpinBox_valueChanged(int arg1);

    void on_maxThreadSpinBox_valueChanged(int arg1);



private:
    Ui::MainWindow *ui;
    ThreadPool *m_pool;
    // ui中，把poolGraphicsView提升为PoolView后,不再需要PoolView* m_poolView这个成员变量

    int m_totalTasks = 0;
};
#endif // MAINWINDOW_H
