#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
// #include <QCoreApplication>
// #include <QDebug>
// #include <QThread>
// #include <QTimer>
// #include "threadpool.h"

// const int TASK_COUNT = 20;

// void simpleTask(void* arg) {
//     int taskId = *(int*)arg;
//     qDebug() << "[任务函数] 任务" << taskId << "开始执行，线程ID:" << QThread::currentThreadId();

//     int sleepTime = 2000 + (taskId % 5) * 1000;  // 2-6秒
//     QThread::msleep(sleepTime);
//     qDebug() << "[任务函数] 任务" << taskId << "准备释放参数内存:" << arg;
//     // delete static_cast<int*>(arg);
//     qDebug() << "[任务函数] 任务" << taskId << "执行完毕";
// }
// int main(int argc, char *argv[])
// {
//     QCoreApplication a(argc, argv);

//     ThreadPool* pool = new ThreadPool(1, 10);

//     QObject::connect(pool, &ThreadPool::logMessage, [](const QString& msg){
//         qDebug() << "[线程池日志]" << msg;
//     });

//     int finished = 0;
//     QObject::connect(pool, &ThreadPool::taskCompleted, [&](){
//         finished++;
//         qDebug() << "[主线程] 已完成任务数:" << finished;
//         if (finished == TASK_COUNT) {
//             qDebug() << "[主线程] 所有任务完成，退出主程序";
//             QTimer::singleShot(3000, &a, [&](){
//                 qDebug() << "[主线程] 准备析构线程池...";
//                 delete pool; // 等所有线程都安全退出后再析构线程池
//                 QCoreApplication::quit();
//             });
//         }
//     });

//     for (int i = 1; i <= TASK_COUNT; ++i) {
//         int* arg = new int(i);
//         qDebug() << "[主线程] 分配参数内存:" << arg << "任务号:" << i;
//         Task task(simpleTask, arg);
//         pool->addTask(task);
//         qDebug() << "[主线程] 已添加任务" << i;
//         QThread::msleep(200);
//     }

//     qDebug() << "[主线程] 所有任务已添加，等待任务完成...";

//     return a.exec();
// }
