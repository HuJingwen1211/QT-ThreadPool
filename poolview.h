#ifndef POOLVIEW_H
#define POOLVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include "threadpool.h"
class PoolView : public QGraphicsView
{
    Q_OBJECT
public:
    PoolView(QWidget* parent = nullptr);
    ~PoolView();

private:
};

#endif // POOLVIEW_H
