#ifndef RTSPTHREAD_H
#define RTSPTHREAD_H

#include <QThread>
#include <QMutex>
#include <QImage>
#include <QWaitCondition>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct AVPacket;

class RtspThread : public QThread
{
    Q_OBJECT
public:
    explicit RtspThread(QObject *parent = nullptr);
    ~RtspThread() override;

    void openStream(const QString &url);
    void closeStream();

signals:
    void frameReady(const QImage &frame);
    void streamOpened();
    void streamError(const QString &msg);

protected:
    void run() override;

private:
    void safeCleanup();

    QMutex m_mutex;
    QWaitCondition m_cond;
    QString m_url;
    bool m_openRequested = false;
    bool m_stop = false;

    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext  *m_decCtx = nullptr;
    SwsContext      *m_swsCtx = nullptr;
    int m_videoStreamIdx = -1;
};

#endif // RTSPTHREAD_H
