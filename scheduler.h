#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <QList>

// 前向声明，避免循环依赖
struct Task;

enum class SchedulePolicy
{
    FIFO,
    LIFO,
    SJF,
    LJF,
    PRIO
};

class TaskScheduler
{
public:
    virtual ~TaskScheduler() = default;
    // 插入并排序
    virtual void insertByPolicy(QList<Task> &tasks, const Task& task) = 0;
    virtual void sortQueue(QList<Task> &tasks) = 0;
};

/* 先进先出FIFO
插入：append（加到队尾）
取出：takeFirst（取队头）
*/
class FIFOScheduler : public TaskScheduler
{
public:
    ~FIFOScheduler() override = default;
    void insertByPolicy(QList<Task> &tasks, const Task& task) override;
    void sortQueue(QList<Task> &tasks) override;
};

/* 后进先出LIFO
插入：prepend（加到队头）
取出：takeLast（取队尾）
*/
class LIFOScheduler : public TaskScheduler
{
public:
    ~LIFOScheduler() override = default;
    void insertByPolicy(QList<Task> &tasks, const Task& task) override;
    void sortQueue(QList<Task> &tasks) override;
};

/* 短作业优先SJF
插入：按总耗时排序（升序）
取出：takeFirst（取队头）
*/
class SJFScheduler : public TaskScheduler
{
public:
    ~SJFScheduler() override = default;
    void insertByPolicy(QList<Task> &tasks, const Task& task) override;
    void sortQueue(QList<Task> &tasks) override;
};

/* 长作业优先LJF
插入：按总耗时排序（降序）
取出：takeFirst（取队头）
*/
class LJFScheduler : public TaskScheduler
{
public:
    ~LJFScheduler() override = default;
    void insertByPolicy(QList<Task> &tasks, const Task& task) override;
    void sortQueue(QList<Task> &tasks) override;
};

class PRIOScheduler : public TaskScheduler
{
public:
    ~PRIOScheduler() override = default;
    void insertByPolicy(QList<Task> &tasks, const Task& task) override;
    void sortQueue(QList<Task> &tasks) override;
};

#endif // SCHEDULE_H
