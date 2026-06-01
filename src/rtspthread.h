#ifndef RTSPTHREAD_H
#define RTSPTHREAD_H

#include <QThread>
#include <QMutex>
#include <QImage>
#include <QWaitCondition>

// 前置声明 FFmpeg 核心结构体，避免在头文件中引入 C 头文件
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct AVPacket;

// RtspThread：RTSP 视频流拉取与解码线程
// 在独立线程中通过 FFmpeg 连接 RTSP 源、解码视频帧并转换为 QImage，
// 通过信号将帧数据传递给主线程的 VideoWidget 进行显示。
// 支持断线自动重连，通过条件变量控制流的开启与关闭。
class RtspThread : public QThread
{
    Q_OBJECT
public:
    explicit RtspThread(QObject *parent = nullptr);
    ~RtspThread() override;

    // 打开指定 URL 的 RTSP 流（非阻塞，实际连接在线程中异步进行）
    void openStream(const QString &url);

    // 关闭当前 RTSP 流，等待线程退出
    void closeStream();

signals:
    // 每解码一帧后发射，携带转换好的 QImage（RGB32 格式）
    void frameReady(const QImage &frame);

    // 流成功打开后发射
    void streamOpened();

    // 连接或解码过程发生错误时发射，携带错误描述信息
    void streamError(const QString &msg);

protected:
    // 线程主循环：等待打开请求 → 连接 RTSP → 解码循环 → 重连
    void run() override;

private:
    // 安全释放 FFmpeg 相关资源（解码器上下文、格式上下文、缩放上下文）
    void safeCleanup();

    QMutex m_mutex;             // 保护共享状态（m_url, m_stop, m_openRequested 等）
    QWaitCondition m_cond;      // 用于线程间同步，等待打开/关闭请求
    QString m_url;              // 当前 RTSP 流的 URL
    bool m_openRequested = false; // 是否有新的打开请求待处理
    bool m_stop = false;        // 通知线程停止运行

    // FFmpeg 相关资源指针
    AVFormatContext *m_fmtCtx = nullptr;  // 格式上下文（封装层）
    AVCodecContext  *m_decCtx = nullptr;  // 解码器上下文
    SwsContext      *m_swsCtx = nullptr;  // 像素格式/尺寸转换上下文
    uint8_t         *m_rgbBuf = nullptr;  // RGB 转换缓冲
    int m_videoStreamIdx = -1;            // 视频流索引
};

#endif // RTSPTHREAD_H
