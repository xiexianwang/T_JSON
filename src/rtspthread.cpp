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

// 构造函数：仅初始化基类，实际资源在线程启动后分配
RtspThread::RtspThread(QObject *parent)
    : QThread(parent)
{
    setObjectName(QStringLiteral("RtspThread"));
}

// 析构函数：确保流被关闭、线程退出并等待其结束
RtspThread::~RtspThread()
{
    qDebug() << "RtspThread::~RtspThread() called!";
    closeStream();
    if (!wait(3000)) {
        qDebug() << "RtspThread failed to finish within 3 seconds, forcibly terminating!";
        terminate();
        wait(1000);
    }
}

// 打开 RTSP 流（非阻塞接口）
// 设置 URL 和打开请求标志，唤醒等待中的线程；若线程未启动则启动之
void RtspThread::openStream(const QString &url)
{
    QMutexLocker lock(&m_mutex);
    m_stop = false;
    m_url = url;
    m_openRequested = true;
    m_cond.wakeOne();
    if (!isRunning())
        start();
}

// 关闭 RTSP 流
// 设置停止标志并唤醒线程，不阻塞等待（由析构函数统一 wait）
void RtspThread::closeStream()
{
    {
        QMutexLocker lock(&m_mutex);
        m_stop = true;
        m_cond.wakeOne();
    }
}

