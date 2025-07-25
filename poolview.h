#ifndef POOLVIEW_H
#define POOLVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include "visualinfo.h"
#include "scheduler.h"

class PoolView : public QGraphicsView
{
    Q_OBJECT
public:
    PoolView(QWidget* parent = nullptr);
    ~PoolView();

    /*
    可视化线程的设计:
    - 传递参数:不用线程池对象, 而是线程池的线程ID列表以及线程状态列表state.
    - 原因:PoolView只关心“要画几个矩形、每个什么颜色、显示什么id”，不关心线程对象本身。
    - 这样UI和业务完全分离，后续如果线程实现变化，UI代码不用动。
    */

   /*
    UI中将poolGraphicsView“提升为”PoolView后：
    - UI里的poolGraphicsView就变成了PoolView对象。
    - 所以，PoolView类中，不需要再定义scene()和viewport()。
    - 直接使用ui->poolGraphicsView->scene()和ui->poolGraphicsView->viewport()即可。
    - 也不需要新建PoolView对象。因为ui->poolGraphicsView已经变成了PoolView对象。
   */

   /*
   可视化任务的设计:
    1. waiting（等待状态）
    显示位置：任务队列区域
    显示方式：矩形节点 + 箭头连接
    效果：横向排列的队列，队头→队尾，箭头表示流动方向

    2. running（执行状态）
    显示位置：直接显示在线程方块上
    显示方式：线程方块里显示任务ID，方块变色（比如红色表示忙碌）
    效果：用户一眼就能看出哪个线程在执行哪个任务

    3. finished（完成状态）
    显示位置：已完成任务列表区域
    显示方式：用列表装着已完成任务的图形,显示完成任务ID+处理该任务的线程ID
    效果：按完成顺序排列，方便查看历史记录
   */

    void visualizeAll(const QList<ThreadVisualInfo>& threadInfos,
                      const QList<TaskVisualInfo>& waitingTasks,
                      const QList<TaskVisualInfo>& finishedTasks);


    void clear();
    // 设置任务ID到总耗时的映射, 用于绘制进度条 
    void setTaskIdToTotalTimeMs(const QMap<int, int>& taskIdToTotalTimeMs) {m_taskIdToTotalTimeMs = taskIdToTotalTimeMs;}
    // 设置当前调度策略,用于switch绘制任务的label
    void setCurrentPolicy(SchedulePolicy policy) {m_currentPolicy = policy;}
private:
    // 重写resizeEvent，在窗口大小变化时，根据快照重新绘制线程和任务,使每行显示的矩形数量自适应。
    void resizeEvent(QResizeEvent* event) override;
    // 绘制线程、等待任务、已完成任务,返回下一个可用的y坐标
    int drawThreads(const QList<ThreadVisualInfo>& threadInfos, int baseY);
    int drawWaitingTasks(const QList<TaskVisualInfo>& waitingTasks, int baseY);
    int drawFinishedTasks(const QList<TaskVisualInfo>& finishedTasks, int baseY);
    
private:
    QGraphicsScene* m_scene = nullptr;
    // 线程池快照：保存上一次绘制的线程信息，用于在窗口大小变化时重新绘制，不需要读线程池。
    QList<ThreadVisualInfo> m_lastThreadInfos;
    // 任务池快照
    QList<TaskVisualInfo> m_lastWaitingTasks;
    QList<TaskVisualInfo> m_lastFinishedTasks;
    QMap<int, int> m_taskIdToTotalTimeMs;
    SchedulePolicy m_currentPolicy; // 当前调度策略,用于switch绘制任务的label
    
};

#endif // POOLVIEW_H
