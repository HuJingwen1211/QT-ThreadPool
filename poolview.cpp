#include "poolview.h"
#include <QGraphicsTextItem>
#include <QPen>
#include <QBrush>
#include <QScrollBar>
#include <QTime>
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

/*
第一栏 waitingTaskQueue区域：
- 圆角矩形
- 颜色: 红色边，淡红底
- 按照队列的实际情况用箭头连接
- 任务ID: 显示在圆角矩形上
 */
int PoolView::drawWaitingTasks(const QList<TaskVisualInfo>& waitingTasks, int baseY) {
    const int w = 45, h = 30, spacing = 15, rowSpacing = 5, topSpacing = 5, radius = 10;
    auto scenePtr = scene();
    drawGrid(scenePtr, waitingTasks.size(), w, h, spacing, rowSpacing, topSpacing, baseY,
        [&](int x, int y, int col, int idx) {
            QPainterPath path;
            path.addRoundedRect(x, y, w, h, radius, radius);
            // 画圆角矩形，红色边，淡红底
            scenePtr->addPath(path, QPen(QColor(220, 60, 60), 2), QBrush(QColor(255, 220, 220)));
            // auto* text = scenePtr->addSimpleText(QString::number(waitingTasks[idx].taskId));
            // 显示任务ID和总耗时s
            int totalTimeMs = waitingTasks[idx].totalTimeMs;
            double seconds = totalTimeMs / 1000.0;
            QString label;
            switch (m_currentPolicy) {
                case SchedulePolicy::FIFO:
                case SchedulePolicy::LIFO:
                    label = QString::number(waitingTasks[idx].taskId);
                    break;
                case SchedulePolicy::SJF:
                case SchedulePolicy::LJF:
                    label = QString("%1(%2s)").arg(waitingTasks[idx].taskId).arg(seconds, 0, 'f', 1);
                    break;
                case SchedulePolicy::PRIO:
                    label = QString("%1(★%2)").arg(waitingTasks[idx].taskId).arg(waitingTasks[idx].priority);
                    break;
                case SchedulePolicy::HRRN: {
                    // 计算响应比
                    int currentTime = QTime::currentTime().msecsSinceStartOfDay();
                    int waitTime = currentTime - waitingTasks[idx].arrivalTimestampMs;
                    double responseRatio = (waitTime + waitingTasks[idx].totalTimeMs) / (double)waitingTasks[idx].totalTimeMs;

                    label = QString("%1(hr%2)").arg(waitingTasks[idx].taskId).arg(responseRatio, 0, 'f', 1);
                    break;
                }
                default:
                    label = QString::number(waitingTasks[idx].taskId);
            }
            auto* text = scenePtr->addSimpleText(label);

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
/*
第二栏 poolView区域：
- 线程矩形从左到右排满一行，自动换行，从上到下，依次排列
- 线程矩形外观:
    1. 矩形
    2. 颜色（线程状态）: 0:空闲=绿色, 1:忙碌=红色, -1:退出=隐藏
    3. 线程ID: 显示在矩形上
    4. 线程正在执行的任务ID: 显示在矩形上
    5. 忙碌线程：矩形左侧“已完成部分”为绿色，右侧“未完成部分”为红色，进度条就是整个矩形本身。
    6. 空闲线程：矩形全绿。

*/
int PoolView::drawThreads(const QList<ThreadVisualInfo>& threadInfos, int baseY) {
    const int w = 40, h = 40, spacing = 10, rowSpacing = 10, topSpacing = 0;
    auto scenePtr = scene();

    drawGrid(scenePtr, threadInfos.size(), w, h, spacing, rowSpacing, topSpacing, baseY,
        [&](int x, int y, int /*col*/, int idx) {
            const auto& info = threadInfos[idx];
            if (info.state == -1) return;

            QColor idleColor = Qt::green;
            QColor busyDoneColor = Qt::green;
            QColor busyTodoColor = Qt::red;

            if (info.state == 0) {
                // 空闲线程，全部绿色
                scenePtr->addRect(x, y, w, h, QPen(Qt::white, 2), QBrush(idleColor));
            } else if (info.state == 1 && info.curTaskId != -1) {
                // 用map获取totalTimeMs, 用于绘制进度条
                int totalTimeMs = m_taskIdToTotalTimeMs.value(info.curTaskId);
                // 忙碌线程，左侧绿色（已完成），右侧红色（未完成）
                double percent = qBound(0.0, double(info.curTimeMs) / totalTimeMs, 1.0);
                int doneWidth = int(w * percent);
                int todoWidth = w - doneWidth;
                // 先画已完成部分（绿色）
                if (doneWidth > 0) {
                    scenePtr->addRect(x, y, doneWidth, h, QPen(Qt::NoPen), QBrush(busyDoneColor));
                }
                // 再画未完成部分（红色）
                if (todoWidth > 0) {
                    scenePtr->addRect(x + doneWidth, y, todoWidth, h, QPen(Qt::NoPen), QBrush(busyTodoColor));
                }
                // 画边框
                scenePtr->addRect(x, y, w, h, QPen(Qt::white, 2));
            } else {
                // 其他情况（如异常），用灰色
                scenePtr->addRect(x, y, w, h, QPen(Qt::white, 2), QBrush(Qt::gray));
            }

            // 画文字
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


/*    
第三栏 finishedTaskList区域：
- 圆角矩形
- 颜色: 绿色边，淡绿底
- 任务ID: 显示在圆角矩形上
*/
int PoolView::drawFinishedTasks(const QList<TaskVisualInfo>& finishedTasks, int baseY) {
    const int w = 45, h = 30, spacing = 8, rowSpacing = 8, topSpacing = 5, radius = 10;
    auto scenePtr = scene();

    drawGrid(scenePtr, finishedTasks.size(), w, h, spacing, rowSpacing, topSpacing, baseY,
        [&](int x, int y, int /*col*/, int idx) {
            QPainterPath path;
            path.addRoundedRect(x, y, w, h, radius, radius);
            // 画圆角矩形，绿色边，淡绿底
            scenePtr->addPath(path, QPen(QColor(60, 180, 60), 2), QBrush(QColor(220, 255, 220)));
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
