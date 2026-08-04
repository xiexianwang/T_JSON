#ifndef S3UPLOADER_H
#define S3UPLOADER_H

#include <QObject>
#include <QByteArray>
#include <QString>

#if ENABLE_S3_UPLOAD

class S3Uploader : public QObject
{
    Q_OBJECT
public:
    explicit S3Uploader(QObject *parent = nullptr);

    static void initAws();
    static void shutdownAws();

    void setEndpoint(const QString& endpoint) { m_endpoint = endpoint; }
    void setBucket(const QString& bucket) { m_bucket = bucket; }
    void setCredentials(const QString& accessKey, const QString& secretKey);

    void upload(const QString& objectKey, const QByteArray& data);

signals:
    void uploadFinished(const QString& objectKey, bool success, const QString& errorMsg);

private:
    QString m_endpoint;
    QString m_bucket;
    QString m_accessKey;
    QString m_secretKey;
};

#else  // !ENABLE_S3_UPLOAD → 空桩，所有操作无效果

class S3Uploader : public QObject
{
    Q_OBJECT
public:
    explicit S3Uploader(QObject *parent = nullptr) : QObject(parent) {}
    static void initAws() {}
    static void shutdownAws() {}
    void setEndpoint(const QString&) {}
    void setBucket(const QString&) {}
    void setCredentials(const QString&, const QString&) {}
    void upload(const QString&, const QByteArray&) {}

signals:
    void uploadFinished(const QString& objectKey, bool success, const QString& errorMsg);
};

#endif // ENABLE_S3_UPLOAD

#endif // S3UPLOADER_H
