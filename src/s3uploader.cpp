#include "s3uploader.h"

#if ENABLE_S3_UPLOAD

#ifdef _MSC_VER
#pragma warning(disable: 4858)
#endif
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/PutObjectResult.h>
#include <QtConcurrent/QtConcurrentRun>
#include <sstream>

static Aws::SDKOptions g_awsOptions;

S3Uploader::S3Uploader(QObject *parent)
    : QObject(parent)
{
}

void S3Uploader::initAws()
{
    g_awsOptions.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Warn;
    Aws::InitAPI(g_awsOptions);
}

void S3Uploader::shutdownAws()
{
    Aws::ShutdownAPI(g_awsOptions);
}

void S3Uploader::setCredentials(const QString& accessKey, const QString& secretKey)
{
    m_accessKey = accessKey;
    m_secretKey = secretKey;
}

void S3Uploader::upload(const QString& objectKey, const QByteArray& data)
{
    QString ep = m_endpoint;
    QString bk = m_bucket;
    QString ak = m_accessKey;
    QString sk = m_secretKey;

    QtConcurrent::run([this, objectKey, data, ep, bk, ak, sk]() {
        Aws::Client::ClientConfiguration config;
        config.scheme = Aws::Http::Scheme::HTTP;
        config.endpointOverride = ep.toStdString();
        config.region = "us-east-1";
        config.verifySSL = false;

        Aws::Auth::AWSCredentials creds(ak.toStdString(), sk.toStdString());

        Aws::S3::S3Client client(creds, config,
            Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
            false);

        Aws::S3::Model::PutObjectRequest request;
        request.SetBucket(bk.toStdString());
        request.SetKey(objectKey.toStdString());

        auto body = std::make_shared<Aws::StringStream>(
            std::string(data.constData(), data.size())
        );
        request.SetBody(body);

        auto outcome = client.PutObject(request);
        bool ok = outcome.IsSuccess();
        QString err;
        if (!ok) {
            auto& error = outcome.GetError();
            err = QStringLiteral("[%1] %2")
                      .arg(QString::fromStdString(error.GetExceptionName()))
                      .arg(QString::fromStdString(error.GetMessage()));
        }

        QMetaObject::invokeMethod(this, [this, objectKey, ok, err]() {
            emit uploadFinished(objectKey, ok, err);
        }, Qt::QueuedConnection);
    });
}

#else  // !ENABLE_S3_UPLOAD

// 空桩 — 所有实现已在 s3uploader.h 中内联
// 此文件仅用于确保 Q_OBJECT / moc 正常生成

#endif // ENABLE_S3_UPLOAD
