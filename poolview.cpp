#include "poolview.h"
#include <QGraphicsTextItem>
#include <QPen>
#include <QBrush>
#include <QScrollBar>

PoolView::PoolView(QWidget* parent) : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(this); // 设置场景, 场景是绘图的容器, 会自动析构
    setScene(m_scene);
    // // 只允许上下滚动
    // setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setRenderHint(QPainter::Antialiasing);
    setAlignment(Qt::AlignTop | Qt::AlignLeft); // 左上角对齐
}

PoolView::~PoolView() {

}

/*
布局：让每个 drawXxx 函数都接收一个起始 y 坐标，绘制完后返回“下一个可用的 y 坐标”。
这样每一栏都能自适应高度，互不重叠。

    第一栏 waitingTaskQueue区域：
    - 圆角矩形
    - 颜色: 白底黑字
    - 按照队列的实际情况用箭头连接
    - 任务ID: 显示在圆角矩形上

    第二栏 poolView区域：
    - 线程矩形从左到右排满一行，自动换行，从上到下，依次排列
    - 线程矩形外观:
        1. 矩形
        2. 颜色（线程状态）: 0:空闲=绿色, 1:忙碌=红色, -1:退出=隐藏
        3. 线程ID: 显示在矩形上
        4. 线程正在执行的任务ID: 显示在矩形上

    第三栏 finishedTaskList区域：
    - 圆角矩形
    - 颜色: 灰底黑字
    - 任务ID: 显示在圆角矩形上
 */


void PoolView::visualizeAll(const QList<ThreadVisualInfo>& threadInfos,
                            const QList<TaskVisualInfo>& waitingTasks,
                            const QList<TaskVisualInfo>& finishedTasks) {
    m_lastThreadInfos = threadInfos;
    m_lastWaitingTasks = waitingTasks;
    m_lastFinishedTasks = finishedTasks;
    scene()->clear();

    int baseY = 0;
    baseY = drawWaitingTasks(waitingTasks, baseY);

    baseY = drawThreads(threadInfos, baseY);

    baseY = drawFinishedTasks(finishedTasks, baseY);

    scene()->setSceneRect(0, 0, viewport()->width(), baseY);

}

int PoolView::drawThreads(const QList<ThreadVisualInfo>& threadInfos, int baseY) {

    const int rectWidth = 40;
    const int rectHeight = 40;
    const int spacing = 15;

    
    int maxPerRow = (viewport()->width() + spacing) / (rectWidth + spacing);
    if (maxPerRow < 1) maxPerRow = 1;
    int leftMargin = (viewport()->width() - maxPerRow * rectWidth - (maxPerRow - 1) * spacing) / 2;

    int shownIndex = 0;
    for (const auto& info : threadInfos) {
        if (info.state == -1) continue;

        int row = shownIndex / maxPerRow;
        int col = shownIndex % maxPerRow;
        int x = col * (rectWidth + spacing) + leftMargin;
        int y = baseY + row * (rectHeight + spacing);

        QColor color = (info.state == 0) ? Qt::green : Qt::red;

        scene()->addRect(
            x, y, rectWidth, rectHeight,
            QPen(Qt::white, 2), QBrush(color)
            );
        // 忙碌线程：线程ID + # + 任务ID
        // 空闲线程：线程ID
        QString label = QString("T%1").arg(info.threadId);
        if (info.state == 1 && info.curTaskId != -1) {
            label += QString(" #%1").arg(info.curTaskId);
        }

        QGraphicsSimpleTextItem* text = scene()->addSimpleText(label);
        text->setBrush(Qt::black);
        QRectF textRect = text->boundingRect();
        text->setPos(
            x + (rectWidth - textRect.width()) / 2,
            y + (rectHeight - textRect.height()) / 2
            );

        ++shownIndex;
    }


    int totalRows = (shownIndex + maxPerRow - 1) / maxPerRow;
    int sceneHeight = totalRows * (rectHeight + spacing);
    // 后续放到visualizeAll中统一设置
    // scene()->setSceneRect(0, 0, viewWidth, baseY + sceneHeight);

    return baseY + sceneHeight;
}


