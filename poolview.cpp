#include "poolview.h"
#include <QGraphicsTextItem>
#include <QPen>
#include <QBrush>
#include <QScrollBar>
namespace {
    // 通用网格节点绘制
    void drawGrid(
        QGraphicsScene* scene,
        int itemCount,
        int itemWidth,
        int itemHeight,
        int spacing,
        int rowSpacing,
        int topSpacing,
        int baseY,
        std::function<void(int x, int y, int col, int idx)> drawNode
    ) {
        int viewWidth = scene->views().isEmpty() ? 800 : scene->views().first()->viewport()->width();
        int maxPerRow = (viewWidth + spacing) / (itemWidth + spacing);
        if (maxPerRow < 1) maxPerRow = 1;
        int leftMargin = (viewWidth - maxPerRow * itemWidth - (maxPerRow - 1) * spacing) / 2;
        if (leftMargin < 0) leftMargin = 0;

        for (int i = 0; i < itemCount; ++i) {
            int col = i % maxPerRow;
            int x = leftMargin + col * (itemWidth + spacing);
            int row = i / maxPerRow;
            int y = baseY + topSpacing + row * (itemHeight + rowSpacing);
            drawNode(x, y, col, i);
        }
    }
}

PoolView::PoolView(QWidget* parent) : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(this); // 设置场景, 场景是绘图的容器, 会自动析构
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setAlignment(Qt::AlignTop | Qt::AlignLeft); // 左上角对齐
}

PoolView::~PoolView() {}

/*
布局：让每个 drawXxx 函数都接收一个起始 y 坐标，绘制完后返回“下一个可用的 y 坐标”。
这样每一栏都能自适应高度，互不重叠。

    第一栏 waitingTaskQueue区域：
    - 圆角矩形
    - 颜色: 淡蓝底，深蓝边
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
    const int w = 40, h = 40, spacing = 10, rowSpacing = 10, topSpacing = 0;
    auto scenePtr = scene();

    drawGrid(scenePtr, threadInfos.size(), w, h, spacing, rowSpacing, topSpacing, baseY,
        [&](int x, int y, int /*col*/, int idx) {
            const auto& info = threadInfos[idx];
            if (info.state == -1) return;
            QColor color = (info.state == 0) ? Qt::green : Qt::red;
            scenePtr->addRect(x, y, w, h, QPen(Qt::white, 2), QBrush(color));
            QString label = QString("T%1").arg(info.threadId);
            if (info.state == 1 && info.curTaskId != -1)
                label += QString(" #%1").arg(info.curTaskId);
            auto* text = scenePtr->addSimpleText(label);
            text->setBrush(Qt::black);
            QRectF r = text->boundingRect();
            text->setPos(x + (w - r.width()) / 2, y + (h - r.height()) / 2);
        }
    );
    int viewWidth = scenePtr->views().isEmpty() ? 800 : scenePtr->views().first()->viewport()->width();
    int maxPerRow = (viewWidth + spacing) / (w + spacing);
    if (maxPerRow < 1) maxPerRow = 1;
    int rowCount = (threadInfos.size() + maxPerRow - 1) / maxPerRow;
    return baseY + rowCount * (h + rowSpacing) + topSpacing;
}

int PoolView::drawWaitingTasks(const QList<TaskVisualInfo>& waitingTasks, int baseY) {
    const int w = 30, h = 30, spacing = 15, rowSpacing = 5, topSpacing = 5, radius = 10;
    auto scenePtr = scene();

    drawGrid(scenePtr, waitingTasks.size(), w, h, spacing, rowSpacing, topSpacing, baseY,
        [&](int x, int y, int col, int idx) {
            QPainterPath path;
            path.addRoundedRect(x, y, w, h, radius, radius);
            scenePtr->addPath(path, QPen(QColor(100, 150, 220), 2), QBrush(QColor(200, 220, 255)));
            auto* text = scenePtr->addSimpleText(QString::number(waitingTasks[idx].taskId));
            text->setBrush(Qt::black);
            QRectF r = text->boundingRect();
            text->setPos(x + (w - r.width()) / 2, y + (h - r.height()) / 2);

            // 只在同一行画连线和箭头
            if (col > 0) {
                int prevX = x - (w + spacing);
                int prevY = y;
                QPointF prevCenterRight(prevX + w, prevY + h / 2);
                QPointF curCenterLeft(x, y + h / 2);
                scenePtr->addLine(QLineF(prevCenterRight, curCenterLeft), QPen(Qt::gray, 2));
                // 箭头
                int arrowSize = 5;
                QPointF dir = curCenterLeft - prevCenterRight;
                double len = std::hypot(dir.x(), dir.y());
                if (len > 0) {
                    dir /= len;
                    QPointF ortho(-dir.y(), dir.x());
                    QPolygonF arrowHead;
                    arrowHead << curCenterLeft
                              << (curCenterLeft - dir * arrowSize + ortho * (arrowSize / 2))
                              << (curCenterLeft - dir * arrowSize - ortho * (arrowSize / 2));
                    scenePtr->addPolygon(arrowHead, QPen(Qt::gray), QBrush(Qt::gray));
                }
            }
        }
    );
    int viewWidth = scenePtr->views().isEmpty() ? 800 : scenePtr->views().first()->viewport()->width();
    int maxPerRow = (viewWidth + spacing) / (w + spacing);
    if (maxPerRow < 1) maxPerRow = 1;
    int rowCount = (waitingTasks.size() + maxPerRow - 1) / maxPerRow;
    return baseY + rowCount * (h + rowSpacing) + topSpacing;
}
int PoolView::drawFinishedTasks(const QList<TaskVisualInfo>& finishedTasks, int baseY) {
    const int w = 35, h = 30, spacing = 8, rowSpacing = 8, topSpacing = 5, radius = 10;
    auto scenePtr = scene();

    drawGrid(scenePtr, finishedTasks.size(), w, h, spacing, rowSpacing, topSpacing, baseY,
        [&](int x, int y, int /*col*/, int idx) {
            QPainterPath path;
            path.addRoundedRect(x, y, w, h, radius, radius);
            scenePtr->addPath(path, QPen(Qt::gray, 2), QBrush(QColor(220, 220, 220)));
            QString label = QString("%1 T:%2").arg(finishedTasks[idx].taskId).arg(finishedTasks[idx].curThreadId);
            auto* text = scenePtr->addSimpleText(label);
            text->setBrush(Qt::black);
            QRectF r = text->boundingRect();
            text->setPos(x + (w - r.width()) / 2, y + (h - r.height()) / 2);
        }
    );
    int viewWidth = scenePtr->views().isEmpty() ? 800 : scenePtr->views().first()->viewport()->width();
    int maxPerRow = (viewWidth + spacing) / (w + spacing);
    if (maxPerRow < 1) maxPerRow = 1;
    int rowCount = (finishedTasks.size() + maxPerRow - 1) / maxPerRow;
    return baseY + rowCount * (h + rowSpacing) + topSpacing;
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