// 安全释放所有 FFmpeg 资源
// 释放顺序：缩放上下文 → RGB 缓冲 → 解码器上下文 → 格式上下文
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
// 流程：等待打开请求 → 连接 RTSP → 查找视频流 → 初始化解码器 →
//       循环读取/解码/发送帧 → 断开时自动重连
void RtspThread::run()
{
    // 预先分配 FFmpeg 资源，避免在循环中反复分配/释放
    AVPacket *pkt = av_packet_alloc();
    AVFrame  *decoded = av_frame_alloc();
    AVFrame  *rgb = av_frame_alloc();

    while (!m_stop) {
        // ── 等待外部通过 openStream() 发起的打开请求或停止信号 ──
        {
            QMutexLocker lock(&m_mutex);
            if (!m_openRequested && !m_stop)
                m_cond.wait(&m_mutex);
            if (m_stop) break;

            m_openRequested = false;
        }

        QByteArray url8 = m_url.toUtf8();
        AVDictionary *opts = nullptr;
        // 设置 RTSP 传输层为 TCP（更稳定），2 秒超时便于快速响应停止信号，1 MB 缓冲区
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&opts, "stimeout", "2000000", 0);  // 2 秒超时
        av_dict_set(&opts, "buffer_size", "1048576", 0);
        av_dict_set(&opts, "max_delay", "500000", 0);

        // ── 打开 RTSP 连接 ──
        int ret = avformat_open_input(&m_fmtCtx, url8.constData(), nullptr, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            char err[128] = {};
            av_strerror(ret, err, sizeof(err));
            emit streamError(QString::fromUtf8("打开RTSP失败: %1").arg(err));
            continue;  // 连接失败则重试
        }

        // ── 读取流信息 ──
        ret = avformat_find_stream_info(m_fmtCtx, nullptr);
        if (ret < 0) {
            emit streamError("查找流信息失败");
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        // ── 查找视频流索引 ──
        m_videoStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (m_videoStreamIdx < 0) {
            emit streamError("未找到视频流");
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        // ── 查找并初始化解码器 ──
        const AVStream *vs = m_fmtCtx->streams[m_videoStreamIdx];
        const AVCodec *codec = avcodec_find_decoder(vs->codecpar->codec_id);
        if (!codec) {
            emit streamError("不支持该视频编码格式");
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        m_decCtx = avcodec_alloc_context3(codec);
        if (!m_decCtx) {
            emit streamError("分配解码器失败");
            avformat_close_input(&m_fmtCtx);
            continue;
        }
        avcodec_parameters_to_context(m_decCtx, vs->codecpar);
        ret = avcodec_open2(m_decCtx, codec, nullptr);
        if (ret < 0) {
            emit streamError("打开解码器失败");
            avcodec_free_context(&m_decCtx);
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        // ── 准备像素格式转换（原始格式 → RGB32）──
        int64_t w64 = m_decCtx->width;
        int64_t h64 = m_decCtx->height;
        constexpr int64_t kMaxDim = 16384;
        if (w64 <= 0 || h64 <= 0 || w64 > kMaxDim || h64 > kMaxDim) {
            emit streamError(QString("异常分辨率 %1x%2").arg(w64).arg(h64));
            avcodec_free_context(&m_decCtx);
            avformat_close_input(&m_fmtCtx);
            continue;
        }
        int w = static_cast<int>(w64);
        int h = static_cast<int>(h64);
        // 用 FFmpeg 的对齐感知函数获取真实缓冲大小（w*4*h 可能因 SIMD 对齐而不足）
        size_t rgbBufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB32, w, h, 1);
        if (rgbBufSize == 0 || rgbBufSize > 64u * 1024 * 1024) {
            emit streamError(QString("RGB 缓冲大小异常: %1").arg(rgbBufSize));
            avcodec_free_context(&m_decCtx);
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        m_swsCtx = sws_getContext(w, h, m_decCtx->pix_fmt,
                                 w, h, AV_PIX_FMT_RGB32,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!m_swsCtx) {
            emit streamError("初始化像素转换器失败");
            avcodec_free_context(&m_decCtx);
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        av_freep(&m_rgbBuf);
        m_rgbBuf = (uint8_t*)av_malloc(rgbBufSize);
        if (!m_rgbBuf) {
            emit streamError("分配 RGB 缓冲失败");
            sws_freeContext(m_swsCtx); m_swsCtx = nullptr;
            avcodec_free_context(&m_decCtx);
            avformat_close_input(&m_fmtCtx);
            continue;
        }
        av_image_fill_arrays(rgb->data, rgb->linesize, m_rgbBuf,
                             AV_PIX_FMT_RGB32, w, h, 1);

        // 通知外部流已成功打开
        emit streamOpened();

        // ── 解码循环：持续读取帧，直到流结束或收到停止信号 ──
        while (!m_stop) {
            ret = av_read_frame(m_fmtCtx, pkt);
            if (ret < 0) {
                // 流结束或网络错误，跳出内层循环以触发重连
                break;
            }

            // 仅处理视频流的数据包
            if (pkt->stream_index == m_videoStreamIdx) {
                ret = avcodec_send_packet(m_decCtx, pkt);
                if (ret < 0) continue;

                // 从解码器取出所有可用帧（可能一包产生多帧）
                while (ret >= 0) {
                    ret = avcodec_receive_frame(m_decCtx, decoded);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0) break;

                    // 将解码后的帧从原始像素格式转换为 RGB32
                    int scaleRet = sws_scale(m_swsCtx,
                                              decoded->data, decoded->linesize,
                                              0, h,
                                              rgb->data, rgb->linesize);
                    if (scaleRet <= 0) continue;

                    // 构造 QImage（共享 m_rgbBuf 数据）后深拷贝一份发送出去
                    // 使用 rgb->linesize[0]（FFmpeg 实际 stride，含对齐）而非 w*4
                    if (w > 0 && h > 0 && m_rgbBuf && rgb->linesize[0] > 0) {
                        QImage img(m_rgbBuf, w, h, rgb->linesize[0], QImage::Format_RGB32);
                        emit frameReady(img.copy());
                    }
                }
            }
            av_packet_unref(pkt);
        }

        // 断开连接后清理资源，准备下一次重连
        safeCleanup();
    }

    // 线程退出前释放所有持久分配的 FFmpeg 资源
    av_frame_free(&rgb);
    av_frame_free(&decoded);
    av_packet_free(&pkt);
    safeCleanup();
}
