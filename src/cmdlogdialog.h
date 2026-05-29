#ifndef CMDLOGDIALOG_H
#define CMDLOGDIALOG_H

#include <QDialog>

class QPlainTextEdit;

// CmdLogDialog：串口指令日志显示对话框
// 以只读文本控件实时展示所有串口发送/接收的原始十六进制指令，
// 每条日志带时间戳和通道标识，便于调试和监控通信过程。
class CmdLogDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CmdLogDialog(QWidget *parent = nullptr);

public slots:
    // 追加一条日志记录：通道类型 + 原始数据（十六进制显示）
    void appendLog(const QString& serialType, const QByteArray& data);

private:
    QPlainTextEdit *m_log;  // 只读日志显示控件，深色背景等宽字体
};

#endif
