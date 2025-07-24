#ifndef VISUALINFO_H
#define VISUALINFO_H

// 只把curTimeMs放在ThreadVisualInfo中，因为curTimeMs是线程的内部状态，不需要显示
struct ThreadVisualInfo {
    int threadId;
    int state;  // 线程状态: 0=idle，1=busy, -1=exit
    int curTaskId = -1;  // 正在执行的任务id
    int curTimeMs = 0;  // 已耗时

};
// 只把totalTimeMs放在TaskVisualInfo中
struct TaskVisualInfo {
    int taskId;
    int state;  // 任务状态: 0=waiting，1=running，2=finished
    int curThreadId = -1;  // 正在被哪个线程执行
    int totalTimeMs = 0;  // 总耗时
    
};


/*
 * TaskAnimationInfo 任务动画信息结构体：待实现
 * -----------------
 * 用于描述任务在可视化界面中的动画流转信息。支持以下三种典型动画阶段：
 *
 * 1. waitingTaskQueue → threadpool
 *    - 任务从等待队列被线程取走，进入执行状态
 *    - 正常的任务分配过程
 *
 * 2. threadpool → waitingTaskQueue
 *    - 任务从线程池退回到等待队列
 *    - 发生在任务执行失败、被挂起、被抢占或线程主动放弃任务等特殊情况
 *
 * 3. threadpool → finishedTaskList
 *    - 任务执行完成，从线程池流向已完成任务列表
 *    - 正常的任务完成过程
 *
 * 该结构体通过 from/to 字段明确动画的起点和终点，配合任务ID和线程ID即可唯一确定动画流转路径。
 */
struct TaskAnimationInfo {
    int taskId;
    // 任务的动画信息
    enum class SourceType { TaskQueue, Thread } from = SourceType::TaskQueue;
    enum class TargetType { TaskQueue, Thread, FinishedList } to = TargetType::Thread;
};
#endif // VISUALINFO_H
