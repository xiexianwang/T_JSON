#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include <QMessageBox>
#include <QDebug>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(new TJsonClient(this))
    , m_device(new DeviceController(m_client, this))
{
    ui->setupUi(this);
    setupUiStyles();

    connect(m_client, &TJsonClient::deviceConnected, this, &MainWindow::onDeviceConnected);
    connect(m_client, &TJsonClient::deviceDisconnected, this, &MainWindow::onDeviceDisconnected);
    connect(m_client, &TJsonClient::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_client, &TJsonClient::jsonReceived, this, &MainWindow::onJsonReceived);
    connect(m_client, &TJsonClient::imageSnapped, this, &MainWindow::onImageSnapped);
    
    connect(m_client, &TJsonClient::reconnecting, this, [this](int attempt, int maxRetries) {
        Q_UNUSED(maxRetries);
        ui->btnConnect->setText(QString::fromUtf8("重连中(次数:%1)").arg(attempt));
        ui->btnConnect->setStyleSheet("background-color: #d68b00;");
        ui->statusbar->showMessage(QString::fromUtf8("网络波动，正在进行第 %1 次自动探测重连...").arg(attempt));
    });
    connect(m_client, &TJsonClient::reconnectFailed, this, [this]() {
        ui->btnConnect->setText(QString::fromUtf8("连接设备"));
        ui->btnConnect->setEnabled(true);
        ui->btnConnect->setStyleSheet("");
        ui->statusbar->showMessage(QString::fromUtf8("重连失败，已放弃连接"), 5000);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUiStyles()
{
    QFile qssFile(":/style.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QString qss = QLatin1String(qssFile.readAll());
        this->setStyleSheet(qss);
        qssFile.close();
    } else {
        qDebug() << "Failed to load style.qss";
    }
    
    ui->tableIdentify->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MainWindow::on_btnConnect_clicked()
{
    if (m_client->isConnected()) {
        m_client->disconnectDevice();
    } else {
        QString ip = ui->lineEditIp->text();
        quint16 port = ui->spinBoxPort->value();
        m_client->connectToDevice(ip, port);
        ui->btnConnect->setText(QString::fromUtf8("连接中..."));
        ui->btnConnect->setEnabled(false);
    }
}

void MainWindow::onDeviceConnected()
{
    ui->btnConnect->setText(QString::fromUtf8("断开连接"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setStyleSheet("background-color: #c75450;");
    ui->statusbar->showMessage(QString::fromUtf8("已连接到设备"), 3000);
}

void MainWindow::onDeviceDisconnected()
{
    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setStyleSheet("");
    ui->statusbar->showMessage(QString::fromUtf8("设备已断开"), 3000);
}

void MainWindow::onErrorOccurred(const QString& errorMsg)
{
    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setStyleSheet("");
    QMessageBox::warning(this, QString::fromUtf8("连接错误"), errorMsg);
}

void MainWindow::onJsonReceived(const QJsonObject& doc)
{
    updateStatusFromJson(doc);
}

void MainWindow::onImageSnapped(const QByteArray& jpegData, const QRect& location)
{
    Q_UNUSED(jpegData);
    Q_UNUSED(location);
}

void MainWindow::updateStatusFromJson(const QJsonObject& doc)
{
    QString controlType = doc.value("ControlType").toString();
    
    if (controlType == "AIInfo") {
        int count = doc.value("ObjectCount").toInt();
        ui->lblIdentifyCount->setText(QString::fromUtf8("目标总数: %1").arg(count));
        ui->tableIdentify->setRowCount(0);
        
        if (doc.contains("Object") && doc.value("Object").isObject()) {
            QJsonObject objMap = doc.value("Object").toObject();
            for (auto it = objMap.begin(); it != objMap.end(); ++it) {
                QString id = it.key();
                QJsonObject obj = it.value().toObject();
                
                int r = ui->tableIdentify->rowCount();
                ui->tableIdentify->insertRow(r);
                ui->tableIdentify->setItem(r, 0, new QTableWidgetItem(id));
                ui->tableIdentify->setItem(r, 1, new QTableWidgetItem(QString::number(obj.value("Class").toInt())));
                ui->tableIdentify->setItem(r, 2, new QTableWidgetItem(QString::number(obj.value("Distance").toDouble(), 'f', 1)));
                
                if (obj.contains("Points")) {
                    QJsonObject pts = obj.value("Points").toObject();
                    QString pos = QString("(%1,%2)").arg(pts.value("Left").toInt()).arg(pts.value("Top").toInt());
                    ui->tableIdentify->setItem(r, 3, new QTableWidgetItem(pos));
                }
            }
        }
    } else if (controlType == "ZoomInfo") {
        ui->statCamMode->setText(QString::number(doc.value("CamShowMode").toInt()));
        ui->statLatitude->setText(doc.value("Latitude").toString());
        ui->statLongitude->setText(doc.value("Longitude").toString());
        ui->statHeight->setText(QString::number(doc.value("Height").toDouble(), 'f', 1));
        ui->statDistance->setText(QString::number(doc.value("LaserRange").toDouble(), 'f', 1));
        ui->statPanAngle->setText(QString::number(doc.value("PTZInfoH").toDouble(), 'f', 1));
        ui->statTiltAngle->setText(QString::number(doc.value("PTZInfoV").toDouble(), 'f', 1));
    } else if (controlType == "ImageSetting") {
        ui->paramResolution->setText(QString::number(doc.value("ImageSize").toInt()));
        ui->paramBitrate->setText(QString::number(doc.value("ImageBit").toInt()));
        ui->paramCodec->setText(QString::number(doc.value("ImageCode").toInt()));
        ui->paramWorkMode->setText(QString::number(doc.value("WorkMode").toInt()));
        ui->paramPipShow->setText(QString::number(doc.value("PipShow").toInt()));
        ui->paramAlgoModel->setText(QString::number(doc.value("Model").toInt()));
        ui->paramMaxVisFL->setText(doc.value("MaxVisFL").toString());
        ui->paramMaxIRFL->setText(doc.value("MaxIRFL").toString());
    }
}

void MainWindow::on_radioModeOff_clicked() { m_device->setWorkMode(0); }
void MainWindow::on_radioModeIdentify_clicked() { m_device->setWorkMode(1); }
void MainWindow::on_radioModeAutoTrack_clicked() { m_device->setWorkMode(2); }

void MainWindow::on_btnPtzMoveTo_clicked()
{
}

void MainWindow::on_btnSettings_clicked()
{
    SettingsDialog dlg(this);
    dlg.exec();
}