#ifndef FILECOMMUNICATION_H
#define FILECOMMUNICATION_H

#include "ICommunication.h"
class FileCommunication : public ICommunication
{
public:
    FileCommunication(const QString& filePath = "");
    // 不需要析构函数。当用基类指针指向子类对象时，会自动调用基类的析构函数

    bool send(const QJsonObject& data) override;
    QJsonObject receive() override;

private:
    QString m_filePath;

};

#endif // FILECOMMUNICATION_H
