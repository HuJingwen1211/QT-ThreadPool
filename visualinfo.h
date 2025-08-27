#ifndef VISUALINFO_H
#define VISUALINFO_H
enum ThreadState {
    THREAD_IDLE = 0,
    THREAD_BUSY = 1,
    THREAD_EXIT = -1
};
enum TaskState {
    TASK_WAITING = 0,
    TASK_RUNNING = 1,
    TASK_FINISHED = 2
};
// 只把curTimeMs放在ThreadVisualInfo中，因为curTimeMs是线程的内部状态，不需要显示
struct ThreadVisualInfo {
    int threadId;
    ThreadState state;  // 线程状态: 0=idle，1=busy, -1=exit
    int curTaskId = -1;  // 正在执行的任务id
    int curTimeMs = 0;  // 已耗时

};
// 只把totalTimeMs放在TaskVisualInfo中
struct TaskVisualInfo {
    int taskId;
    TaskState state;  // 任务状态: 0=waiting，1=running，2=finished
    int curThreadId = -1;  // 正在被哪个线程执行
    int totalTimeMs = 0;  // 总耗时
    int priority = 0;  // 优先级
    int arrivalTimestampMs = 0;  // 到达时间
    int finishTimestampMs = 0;   // 完成时间
};

#endif // VISUALINFO_H
