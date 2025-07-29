#include "scheduler.h"
#include "taskqueue.h"
#include <algorithm>
#include <QTime>

// ============================FIFO============================
void FIFOScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.append(task);
}
void FIFOScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.id < b.id;
    });
}
// ============================LIFO============================
void LIFOScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.prepend(task);
}
void LIFOScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.id > b.id;
    });
}
// ============================SJF============================
void SJFScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.append(task);
    sortQueue(tasks);
}
void SJFScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.totalTimeMs < b.totalTimeMs;
    });
}
// ============================LJF============================
void LJFScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.append(task);
    sortQueue(tasks);
}
void LJFScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.totalTimeMs > b.totalTimeMs;
    });
}
// ============================PRIO============================
void PRIOScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.append(task);
    sortQueue(tasks);
}
void PRIOScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.priority > b.priority;
    });
}
// ============================HRRN============================
void HRRNScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.append(task);
    sortQueue(tasks);
}
void HRRNScheduler::sortQueue(QList<Task> &tasks) {
    int currentTime = QTime::currentTime().msecsSinceStartOfDay();

    std::sort(tasks.begin(), tasks.end(), [currentTime](const Task& a, const Task& b) {
        int waitTimeA = currentTime - a.arrivalTimestampMs;
        int waitTimeB = currentTime - b.arrivalTimestampMs;
        
        double responseRatioA = (waitTimeA + a.totalTimeMs) / (double)a.totalTimeMs;
        double responseRatioB = (waitTimeB + b.totalTimeMs) / (double)b.totalTimeMs;
        
        return responseRatioA > responseRatioB; // 响应比高的排在前面
    });

}