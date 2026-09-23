#include "rtspthread.h"
#include "infrastructure/rtspbackoff.h"
#include "infrastructure/streamwatchdog.h"

// FFmpeg 为 C 接口，需用 extern "C" 包裹
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

#include <QAtomicInt>
#include <QAtomicInteger>
#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>

namespace {

// 析构阶段的会话线程等待上限（毫秒）。仅在控件销毁/进程退出时使用，
// 不参与日常 openStream/closeStream 路径（那条路径保证非阻塞）。
constexpr int kDestructorWaitMs = 3000;

} // namespace

// ============================================================================
// Worker - 单次会话工作线程
// 持有该会话的 FFmpeg 资源与自动重连循环（指数退避 + 抖动 + 停帧看门狗）。
// 停止通过原子标志 + 条件变量传递，FFmpeg 中断回调据此让阻塞调用快速返回。
//
// 会话代次（generation）：控制器每次 openStream 分配新代次，Worker 仅在
// 自身代次等于控制器当前代次时才上报帧/事件/统计，从而保证被替换的旧会话
// （即使仍在收尾）不会污染新会话的显示。
// ============================================================================
class RtspThread::Worker : public QThread
{
public:
    Worker(const QString &url, RtspThread *sink, quint64 generation,
           const StreamConfig &config)
        : QThread(nullptr)
        , m_url(url)
        , m_sink(sink)
        , m_generation(generation)
        , m_cfg(config)
    {
        setObjectName(QStringLiteral("RtspSession"));
        // 不入控制器对象树：即使线程未能及时退出而被脱离，
        // 控制器析构也不会删除运行中的线程（否则会崩溃）。
        // 线程结束后统一由所属线程（UI）事件循环经 finished -> deleteLater 自毁。
        connect(this, &QThread::finished, this, &QObject::deleteLater);
    }

    // 线程安全：可从任意线程请求停止
    void requestStop()
    {
        m_stop.storeRelaxed(1);
        QMutexLocker lock(&m_sleepMutex);
        m_sleepCond.wakeAll();
    }

    bool stopRequested() const { return m_stop.loadRelaxed() != 0; }

    // 本会话是否仍是控制器当前会话（否则视为已被替换，停止上报）
    bool isCurrent() const
    {
        return m_sink && m_sink->m_workerGeneration.loadRelaxed() == m_generation;
    }

    // 直接向控制器写入「最后收帧时刻」（跨线程原子写，供外部看门狗读取）。
    // 即使本线程随后卡死在读取中，控制器仍能据此判定超时并重建会话。
    void touchControllerActivity()
    {
        if (m_sink && isCurrent())
            m_sink->m_lastFrameMs.storeRelaxed(QDateTime::currentMSecsSinceEpoch());
    }

    // FFmpeg 中断回调：非 0 使阻塞中的 avformat_open_input / av_read_frame 立即返回。
    // 除停止请求外，还兼顾「停帧看门狗」：流已打开后长时间无帧时判定会话僵死，
    // 主动打断阻塞读，让 run() 走退避重连（网线插拔后自愈）。
    static int interruptCallback(void *opaque)
    {
        auto *self = static_cast<Worker *>(opaque);
        if (self->stopRequested()) return 1;
        if (self->stalled()) {
            self->m_stallLatched.storeRelaxed(1);
            return 1;
        }
        return 0;
    }

    bool stalled() const
    {
        return rtspIsStalled(m_watchdogArmed.loadRelaxed() != 0,
                             m_lastActivityMs.loadRelaxed(),
                             QDateTime::currentMSecsSinceEpoch(),
                             m_cfg.stallTimeoutMs);
    }

    bool stallLatched() const { return m_stallLatched.loadRelaxed() != 0; }

protected:
    void run() override;

private:
    // 打开并解码直到出错/结束；返回 true 表示本次曾成功打开
    bool openAndDecode(AVPacket *pkt, AVFrame *decoded, AVFrame *rgb);

