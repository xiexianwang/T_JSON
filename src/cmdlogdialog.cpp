#include "cmdlogdialog.h"
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QPushButton>

CmdLogDialog::CmdLogDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("指令日志"));
    resize(500, 350);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFont(QFont("Consolas", 9));
    m_log->setStyleSheet("QPlainTextEdit{background:#1e1e1e;color:#d4d4d4;}");
    lay->addWidget(m_log);

    auto *btnClear = new QPushButton(QStringLiteral("清空"), this);
    btnClear->setFixedWidth(60);
    connect(btnClear, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);
    lay->addWidget(btnClear, 0, Qt::AlignRight);
}

void CmdLogDialog::appendLog(const QString& serialType, const QByteArray& data)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString hex = QString::fromLatin1(data.toHex(' ').toUpper());
    m_log->appendPlainText(QStringLiteral("[%1] [%2] %3").arg(ts, serialType, hex));
}
