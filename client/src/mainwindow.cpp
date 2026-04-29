#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "filemanagerwindow.h"
#include "systemmonitorwindow.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_network(new NetworkManager(this))
    , m_serverPort(8888)
{
    ui->setupUi(this);
    setWindowTitle("嵌入式设备管理器");
    resize(700, 600);

    // 设置状态栏和标签
    m_labelStatus = new QLabel("连接状态: ○ 未连接", this);
    m_labelServer = new QLabel("服务器: -- | 用户: --", this);
    m_labelStatus->setStyleSheet("padding: 5px 10px; font-size: 14px;");
    m_labelServer->setStyleSheet("padding: 5px 10px; font-size: 14px;");
    ui->statusbar->addWidget(m_labelStatus);
    ui->statusbar->addWidget(m_labelServer);

    // 连接网络信号
    connect(m_network, &NetworkManager::connected, this, &MainWindow::onConnected);
    connect(m_network, &NetworkManager::disconnected, this, &MainWindow::onDisconnected);
    connect(m_network, &NetworkManager::connectionError, this, &MainWindow::onConnectionError);
    connect(m_network, &NetworkManager::loginResult, this, &MainWindow::onLoginResult);

    // 连接按钮信号
    connect(ui->btnFileManager, &QPushButton::clicked, this, &MainWindow::onBtnFileManagerClicked);
    connect(ui->btnSystemMonitor, &QPushButton::clicked, this, &MainWindow::onBtnSystemMonitorClicked);
    connect(ui->btnLogViewer, &QPushButton::clicked, this, &MainWindow::onBtnLogViewerClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &MainWindow::onBtnSettingsClicked);

    // 菜单信号
    connect(ui->actionConnect, &QAction::triggered, this, &MainWindow::onActionConnect);
    connect(ui->actionDisconnect, &QAction::triggered, this, &MainWindow::onActionDisconnect);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExit);

    // 禁用断开按钮
    ui->actionDisconnect->setEnabled(false);

    updateStatus();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateStatus()
{
    if (m_network->isConnected()) {
        if (m_network->isAuthenticated()) {
            m_labelStatus->setText("连接状态: ● 已登录");
            m_labelServer->setText(QString("服务器: %1:%2 | 用户: %3")
                                     .arg(m_serverIp).arg(m_serverPort).arg(m_username));
            ui->actionConnect->setEnabled(false);
            ui->actionDisconnect->setEnabled(true);
        } else {
            m_labelStatus->setText("连接状态: ● 已连接(未登录)");
            m_labelServer->setText(QString("服务器: %1:%2 | 用户: --").arg(m_serverIp).arg(m_serverPort));
        }
    } else {
        m_labelStatus->setText("连接状态: ○ 未连接");
        m_labelServer->setText("服务器: -- | 用户: --");
        ui->actionConnect->setEnabled(true);
        ui->actionDisconnect->setEnabled(false);
    }
}

void MainWindow::showSettingsDialog()
{
    SettingsDialog dialog(this);
    dialog.setServerInfo(m_serverIp, m_serverPort, m_username);
    if (dialog.exec() == QDialog::Accepted) {
        m_serverIp = dialog.serverIp();
        m_serverPort = dialog.port();
        m_username = dialog.username();
        QString password = dialog.password();

        // 连接服务器
        m_network->connectToServer(m_serverIp, m_serverPort);

        // 如果已经连接则直接登录，否则等待连接
        if (m_network->isConnected()) {
            m_network->sendLogin(m_username, password);
        } else {
            QObject::connect(m_network, &NetworkManager::connected, this, [this, password]() {
                m_network->sendLogin(m_username, password);
            });
        }
    }
}

void MainWindow::showFileManager()
{
    if (!m_network->isAuthenticated()) {
        QMessageBox::warning(this, "提示", "请先登录服务器");
        return;
    }
    FileManagerWindow* window = new FileManagerWindow(m_network, this);
    window->show();
}

void MainWindow::showSystemMonitor()
{
    if (!m_network->isAuthenticated()) {
        QMessageBox::warning(this, "提示", "请先登录服务器");
        return;
    }
    SystemMonitorWindow* window = new SystemMonitorWindow(m_network, this);
    window->show();
}

void MainWindow::showLogViewer()
{
    QMessageBox::information(this, "日志查看", "日志查看功能开发中...");
}

void MainWindow::onConnected()
{
    updateStatus();
}

void MainWindow::onDisconnected()
{
    updateStatus();
}

void MainWindow::onConnectionError(const QString& error)
{
    QMessageBox::critical(this, "连接错误", error);
    updateStatus();
}

void MainWindow::onLoginResult(bool success, int perm, const QString& message)
{
    if (success) {
        QMessageBox::information(this, "登录成功", message);
    } else {
        QMessageBox::warning(this, "登录失败", message);
    }
    updateStatus();
}

void MainWindow::onBtnFileManagerClicked()
{
    showFileManager();
}

void MainWindow::onBtnSystemMonitorClicked()
{
    showSystemMonitor();
}

void MainWindow::onBtnLogViewerClicked()
{
    showLogViewer();
}

void MainWindow::onBtnSettingsClicked()
{
    showSettingsDialog();
}

void MainWindow::onActionConnect()
{
    showSettingsDialog();
}

void MainWindow::onActionDisconnect()
{
    m_network->disconnect();
    updateStatus();
}

void MainWindow::onActionExit()
{
    close();
}
