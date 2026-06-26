#include "rtspthread.h"

// FFmpeg 为 C 接口，需用 extern "C" 包裹
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

#include <QDebug>

// ── FFmpeg 中断回调 ──
// 当 m_stop=true 时返回 1，让 avformat_open_input / av_read_frame 立即返回错误
// 解决线程卡在阻塞调用中无法及时退出的问题
int RtspThread::interruptCallback(void *opaque)
{
    auto *self = static_cast<RtspThread*>(opaque);
    return self->m_stop ? 1 : 0;
}

// 构造函数：仅初始化基类，实际资源在线程启动后分配
RtspThread::RtspThread(QObject *parent)
    : QThread(parent)
{
    setObjectName(QStringLiteral("RtspThread"));
}

// 析构函数：确保流被关闭、线程退出并等待其结束
RtspThread::~RtspThread()
{
    closeStream();
    if (!wait(2000)) {
        qDebug() << "RtspThread::~RtspThread() - thread timeout, force stop";
    }
}

// 打开 RTSP 流（非阻塞接口）
// 设置 URL 和打开请求标志，唤醒等待中的线程；若线程未启动则启动之
void RtspThread::openStream(const QString &url)
{
    {
        QMutexLocker lock(&m_mutex);
        m_stop = false;
        m_openRequested = true;
        m_url = url;
        m_autoReconnect = true;
        m_retryCount = 0;
        m_currentDelay = 2000;
        m_connecting = false;
        m_cond.wakeOne();
    }

    // 等待旧线程退出（interrupt callback 会使其快速返回）
    if (isRunning()) {
        qDebug() << "RtspThread::openStream - waiting for old thread...";
        if (!wait(2000)) {
            qDebug() << "RtspThread::openStream - old thread stuck, skip";
        }
    }

    if (!isRunning()) {
        m_connecting = true;
        start();
    }
}

// 关闭 RTSP 流（非阻塞）
// 设置停止标志，interrupt callback 会让 FFmpeg 阻塞调用快速返回
void RtspThread::closeStream()
{
    QMutexLocker lock(&m_mutex);
    m_stop = true;
    m_autoReconnect = false;
    m_openRequested = false;
    m_connecting = false;
    m_cond.wakeOne();
}

// 安全释放所有 FFmpeg 资源
void RtspThread::safeCleanup()
{
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_decCtx) {
        avcodec_free_context(&m_decCtx);
    }
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
    }
    av_freep(&m_rgbBuf);
    m_videoStreamIdx = -1;
}

