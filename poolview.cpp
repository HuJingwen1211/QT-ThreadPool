#include "poolview.h"
#include <QGraphicsTextItem>
#include <QPen>
#include <QBrush>

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
    可视化线程外观:
    1. 矩形
    2. 颜色: 0:空闲=绿色, 1:忙碌=红色, -1:退出=隐藏
    3. 线程ID: 显示在矩形上

    poolView区域：
    - 线程矩形从左到右排满一行，自动换行，从上到下，依次排列

 */



void PoolView::visualizeThreads(const QList<ThreadVisualInfo>& threadInfos) {
    m_lastThreadInfos = threadInfos;
    scene()->clear();
    
    const int rectWidth = 40;
    const int rectHeight = 40;
    const int spacing = 15;

    int viewWidth = viewport()->width();
    int maxPerRow = (viewWidth + spacing) / (rectWidth + spacing);
    if (maxPerRow < 1) maxPerRow = 1;


    qDebug() << "viewWidth = " << viewWidth << ", maxPerRow = " << maxPerRow;

    int leftMargin = (viewWidth - maxPerRow * rectWidth - (maxPerRow - 1) * spacing) / 2;

    int shownIndex = 0;
    for (const auto& info : threadInfos) {
        if (info.state == -1) continue;

        int row = shownIndex / maxPerRow;
        int col = shownIndex % maxPerRow;
        int x = col * (rectWidth + spacing) + leftMargin;
        int y = row * (rectHeight + spacing);

        QColor color = (info.state == 0) ? Qt::green : Qt::red;

        scene()->addRect(
            x, y, rectWidth, rectHeight,
            QPen(Qt::white, 2), QBrush(color)
            );

        QGraphicsSimpleTextItem* text = scene()->addSimpleText(QString::number(info.threadId));
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
    scene()->setSceneRect(0, 0, viewWidth, sceneHeight);

}

void PoolView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (!m_lastThreadInfos.isEmpty()) {
        visualizeThreads(m_lastThreadInfos);
    }
}