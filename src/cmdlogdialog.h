#ifndef CMDLOGDIALOG_H
#define CMDLOGDIALOG_H

#include <QDialog>

class QPlainTextEdit;

class CmdLogDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CmdLogDialog(QWidget *parent = nullptr);

public slots:
    void appendLog(const QString& serialType, const QByteArray& data);

private:
    QPlainTextEdit *m_log;
};

#endif