    // 可被停止请求打断的等待；返回 false 表示期间收到停止请求
    bool waitInterruptible(int ms);

    void safeCleanup();

    // 退避抖动：在 baseMs 基础上叠加 ±jitter% 随机量，避免多设备同拍重连
    int jitteredDelay(int baseMs) const;

    // 向控制器投递统计快照（跨线程队列投递到控制器所在线程）
    void pushStats();

    void setLastError(const QString &msg) { m_lastError = msg; }

    void emitError(const QString &msg)
    {
        if (m_sink && isCurrent())
            emit m_sink->streamError(msg);
    }

    QString m_url;
    QPointer<RtspThread> m_sink;
    quint64 m_generation = 0;
    StreamConfig m_cfg;

    QAtomicInt m_stop{0};
    QMutex m_sleepMutex;
    QWaitCondition m_sleepCond;

    // 停帧看门狗（跨线程：中断回调读取，解码线程写入）
    QAtomicInteger<qint64> m_lastActivityMs{0};   // 最近一帧/流打开时刻
    QAtomicInt m_watchdogArmed{0};               // 仅流成功打开后武装
    QAtomicInt m_stallLatched{0};                // 本次会话是否已因僵死被打断

    // 统计（仅工作线程写入，经 pushStats 投递）
    bool m_hadOpen = false;
    int m_reconnectCount = 0;
    int m_consecutiveFailures = 0;
    QString m_lastError;

    // 本会话 FFmpeg 资源（仅在工作线程内访问）
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext  *m_decCtx = nullptr;
    SwsContext      *m_swsCtx = nullptr;
    uint8_t         *m_rgbBuf = nullptr;
    int m_videoStreamIdx = -1;
};

// ============================================================================
// Worker 实现
// ============================================================================
void RtspThread::Worker::run()
{
    AVPacket *pkt = av_packet_alloc();
    AVFrame  *decoded = av_frame_alloc();
    AVFrame  *rgb = av_frame_alloc();
    if (!pkt || !decoded || !rgb) {
        av_packet_free(&pkt);
        av_frame_free(&decoded);
        av_frame_free(&rgb);
        setLastError(QStringLiteral("FFmpeg 结构分配失败"));
        pushStats();
        emitError(m_lastError);
        return;
    }

    int delay = qMax(1, m_cfg.backoffInitialMs);

    while (!stopRequested()) {
        const qint64 sessionStart = QDateTime::currentMSecsSinceEpoch();
        const bool opened = openAndDecode(pkt, decoded, rgb);
        const qint64 sessionDur = QDateTime::currentMSecsSinceEpoch() - sessionStart;
        if (stopRequested()) break;

        if (opened && sessionDur >= m_cfg.minSessionMs) {
            // 有效会话：重置退避与重试计数（避免长时间正常播放后偶发中断被无限延长，
            // 也避免累计计数在健康运行一段时间后误触发 maxRetries 上限）
            delay = qMax(1, m_cfg.backoffInitialMs);
            m_consecutiveFailures = 0;
            m_reconnectCount = 0;
        } else {
            // 打开失败，或“刚连上即断”的过短会话：计入连续失败，退避继续增长
            ++m_consecutiveFailures;
        }

        pushStats();

        // 有限重试模式下，达到上限即停止并通知
        if (m_cfg.maxRetries > 0 && m_reconnectCount >= m_cfg.maxRetries) {
            setLastError(QStringLiteral("重连次数已达上限 %1").arg(m_cfg.maxRetries));
            pushStats();
            emitError(m_lastError);
            break;
        }

        ++m_reconnectCount;

        // 空字符串 = 流断开，正在重连
        emitError(QString());
        if (!waitInterruptible(jitteredDelay(delay))) break;

        delay = qMin(delay * 2, qMax(delay, m_cfg.backoffMaxMs));
    }

    av_frame_free(&rgb);
    av_frame_free(&decoded);
    av_packet_free(&pkt);
    safeCleanup();
    qDebug() << "RtspThread session thread exited";
}

