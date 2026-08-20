// ============================================================
// test_stm32tcptransport.cpp - Stm32TcpTransport 单元测试
// 覆盖：初始状态（seq / isOpen）、send 未连接时返回 false
// ============================================================

#include <QtTest>
#include "infrastructure/stm32tcptransport.h"

class TestStm32TcpTransport : public QObject
{
    Q_OBJECT
private slots:
    void initialState();
    void sendWithoutConnection();
};

void TestStm32TcpTransport::initialState()
{
    Stm32TcpTransport transport;
    QCOMPARE(transport.isOpen(), false);
    QCOMPARE(transport.seq(), 0);
}

void TestStm32TcpTransport::sendWithoutConnection()
{
    Stm32TcpTransport transport;
    QJsonObject json;
    json["cmd"] = "test";
    // 未连接时 send 应返回 false（无 socket 或 socket 未连接）
    QCOMPARE(transport.send(json), false);
    // seq 未自增（因为 send 未执行到 ++m_seq）
    QCOMPARE(transport.seq(), 0);
}

QTEST_GUILESS_MAIN(TestStm32TcpTransport)
#include "test_stm32tcptransport.moc"