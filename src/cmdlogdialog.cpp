#include "cmdlogdialog.h"
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QPushButton>

// 构造函数：创建日志对话框布局
// 包含一个深色背景的只读文本控件和一个"清空"按钮
CmdLogDialog::CmdLogDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("指令日志"));
    resize(500, 350);

    // 垂直布局，紧凑边距
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    // 日志文本控件：只读、等宽字体、VS Code 风格深色主题
    m_log = new QPlainTextEdit(this);
    m_log->setObjectName(QStringLiteral("cmdLog"));
    m_log->setReadOnly(true);
    m_log->setFont(QFont("Consolas", 9));
    lay->addWidget(m_log);

    // 清空按钮：点击后清除所有日志内容
    auto *btnClear = new QPushButton(QStringLiteral("清空"), this);
    btnClear->setFixedWidth(60);
    connect(btnClear, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);
    lay->addWidget(btnClear, 0, Qt::AlignRight);
}

// 追加日志条目
// 格式：[HH:mm:ss.zzz] [通道类型] HEX_DATA
// data 以空格分隔的大写十六进制形式显示
void CmdLogDialog::appendLog(const QString& serialType, const QByteArray& data)
{
    // 生成当前时间戳（精确到毫秒）
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    // 将原始字节转换为大写十六进制字串，字节间以空格分隔
    QString hex = QString::fromLatin1(data.toHex(' ').toUpper());
    // 追加一行日志
    m_log->appendPlainText(QStringLiteral("[%1] [%2] %3").arg(ts, serialType, hex));
}