int RtspThread::Worker::jitteredDelay(int baseMs) const
{
    return rtspApplyJitter(baseMs, m_cfg.backoffJitterPercent,
                           QRandomGenerator::global()->generate());
}

void RtspThread::Worker::pushStats()
{
    RtspThread *sink = m_sink;
    if (!sink) return;

    Stats s;
    s.connected = m_hadOpen;
    s.healthy = (m_watchdogArmed.loadRelaxed() != 0) && !stalled();
    s.reconnectCount = m_reconnectCount;
    s.consecutiveFailures = m_consecutiveFailures;
    const qint64 last = m_lastActivityMs.loadRelaxed();
    s.lastFrameAgeMs = (last > 0) ? (QDateTime::currentMSecsSinceEpoch() - last) : -1;
    s.lastError = m_lastError;

    const quint64 gen = m_generation;
    QMetaObject::invokeMethod(sink, [sink, gen, s]() {
        sink->applyWorkerStats(gen, s);
    }, Qt::QueuedConnection);
}

bool RtspThread::Worker::openAndDecode(AVPacket *pkt, AVFrame *decoded, AVFrame *rgb)
{
    bool opened = false;

    // 连接/开流阶段不武装看门狗：慢连接由 rw_timeout 兜底，
    // 若此阶段也判停帧会误杀正常但偏慢的首帧协商。
    m_watchdogArmed.storeRelaxed(0);
    m_stallLatched.storeRelaxed(0);

    // 传输协议：显式 udp 便捷项优先
    const QByteArray transport = m_cfg.udp
        ? QByteArray("udp")
        : m_cfg.transport.toUtf8();
    const QByteArray ioTimeout = QByteArray::number(qMax(100, m_cfg.ioTimeoutMs) * 1000);

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", transport.constData(), 0);
    av_dict_set(&opts, "rw_timeout", ioTimeout.constData(), 0);
    // RTSP demuxer 自身的 socket TCP I/O 超时（微秒）：让卡在读取中的调用
    // 有机会自行超时返回，减少外部看门狗重建时遗留的僵尸线程。
    av_dict_set(&opts, "timeout", ioTimeout.constData(), 0);
    av_dict_set(&opts, "buffer_size", "1048576", 0);
    av_dict_set(&opts, "max_delay", "500000", 0);
    if (m_cfg.tcpKeepAlive)
        av_dict_set(&opts, "tcp_keepalive", "1", 0);

    // 先 alloc 再设中断回调，确保 avformat_open_input 使用的是带回调的 context
    m_fmtCtx = avformat_alloc_context();
    if (!m_fmtCtx) {
        av_dict_free(&opts);
        setLastError(QStringLiteral("分配格式上下文失败"));
        return false;
    }
    m_fmtCtx->interrupt_callback.callback = interruptCallback;
    m_fmtCtx->interrupt_callback.opaque = this;

    const QByteArray url8 = m_url.toUtf8();
    int ret = avformat_open_input(&m_fmtCtx, url8.constData(), nullptr, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        if (stopRequested()) { safeCleanup(); return false; }
        char err[128] = {};
        av_strerror(ret, err, sizeof(err));
        qDebug() << "RtspThread - open failed:" << err;
        safeCleanup();
        setLastError(QString::fromUtf8("打开RTSP失败: %1").arg(err));
        emitError(m_lastError);
        return false;
    }

    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        setLastError(QStringLiteral("查找流信息失败"));
        emitError(m_lastError);
        safeCleanup();
        return false;
    }

    m_videoStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStreamIdx < 0) {
        setLastError(QStringLiteral("未找到视频流"));
        emitError(m_lastError);
        safeCleanup();
        return false;
    }

    const AVStream *vs = m_fmtCtx->streams[m_videoStreamIdx];
    const AVCodec *codec = avcodec_find_decoder(vs->codecpar->codec_id);
    if (!codec) {
        setLastError(QStringLiteral("不支持该视频编码格式"));
        emitError(m_lastError);
        safeCleanup();
        return false;
    }

    m_decCtx = avcodec_alloc_context3(codec);
    if (!m_decCtx) {
        setLastError(QStringLiteral("分配解码器失败"));
        emitError(m_lastError);
        safeCleanup();
        return false;
    }
    avcodec_parameters_to_context(m_decCtx, vs->codecpar);
    ret = avcodec_open2(m_decCtx, codec, nullptr);
    if (ret < 0) {
        setLastError(QStringLiteral("打开解码器失败"));
        emitError(m_lastError);
        safeCleanup();
        return false;
    }

    const int w = m_decCtx->width;
    const int h = m_decCtx->height;
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
        setLastError(QStringLiteral("异常分辨率 %1x%2").arg(w).arg(h));
        emitError(m_lastError);
        safeCleanup();
        return false;
    }

    const size_t rgbBufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB32, w, h, 32);
    if (rgbBufSize == 0 || rgbBufSize > 64u * 1024 * 1024) {
        setLastError(QStringLiteral("RGB 缓冲大小异常: %1").arg(rgbBufSize));
        emitError(m_lastError);
        safeCleanup();
        return false;
    }

    m_swsCtx = sws_getContext(w, h, m_decCtx->pix_fmt,
                              w, h, AV_PIX_FMT_RGB32,
                              SWS_POINT, nullptr, nullptr, nullptr);
    if (!m_swsCtx) {
        setLastError(QStringLiteral("初始化像素转换器失败"));
        emitError(m_lastError);
        safeCleanup();
        return false;
    }

    av_freep(&m_rgbBuf);
    // 预留额外填充，防止 SIMD 转换越界写
    m_rgbBuf = static_cast<uint8_t *>(av_malloc(rgbBufSize + 1024));
    if (!m_rgbBuf) {
        setLastError(QStringLiteral("分配 RGB 缓冲失败"));
        emitError(m_lastError);
        safeCleanup();
        return false;
    }
    av_image_fill_arrays(rgb->data, rgb->linesize, m_rgbBuf, AV_PIX_FMT_RGB32, w, h, 32);

    // 打开过程中可能已被要求停止，避免关闭后仍上报已打开
    if (stopRequested()) { safeCleanup(); return false; }

    qDebug() << "RtspThread - stream opened successfully";
    m_hadOpen = true;
    m_lastError.clear();
    if (m_sink && isCurrent()) emit m_sink->streamOpened();
    opened = true;

    // 流已打开：武装停帧看门狗（此后长时间无帧即判定会话僵死）
    m_lastActivityMs.storeRelaxed(QDateTime::currentMSecsSinceEpoch());
    m_watchdogArmed.storeRelaxed(1);
    // 通知控制器级外部看门狗：会话已就绪并开始计时
    touchControllerActivity();
    pushStats();
    // mark controller-side "reading" so external watchdog only judges during active read
    if (m_sink && isCurrent()) m_sink->m_workerReading.storeRelaxed(1);

    // ── 解码循环 ──
    qint64 lastStatsPush = QDateTime::currentMSecsSinceEpoch();
    while (!stopRequested()) {
        ret = av_read_frame(m_fmtCtx, pkt);
        if (ret < 0) {
            if (stallLatched() && !stopRequested()) {
                // 看门狗判定僵死并打断阻塞读。保留 opened=true 让 run() 重置退避，
                // 以最小延迟快速重开，等价于自动执行一次断开+重连。
                qWarning() << "RtspThread - stall watchdog fired, restarting session";
                setLastError(QStringLiteral("流僵死，已重启会话"));
            } else {
                qDebug() << "RtspThread - read frame error:" << ret;
                char err[128] = {};
                av_strerror(ret, err, sizeof(err));
                setLastError(QString::fromUtf8("RTSP 读取中断: %1").arg(err));
            }
            break;
        }

        if (pkt->stream_index == m_videoStreamIdx) {
            ret = avcodec_send_packet(m_decCtx, pkt);
            if (ret < 0) { av_packet_unref(pkt); continue; }

            while (ret >= 0) {
                ret = avcodec_receive_frame(m_decCtx, decoded);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) break;

                // 收到有效帧即刷新看门狗活动时间
                m_lastActivityMs.storeRelaxed(QDateTime::currentMSecsSinceEpoch());
                touchControllerActivity();
                if (sws_scale(m_swsCtx, decoded->data, decoded->linesize,
                              0, h, rgb->data, rgb->linesize) > 0) {
                    if (!stopRequested() && m_rgbBuf && rgb->linesize[0] > 0) {
                        const QImage img(m_rgbBuf, w, h, rgb->linesize[0], QImage::Format_RGB32);
                        if (m_sink && isCurrent()) emit m_sink->frameReady(img.copy());
                    }
                }
            }
        }
        av_packet_unref(pkt);

        // 周期性上报统计（约 1s 一次），用于 UI 链路健康显示
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - lastStatsPush >= 1000) {
            lastStatsPush = now;
            pushStats();
        }
    }

    safeCleanup();
    return opened;
}

