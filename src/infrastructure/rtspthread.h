#ifndef RTSPTHREAD_H
#define RTSPTHREAD_H

#include <QAtomicInt>
#include <QAtomicInteger>
#include <QObject>
#include <QImage>
#include <QList>
#include <QMetaType>
#include <QString>

class QTimer;

// ============================================================================
// RtspThread - RTSP 流控制器（对外稳定接口）
//
// 采用音视频行业标准的「一次会话一个工作线程」模型：
//   * 每次 openStream() 都会停用旧会话并创建一个全新的工作线程，
//     从根本上消除 QThread 复用 / 停止标志被复位导致的“复活卡死”问题；
//   * closeStream() 非阻塞：立即停用旧会话（丢弃其后续帧/信号）并脱离，
//     旧线程在中断回调作用下尽快自毁，UI 线程不再有任何 wait() 阻塞；
//   * 工作线程内部负责 FFmpeg 打开/解码、指数退避 + 抖动自动重连、
//     停帧看门狗（打断阻塞读），并通过 rw_timeout 保证阻塞调用可被中断。
//
// 兜底恢复（关键）：仅依赖「中断回调打断 av_read_frame」在部分 RTSP/TCP
// 实现下并不可靠（线程可能永久阻塞在读取中）。因此控制器额外维护一个
// 「外部看门狗」：由 UI 线程定时比对最后收帧时间，超时即主动关闭并重建
// 会话——等价于自动执行一次“手动断开 + 重新连接”，不依赖卡死线程的任何
// 回调，从根本上保证网线插拔后能够恢复。
//
// 本类不是 QThread：线程生命周期完全封装在实现中。
// ============================================================================
class RtspThread : public QObject
{
    Q_OBJECT
public:
    // 单个会话的运行时配置（可按设备/链路覆盖）
    struct StreamConfig {
        QString transport = QStringLiteral("tcp");  // tcp / udp
        int ioTimeoutMs = 2000;                     // socket I/O 超时
        int stallTimeoutMs = 10000;                 // 停帧看门狗阈值
        int backoffInitialMs = 1000;                // 退避初始延迟
        int backoffMaxMs = 15000;                   // 退避上限
        int backoffJitterPercent = 20;              // 退避抖动幅度（±%）
        int maxRetries = 0;                         // 0 = 无限重试
        int minSessionMs = 2000;                    // 最短有效会话时长（防抖）
        bool tcpKeepAlive = true;                   // TCP keepalive
        bool udp = false;                           // 便捷项：true 时等价 transport=udp
        bool externalWatchdog = true;               // 启用控制器级外部看门狗（兜底恢复）
    };

    // 可观测性统计快照（UI/日志用）
    struct Stats {
        bool connected = false;        // 当前是否已成功打开过流
        bool healthy = false;          // 当前是否在持续收帧
        int reconnectCount = 0;        // 累计重连次数
        int consecutiveFailures = 0;   // 连续失败次数
        int watchdogRestarts = 0;      // 外部看门狗触发的会话重建次数
        qint64 lastFrameAgeMs = -1;    // 距最近一帧的时长（-1 = 尚无帧）
        QString lastError;             // 最近一次错误（空 = 无）
    };

    explicit RtspThread(QObject *parent = nullptr);
    ~RtspThread() override;

    // 打开指定 URL 的 RTSP 流（非阻塞，内部异步会话）
    void openStream(const QString &url, const StreamConfig &config = StreamConfig());

    // 关闭当前会话；立即停用并脱离，不阻塞调用线程
    void closeStream();

    // 当前是否有活动会话线程
    bool isRunning() const;

    // 最近一次统计快照
    Stats stats() const { return m_stats; }

signals:
    // 每解码一帧后发射，携带 RGB32 格式图像（跨线程队列投递）
    void frameReady(const QImage &frame);

    // 流成功打开后发射
    void streamOpened();

    // 连接/解码发生错误时发射；空字符串表示“流结束，正在重连”
    void streamError(const QString &msg);

    // 统计快照更新（连接状态/重连次数/健康度等）
    void statsChanged(const RtspThread::Stats &stats);

private:
    class Worker;
    friend class Worker;

    void retireWorker(Worker *worker);   // 停用并登记待回收工作线程
    // 工作线程回报统计（仅在仍是当前会话时生效）
    void applyWorkerStats(quint64 generation, const Stats &stats);
    // 外部看门狗：超时未收帧则重建会话（不依赖工作线程回调）
    void checkExternalWatchdog();
    void restartSessionByWatchdog();

    QTimer* m_watchdogTimer = nullptr;   // 控制器级看门狗定时器（UI 线程）
    QString m_url;                       // 当前 URL（看门狗重建用）
    StreamConfig m_config;               // 当前会话配置（看门狗重建用）

    Worker *m_worker = nullptr;          // 当前会话工作线程（未加入对象树）
    QList<Worker *> m_retired;           // 已脱离、等待自毁的旧会话线程
    quint64 m_nextGeneration = 1;
    QAtomicInteger<quint64> m_workerGeneration{0};  // 跨线程读取：0 表示无当前会话
    // 控制器侧「最后收帧时刻」（工作线程直接写，看门狗读）。
    // 流成功打开时种下初值，每解码一帧刷新；长时间不刷新即由外部看门狗判定僵死。
    QAtomicInteger<qint64> m_lastFrameMs{0};
    // 控制器侧「读取中」标志：工作线程进入解码循环置 1、退出（含正常出错返回）置 0。
    // 外部看门狗仅在置 1 时判定，避免在工作线程正常退避重连期间误触发重建。
    QAtomicInt m_workerReading{0};
    int m_watchdogRestarts = 0;                     // 外部看门狗触发次数
    Stats m_stats;
};

Q_DECLARE_METATYPE(RtspThread::Stats)

#endif // RTSPTHREAD_H
