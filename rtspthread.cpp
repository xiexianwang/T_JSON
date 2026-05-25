#include "rtspthread.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

#include <QDebug>

RtspThread::RtspThread(QObject *parent)
    : QThread(parent)
{
}

RtspThread::~RtspThread()
{
    closeStream();
    quit();
    wait(3000);
}

void RtspThread::openStream(const QString &url)
{
    QMutexLocker lock(&m_mutex);
    m_url = url;
    m_openRequested = true;
    m_cond.wakeOne();
    if (!isRunning())
        start();
}

void RtspThread::closeStream()
{
    {
        QMutexLocker lock(&m_mutex);
        m_stop = true;
        m_cond.wakeOne();
    }
    wait(3000);
    safeCleanup();
}

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
    m_videoStreamIdx = -1;
}

void RtspThread::run()
{
    AVPacket *pkt = av_packet_alloc();
    AVFrame  *decoded = av_frame_alloc();
    AVFrame  *rgb = av_frame_alloc();
    uint8_t  *rgbBuf = nullptr;

    while (!m_stop) {
        // ── Wait for open request or stop ──
        {
            QMutexLocker lock(&m_mutex);
            if (!m_openRequested && !m_stop)
                m_cond.wait(&m_mutex);
            if (m_stop) break;

            m_openRequested = false;
            m_url = m_url;
        }

        QByteArray url8 = m_url.toUtf8();
        AVDictionary *opts = nullptr;
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&opts, "stimeout", "5000000", 0);  // 5 s timeout
        av_dict_set(&opts, "buffer_size", "1048576", 0);
        av_dict_set(&opts, "max_delay", "500000", 0);

        // Open
        int ret = avformat_open_input(&m_fmtCtx, url8.constData(), nullptr, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            char err[128] = {};
            av_strerror(ret, err, sizeof(err));
            emit streamError(QString::fromUtf8("打开RTSP失败: %1").arg(err));
            continue;
        }

        ret = avformat_find_stream_info(m_fmtCtx, nullptr);
        if (ret < 0) {
            emit streamError("查找流信息失败");
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        // Find video stream
        m_videoStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (m_videoStreamIdx < 0) {
            emit streamError("未找到视频流");
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        const AVStream *vs = m_fmtCtx->streams[m_videoStreamIdx];
        const AVCodec *codec = avcodec_find_decoder(vs->codecpar->codec_id);
        if (!codec) {
            emit streamError("不支持该视频编码格式");
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        m_decCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(m_decCtx, vs->codecpar);
        ret = avcodec_open2(m_decCtx, codec, nullptr);
        if (ret < 0) {
            emit streamError("打开解码器失败");
            avcodec_free_context(&m_decCtx);
            avformat_close_input(&m_fmtCtx);
            continue;
        }

        int w = m_decCtx->width;
        int h = m_decCtx->height;
        m_swsCtx = sws_getContext(w, h, m_decCtx->pix_fmt,
                                 w, h, AV_PIX_FMT_BGR32,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);

        int rgbLinesize = w * 4;
        rgbBuf = (uint8_t*)av_malloc(rgbLinesize * h);
        av_image_fill_arrays(rgb->data, rgb->linesize, rgbBuf,
                             AV_PIX_FMT_BGR32, w, h, 1);

        emit streamOpened();

        // ── Decode loop ──
        while (!m_stop) {
            ret = av_read_frame(m_fmtCtx, pkt);
            if (ret < 0) {
                // Stream ended or error — try reconnecting
                break;
            }

            if (pkt->stream_index == m_videoStreamIdx) {
                ret = avcodec_send_packet(m_decCtx, pkt);
                if (ret < 0) continue;

                while (ret >= 0) {
                    ret = avcodec_receive_frame(m_decCtx, decoded);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0) break;

                    sws_scale(m_swsCtx,
                              decoded->data, decoded->linesize,
                              0, h,
                              rgb->data, rgb->linesize);

                    QImage img(rgbBuf, w, h, rgbLinesize, QImage::Format_RGB32);
                    emit frameReady(img.copy());
                }
            }
            av_packet_unref(pkt);
        }

        // Cleanup for reconnect
        safeCleanup();
    }

    av_freep(&rgbBuf);
    av_frame_free(&rgb);
    av_frame_free(&decoded);
    av_packet_free(&pkt);
    safeCleanup();
}