bool RtspThread::Worker::waitInterruptible(int ms)
{
    QMutexLocker lock(&m_sleepMutex);
    if (stopRequested()) return false;
    m_sleepCond.wait(&m_sleepMutex, static_cast<unsigned long>(ms));
    return !stopRequested();
}

void RtspThread::Worker::safeCleanup()
{
    // clear controller-side "reading" flag (worker no longer reading)
    if (m_sink && isCurrent()) m_sink->m_workerReading.storeRelaxed(0);
    // 会话资源销毁前撤防看门狗，避免中断回调误判残余状态
    m_watchdogArmed.storeRelaxed(0);
    // avformat_close_input 对 RTSP 会自动发送 TEARDOWN，回收服务端会话
    if (m_swsCtx) { sws_freeContext(m_swsCtx); m_swsCtx = nullptr; }
    if (m_decCtx) avcodec_free_context(&m_decCtx);
    if (m_fmtCtx) avformat_close_input(&m_fmtCtx);
    av_freep(&m_rgbBuf);
    m_videoStreamIdx = -1;
}

// ============================================================================
// 控制器实现
// ============================================================================
RtspThread::RtspThread(QObject *parent)
    : QObject(parent)
{
    setObjectName(QStringLiteral("RtspThread"));
    qRegisterMetaType<RtspThread::Stats>("RtspThread::Stats");

    // 控制器级外部看门狗：在 UI 线程周期性检查「是否长时间无帧」。
    // 这是网线插拔后能恢复的关键——不依赖工作线程是否能被中断回调唤醒。
    m_watchdogTimer = new QTimer(this);
    m_watchdogTimer->setInterval(2000);
    connect(m_watchdogTimer, &QTimer::timeout, this, &RtspThread::checkExternalWatchdog);
}