// 线程主循环
void RtspThread::run()
{
    AVPacket *pkt = av_packet_alloc();
    AVFrame  *decoded = av_frame_alloc();
    AVFrame  *rgb = av_frame_alloc();

    while (!m_stop) {
        // 等待打开请求或自动重连
        {
            QMutexLocker lock(&m_mutex);
            if (!m_openRequested && !m_stop) {
                if (m_autoReconnect && !m_url.isEmpty()) {
                    if (m_retryCount > 0) {
                        m_currentDelay = qMin(m_currentDelay * 2, 60000);
                    }
                    m_retryCount++;
                    qDebug() << "RtspThread - auto reconnect attempt" << m_retryCount
                             << "delay" << m_currentDelay << "ms";
                    m_cond.wait(&m_mutex, m_currentDelay);
                } else if (!m_url.isEmpty()) {
                    m_cond.wait(&m_mutex, 2000);
                } else {
                    m_cond.wait(&m_mutex);
                }
            }
            if (m_stop) break;

            bool wasRequested = m_openRequested;
            m_openRequested = false;
            if (wasRequested) {
                m_retryCount = 0;
                m_currentDelay = 2000;
            }
        }

        QByteArray url8 = m_url.toUtf8();
        AVDictionary *opts = nullptr;
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&opts, "stimeout", "2000000", 0);
        av_dict_set(&opts, "buffer_size", "1048576", 0);
        av_dict_set(&opts, "max_delay", "500000", 0);

        // ── 分配格式上下文并设置中断回调 ──
        // 必须先 alloc 再设 callback，否则 avformat_open_input 内部分配的 context 无 callback
        m_fmtCtx = avformat_alloc_context();
        if (m_fmtCtx) {
            m_fmtCtx->interrupt_callback.callback = interruptCallback;
            m_fmtCtx->interrupt_callback.opaque = this;
        }

        // ── 打开 RTSP 连接 ──
        int ret = avformat_open_input(&m_fmtCtx, url8.constData(), nullptr, &opts);
        av_dict_free(&opts);

        if (ret < 0) {
            char err[128] = {};
            av_strerror(ret, err, sizeof(err));
            qDebug() << "RtspThread - open failed:" << err;
            safeCleanup();
            {
                QMutexLocker lock(&m_mutex);
                if (m_stop) break;
            }
            emit streamError(QString::fromUtf8("打开RTSP失败: %1").arg(err));
            continue;
        }

        {
            QMutexLocker lock(&m_mutex);
            m_connecting = false;
        }

        // ── 读取流信息 ──
        ret = avformat_find_stream_info(m_fmtCtx, nullptr);
        if (ret < 0) {
            emit streamError("查找流信息失败");
            safeCleanup();
            continue;
        }

        // ── 查找视频流索引 ──
        m_videoStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (m_videoStreamIdx < 0) {
            emit streamError("未找到视频流");
            safeCleanup();
            continue;
        }

        // ── 查找并初始化解码器 ──
        const AVStream *vs = m_fmtCtx->streams[m_videoStreamIdx];
        const AVCodec *codec = avcodec_find_decoder(vs->codecpar->codec_id);
        if (!codec) {
            emit streamError("不支持该视频编码格式");
            safeCleanup();
            continue;
        }

        m_decCtx = avcodec_alloc_context3(codec);
        if (!m_decCtx) {
            emit streamError("分配解码器失败");
            safeCleanup();
            continue;
        }
        avcodec_parameters_to_context(m_decCtx, vs->codecpar);
        ret = avcodec_open2(m_decCtx, codec, nullptr);
        if (ret < 0) {
            emit streamError("打开解码器失败");
            safeCleanup();
            continue;
        }

        // ── 准备像素格式转换 ──
        int w = m_decCtx->width;
        int h = m_decCtx->height;
        if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
            emit streamError(QString("异常分辨率 %1x%2").arg(w).arg(h));
            safeCleanup();
            continue;
        }

        size_t rgbBufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB32, w, h, 1);
        if (rgbBufSize == 0 || rgbBufSize > 64u * 1024 * 1024) {
            emit streamError(QString("RGB 缓冲大小异常: %1").arg(rgbBufSize));
            safeCleanup();
            continue;
        }

        m_swsCtx = sws_getContext(w, h, m_decCtx->pix_fmt,
                                 w, h, AV_PIX_FMT_RGB32,
                                 SWS_POINT, nullptr, nullptr, nullptr);
        if (!m_swsCtx) {
            emit streamError("初始化像素转换器失败");
            safeCleanup();
            continue;
        }

        av_freep(&m_rgbBuf);
        m_rgbBuf = (uint8_t*)av_malloc(rgbBufSize);
        if (!m_rgbBuf) {
            emit streamError("分配 RGB 缓冲失败");
            safeCleanup();
            continue;
        }
        av_image_fill_arrays(rgb->data, rgb->linesize, m_rgbBuf,
                             AV_PIX_FMT_RGB32, w, h, 1);

        qDebug() << "RtspThread - stream opened successfully";
        // 检查是否已被要求停止，避免关闭后还触发 streamOpened
        {
            QMutexLocker lock(&m_mutex);
            if (m_stop) { safeCleanup(); continue; }
        }
        emit streamOpened();

        // ── 解码循环 ──
        while (!m_stop) {
            ret = av_read_frame(m_fmtCtx, pkt);
            if (ret < 0) {
                qDebug() << "RtspThread - read frame error:" << ret;
                break;
            }

            if (pkt->stream_index == m_videoStreamIdx) {
                ret = avcodec_send_packet(m_decCtx, pkt);
                if (ret < 0) { av_packet_unref(pkt); continue; }

                while (ret >= 0) {
                    ret = avcodec_receive_frame(m_decCtx, decoded);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                    if (ret < 0) break;

                    if (sws_scale(m_swsCtx, decoded->data, decoded->linesize,
                                  0, h, rgb->data, rgb->linesize) > 0) {
                        // 检查是否已被要求停止，避免关闭后还发帧导致不黑屏
                        if (!m_stop && w > 0 && h > 0 && m_rgbBuf && rgb->linesize[0] > 0) {
                            QImage img(m_rgbBuf, w, h, rgb->linesize[0], QImage::Format_RGB32);
                            emit frameReady(img.copy());
                        }
                    }
                }
            }
            av_packet_unref(pkt);
        }

        safeCleanup();

        if (m_stop) break;

        if (m_autoReconnect) {
            qDebug() << "RtspThread - stream ended, will auto reconnect";
            emit streamError(QString());
        }
    }

    av_frame_free(&rgb);
    av_frame_free(&decoded);
    av_packet_free(&pkt);
    safeCleanup();

    qDebug() << "RtspThread - thread exited";
}
