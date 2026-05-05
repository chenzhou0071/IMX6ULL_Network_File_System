#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "filemanagerwindow.h"
#include "systemmonitorwindow.h"
#include "logviewerwindow.h"
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QAction>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>
#include <QFont>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_network(new NetworkManager(this))
    , m_serverPort(8888)
{
    ui->setupUi(this);
    setWindowTitle("嵌入式设备管理器");
    resize(700, 600);

    // 设置窗口背景色（浅灰白）
    setStyleSheet("QMainWindow { background-color: #FAFAFA; }");

    // 动态设置按钮尺寸
    QSize btnSize(240, 80);
    ui->btnFileManager->setMinimumSize(btnSize);
    ui->btnSystemMonitor->setMinimumSize(btnSize);
    ui->btnLogViewer->setMinimumSize(btnSize);
    ui->btnSettings->setMinimumSize(btnSize);

    // 设置按钮样式（浅蓝灰底、深灰圆角文字）
    QString btnStyle = R"(
        QPushButton {
            background-color: #607D8B;
            color: white;
            border-radius: 12px;
            font-size: 16px;
            font-weight: bold;
            padding: 10px;
            border: none;
        }
        QPushButton:hover {
            background-color: #455A64;
        }
        QPushButton:pressed {
            background-color: #37474F;
        }
    )";
    ui->btnFileManager->setStyleSheet(btnStyle);
    ui->btnSystemMonitor->setStyleSheet(btnStyle);
    ui->btnLogViewer->setStyleSheet(btnStyle);
    ui->btnSettings->setStyleSheet(btnStyle);

    // 设置状态栏标签样式
    m_labelStatus = new QLabel("○ 未连接", this);
    m_labelServer = new QLabel("服务器: -- | 用户: --", this);
    m_labelStatus->setStyleSheet(R"(
        padding: 8px 15px;
        font-size: 14px;
        color: #607D8B;
        font-weight: bold;
        background-color: white;
        border-radius: 5px;
        border: 1px solid #B0BEC5;
    )");
    m_labelServer->setStyleSheet(R"(
        padding: 8px 15px;
        font-size: 14px;
        color: #455A64;
        background-color: white;
        border-radius: 5px;
        border: 1px solid #B0BEC5;
    )");
    ui->statusbar->setStyleSheet(R"(
        QStatusBar {
            background-color: #455A64;
            color: white;
            padding: 5px;
        }
    )");
    ui->statusbar->addWidget(m_labelStatus);
    ui->statusbar->addWidget(m_labelServer);

    // 设置菜单栏样式
    ui->menubar->setStyleSheet(R"(
        QMenuBar {
            background-color: #FAFAFA;
            padding: 5px;
        }
        QMenuBar::item {
            background-color: #FAFAFA;
            padding: 5px 15px;
            color: #455A64;
            font-weight: bold;
        }
        QMenuBar::item:selected {
            background-color: #607D8B;
            color: white;
        }
        QMenu {
            background-color: #FAFAFA;
            border: 1px solid #B0BEC5;
            padding: 5px;
        }
        QMenu::item {
            padding: 5px 20px;
            color: #455A64;
        }
        QMenu::item:selected {
            background-color: #607D8B;
            color: white;
        }
    )");

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
            m_labelStatus->setText("● 已登录");
            m_labelStatus->setStyleSheet(R"(
                padding: 8px 15px;
                font-size: 14px;
                color: #27AE60;
                font-weight: bold;
                background-color: #E8F5E9;
                border-radius: 5px;
                border: 1px solid #27AE60;
            )");
            m_labelServer->setText(QString("服务器: %1:%2 | 用户: %3")
                                     .arg(m_serverIp).arg(m_serverPort).arg(m_username));
            ui->actionConnect->setEnabled(false);
            ui->actionDisconnect->setEnabled(true);
        } else {
            m_labelStatus->setText("● 已连接");
            m_labelStatus->setStyleSheet(R"(
                padding: 8px 15px;
                font-size: 14px;
                color: #607D8B;
                font-weight: bold;
                background-color: white;
                border-radius: 5px;
                border: 1px solid #B0BEC5;
            )");
            m_labelServer->setText(QString("服务器: %1:%2 | 用户: --").arg(m_serverIp).arg(m_serverPort));
        }
    } else {
        m_labelStatus->setText("○ 未连接");
        m_labelStatus->setStyleSheet(R"(
            padding: 8px 15px;
            font-size: 14px;
            color: #9E9E9E;
            font-weight: bold;
            background-color: white;
            border-radius: 5px;
            border: 1px solid #B0BEC5;
        )");
        m_labelServer->setText("服务器: -- | 用户: --");
        ui->actionConnect->setEnabled(true);
        ui->actionDisconnect->setEnabled(false);
    }
}

void MainWindow::showSettingsDialog()
{
    // 如果已连接且已认证，直接返回
    if (m_network->isConnected() && m_network->isAuthenticated()) {
        return;
    }

    // 如果已连接但未认证，先断开旧连接
    if (m_network->isConnected()) {
        m_network->disconnect();
    }

    SettingsDialog dialog(this);
    dialog.setServerInfo(m_serverIp, m_serverPort, m_username);
    if (dialog.exec() == QDialog::Accepted) {
        m_serverIp = dialog.serverIp();
        m_serverPort = dialog.port();
        m_username = dialog.username();
        QString password = dialog.password();

        // 连接服务器
        m_network->connectToServer(m_serverIp, m_serverPort);

        // 等待连接成功后自动登录（使用信号连接，确保只触发一次）
        QObject::connect(m_network, &NetworkManager::connected, this, [this, password]() {
            m_network->sendLogin(m_username, password);
        }, Qt::SingleShotConnection);
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
    if (!m_network->isAuthenticated()) {
        QMessageBox::warning(this, "提示", "请先登录服务器");
        return;
    }
    LogViewerWindow* window = new LogViewerWindow(m_network, this);
    window->show();
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
        m_labelStatus->setText("● 已登录");
        m_labelStatus->setStyleSheet(R"(
            padding: 8px 15px;
            font-size: 14px;
            color: #27AE60;
            font-weight: bold;
            background-color: #E8F5E9;
            border-radius: 5px;
            border: 1px solid #27AE60;
        )");
        // 登录成功后启动心跳，保持连接活跃
        m_network->startHeartbeat(20000);  // 20秒心跳
        // 立即发送一次心跳，避免等待
        m_network->sendHeartbeat();
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