RtspThread::~RtspThread()
{
    // 析构阶段：请求停止当前及已脱离的会话线程，并限时等待退出。
    // 这是本类唯一允许阻塞的位置（对象正在销毁，UI 已无交互）。
    closeStream();

    const QList<Worker *> pending = m_retired;
    for (Worker *w : pending) {
        if (!w) continue;
        w->requestStop();
        if (!w->wait(kDestructorWaitMs)) {
            qWarning() << "RtspThread destructor: session thread detaching";
        }
    }
}

void RtspThread::openStream(const QString &url, const StreamConfig &config)
{
    if (url.isEmpty()) return;

    // 结束旧会话（非阻塞：停用并脱离）
    closeStream();

    m_url = url;
    m_config = config;
    m_stats = Stats();
    m_stats.watchdogRestarts = m_watchdogRestarts;
    m_lastFrameMs.storeRelaxed(0);
    m_workerReading.storeRelaxed(0);

    m_workerGeneration.storeRelaxed(m_nextGeneration);
    m_worker = new Worker(url, this, m_nextGeneration, config);
    ++m_nextGeneration;
    m_worker->start();

    if (config.externalWatchdog && !m_watchdogTimer->isActive())
        m_watchdogTimer->start();
}

void RtspThread::closeStream()
{
    if (m_watchdogTimer) m_watchdogTimer->stop();

    if (!m_worker) return;

    Worker *worker = m_worker;
    m_worker = nullptr;
    // 使旧会话代次失效：其后续的帧/事件/统计均被忽略
    m_workerGeneration.storeRelaxed(0);

    retireWorker(worker);
}

