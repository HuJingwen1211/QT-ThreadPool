#include "scheduler.h"
#include "taskqueue.h"
#include <algorithm>

void FIFOScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.append(task);
}
void FIFOScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.id < b.id;
    });
}
void LIFOScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.prepend(task);
}
void LIFOScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.id > b.id;
    });
}
void SJFScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.append(task);
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.totalTimeMs < b.totalTimeMs;
    });
}
void SJFScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.totalTimeMs < b.totalTimeMs;
    });
}
void LJFScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.append(task);
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.totalTimeMs > b.totalTimeMs;
    });
}
void LJFScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.totalTimeMs > b.totalTimeMs;
    });
}
void PRIOScheduler::insertByPolicy(QList<Task> &tasks, const Task& task) {
    tasks.append(task);
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.priority > b.priority;
    });
}
void PRIOScheduler::sortQueue(QList<Task> &tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.priority > b.priority;
    });
}