#include "filecommunication.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <QFile>

FileCommunication::FileCommunication(const QString& filePath)
: m_filePath(filePath) {}

bool FileCommunication::send(const QJsonObject& data)
{
    QFile file(m_filePath);
    // 以只写+文本模式打开文件
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file for writing:" << m_filePath;
        return false;
    }
    
    // 添加时间戳
    QJsonObject dataWithTimestamp = data;
    dataWithTimestamp["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss");
    
    QJsonDocument doc(dataWithTimestamp);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented);
    
    qint64 bytesWritten = file.write(jsonData);
    file.close();
    
    if (bytesWritten == -1) {
        qDebug() << "Failed to write to file:" << m_filePath;
        return false;
    }
    
    return true;
}

QJsonObject FileCommunication::receive()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file for reading:" << m_filePath;
        return QJsonObject();
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    if (jsonData.isEmpty()) {
        qDebug() << "File is empty:" << m_filePath;
        return QJsonObject();
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString();
        return QJsonObject();
    }
    
    return doc.object();
}
