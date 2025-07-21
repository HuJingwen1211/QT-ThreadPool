#ifndef POOLVIEW_H
#define POOLVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include "visualinfo.h"

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
    void visualizeThreads(const QList<ThreadVisualInfo>& threadInfos);
    

private:
    QGraphicsScene* m_scene = nullptr;
    QList<ThreadVisualInfo> m_lastThreadInfos;

    // 重写resizeEvent，在窗口大小变化时，重新绘制线程。
    // 窗口大小变化时，每行显示的矩形数量会变化。
    void resizeEvent(QResizeEvent* event) override;
};

#endif // POOLVIEW_H