// ============================================================================
// 控制器级外部看门狗（UI 线程）
//
// 判据：存在活动会话，且距最近一次收帧（或流打开）超过 stallTimeoutMs。
// 命中即主动关闭并重建会话，等价于自动执行一次“手动断开 + 重新连接”。
// 这是网线插拔后能恢复的兜底保证：不依赖卡死线程的停止/中断回调是否生效。
// ============================================================================
void RtspThread::checkExternalWatchdog()
{
    if (!m_worker) return;                        // 无活动会话
    if (!m_config.externalWatchdog) return;
    if (!m_worker->isRunning()) return;           // 线程已退出（走内部重连逻辑）

    // only judge while worker is actively reading; during its own backoff/retry
    // the thread is alive but not reading, so don't force a rebuild.
    if (m_workerReading.loadRelaxed() == 0) return;

    const qint64 lastFrame = m_lastFrameMs.loadRelaxed();
    if (lastFrame <= 0) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 timeout = qMax(2000, m_config.stallTimeoutMs);
    // reuse the tested pure predicate: worker-reading && lastFrame>0 && elapsed>timeout
    if (rtspIsStalled(true, lastFrame, now, timeout)) {
        qWarning() << "RtspThread - external watchdog fired (no frame for"
                   << (now - lastFrame) << "ms), restarting session";
        restartSessionByWatchdog();
    }
}

void RtspThread::restartSessionByWatchdog()
{
    if (m_url.isEmpty()) return;

    ++m_watchdogRestarts;
    const QString url = m_url;
    const StreamConfig config = m_config;

    // 广播一次统计，便于 UI 反映“正在重建”
    m_stats.healthy = false;
    m_stats.lastFrameAgeMs = -1;
    m_stats.watchdogRestarts = m_watchdogRestarts;
    m_stats.lastError = QStringLiteral("看门狗：长时间无帧，正在重建会话");
    emit statsChanged(m_stats);

    // 关闭旧会话（可能已卡死，非阻塞脱离）并重建全新会话
    openStream(url, config);
}

void RtspThread::retireWorker(Worker *worker)
{
    if (!worker) return;
    m_retired.append(worker);
    // 线程自然结束后从脱离列表移除（删除由其自身 finished -> deleteLater 完成）
    connect(worker, &QThread::finished, this, [this, worker]() {
        m_retired.removeAll(worker);
    }, Qt::QueuedConnection);
    worker->requestStop();
}

void RtspThread::applyWorkerStats(quint64 generation, const Stats &stats)
{
    // 仅接受当前会话的统计，丢弃被替换旧会话的迟到上报
    if (generation == 0 || generation != m_workerGeneration.loadRelaxed())
        return;
    m_stats = stats;
    emit statsChanged(m_stats);
}

bool RtspThread::isRunning() const
{
    // “活动会话”语义：closeStream() 后立即为 false（非阻塞关闭），
    // 已脱离、正在收尾的旧线程不计入（否则会误判设备仍在拉流）。
    return m_worker && m_worker->isRunning();
}
