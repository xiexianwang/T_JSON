#ifndef PTZFORWARDER_H
#define PTZFORWARDER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QList>
#include <QByteArray>

class PtzForwarder : public QObject
{
    Q_OBJECT
public:
    explicit PtzForwarder(QObject *parent = nullptr);
    ~PtzForwarder();

    void start(const QString& ptzIp, quint16 ptzPort, quint16 mockServerPort);
    void setOffsets(double panOffset, double tiltOffset);
    void flushZeroPosition();
    void stop();

signals:
    void ptzAnglesUpdated(double pan, double tilt);

private slots:
    void onPtzReadyRead();
    void onNewMockConnection();
    void onMockClientDisconnected();
    void onPtzDisconnected();
    void reconnectPtz();

private:
    void parsePelcoD(const QByteArray& data);

    QTcpSocket* m_ptzClient;
    QTcpServer* m_mockServer;
    QList<QTcpSocket*> m_mockClients;
    QByteArray m_buffer;

    QString m_ptzIp;
    quint16 m_ptzPort;
    double m_panOffset = 0.0;
    double m_tiltOffset = 0.0;
};

#endif // PTZFORWARDER_H