int PoolView::drawWaitingTasks(const QList<TaskVisualInfo>& waitingTasks, int baseY) {
    const int nodeWidth = 30;
    const int nodeHeight = 30;
    const int spacing = 15;
    const int rowSpacing = 5;
    const int topSpacing = 5;
    const int radius = 10;


    int maxPerRow = (viewport()->width() + spacing) / (nodeWidth + spacing);
    if (maxPerRow < 1) maxPerRow = 1;
    int leftMargin = (viewport()->width() - maxPerRow * nodeWidth - (maxPerRow - 1) * spacing) / 2;



    // 箭头头部内联
    auto drawArrowHead = [&](const QPointF& from, const QPointF& to) {
        int arrowSize = 5;
        QPointF dir = to - from;
        double len = std::hypot(dir.x(), dir.y());
        if (len > 0) {
            dir /= len;
            QPointF ortho(-dir.y(), dir.x());
            QPolygonF arrowHead;
            arrowHead << to
                      << (to - dir * arrowSize + ortho * (arrowSize / 2))
                      << (to - dir * arrowSize - ortho * (arrowSize / 2));
            scene()->addPolygon(arrowHead, QPen(Qt::gray), QBrush(Qt::gray));
        }
    };

    int rowCount = (waitingTasks.size() + maxPerRow - 1) / maxPerRow;
    for (int i = 0; i < waitingTasks.size(); ++i) {
        int row = i / maxPerRow;
        int col = i % maxPerRow;
        int x = leftMargin + col * (nodeWidth + spacing);
        int y = baseY + topSpacing + row * (nodeHeight + rowSpacing);

        // 画节点
        QPainterPath path;
        path.addRoundedRect(x, y, nodeWidth, nodeHeight, radius, radius);
        scene()->addPath(path, QPen(Qt::gray, 2), QBrush(Qt::white));

        // 画任务ID
        QGraphicsSimpleTextItem* text = scene()->addSimpleText(QString::number(waitingTasks[i].taskId));
        text->setBrush(Qt::black);
        QRectF textRect = text->boundingRect();
        text->setPos(
            x + (nodeWidth - textRect.width()) / 2,
            y + (nodeHeight - textRect.height()) / 2
        );

        // 只在同一行画连线和箭头
        if (col > 0) {
            int prevX = leftMargin + (col - 1) * (nodeWidth + spacing);
            int prevY = y;
            QPointF prevCenterRight(prevX + nodeWidth, prevY + nodeHeight / 2);
            QPointF curCenterLeft(x, y + nodeHeight / 2);
            scene()->addLine(QLineF(prevCenterRight, curCenterLeft), QPen(Qt::gray, 2));
            drawArrowHead(prevCenterRight, curCenterLeft);
        }
    }

    int height = rowCount * (nodeHeight + rowSpacing) + topSpacing;
    return baseY + height;
}


int PoolView::drawFinishedTasks(const QList<TaskVisualInfo>& finishedTasks, int baseY) {
    return baseY;
}
/*
    窗口大小变化时：
    - 重新绘制线程，每行显示的矩形数量会变化。
    - 重新绘制任务，任务的动画信息会变化。

*/
void PoolView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    // 只要有快照就刷新
    if (!m_lastThreadInfos.isEmpty() ||
        !m_lastWaitingTasks.isEmpty() ||
        !m_lastFinishedTasks.isEmpty()) {
        visualizeAll(m_lastThreadInfos, m_lastWaitingTasks, m_lastFinishedTasks);
    }
}


void PoolView::clear() {
    m_lastThreadInfos.clear();
    m_lastWaitingTasks.clear();
    m_lastFinishedTasks.clear();
    scene()->clear();
}
