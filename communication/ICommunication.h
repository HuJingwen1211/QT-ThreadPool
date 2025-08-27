#ifndef ICOMMUNICATION_H
#define ICOMMUNICATION_H

#include <QJsonObject>

// 通信接口基类
class ICommunication
{
public:
    virtual ~ICommunication() = default;
    virtual bool send(const QJsonObject& data) = 0;
    virtual QJsonObject receive() = 0;
};

#endif // ICOMMUNICATION_H
