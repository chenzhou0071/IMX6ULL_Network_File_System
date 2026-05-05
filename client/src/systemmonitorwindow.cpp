#include "systemmonitorwindow.h"
#include "ui_systemmonitorwindow.h"
#include <QTableWidgetItem>
#include <QDateTime>

SystemMonitorWindow::SystemMonitorWindow(NetworkManager* network, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::SystemMonitorWindow)
    , m_network(network)
    , m_updateTimer(new QTimer(this))
    , m_isMonitoring(false)
{
    ui->setupUi(this);
    setWindowTitle("系统监控");

    // 设置窗口背景色
    setStyleSheet("QMainWindow { background-color: #FAFAFA; }");

    // 动态设置按钮尺寸
    QSize btnSize(100, 40);
    ui->btnStartMonitor->setMinimumSize(btnSize);
    ui->btnStopMonitor->setMinimumSize(btnSize);
    ui->btnRefresh->setMinimumSize(btnSize);

    // 统一按钮样式（蓝灰）
    QString btnStyle = R"(
        QPushButton {
            background-color: #607D8B;
            color: white;
            border-radius: 8px;
            font-size: 13px;
            font-weight: bold;
            padding: 8px 16px;
            border: none;
        }
        QPushButton:hover {
            background-color: #455A64;
        }
        QPushButton:pressed {
            background-color: #37474F;
        }
    )";
    ui->btnStartMonitor->setStyleSheet(btnStyle);
    ui->btnStopMonitor->setStyleSheet(btnStyle);
    ui->btnRefresh->setStyleSheet(btnStyle);

    // 动态设置进度条高度
    ui->progressCpu->setMinimumHeight(35);
    ui->progressMem->setMinimumHeight(35);
    ui->progressDisk->setMinimumHeight(35);

    // 进度条样式（蓝灰）
    QString progressStyle = R"(
        QProgressBar {
            border: 2px solid #B0BEC5;
            border-radius: 8px;
            text-align: center;
            background-color: #ECEFF1;
            height: 30px;
        }
        QProgressBar::chunk {
            background-color: #607D8B;
            border-radius: 6px;
        }
    )";
    ui->progressCpu->setStyleSheet(progressStyle);
    ui->progressMem->setStyleSheet(progressStyle);
    ui->progressDisk->setStyleSheet(progressStyle);

    // 分组框样式
    ui->tabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #B0BEC5;
            border-radius: 8px;
            background-color: white;
            padding: 10px;
        }
        QTabBar::tab {
            background-color: #ECEFF1;
            padding: 10px 20px;
            border-top-left-radius: 5px;
            border-top-right-radius: 5px;
            color: #455A64;
            font-weight: bold;
        }
        QTabBar::tab:selected {
            background-color: #607D8B;
            color: white;
        }
        QTabBar::tab:hover:!selected {
            background-color: #CFD8DC;
        }
        QGroupBox {
            border: 2px solid #B0BEC5;
            border-radius: 8px;
            margin-top: 10px;
            font-weight: bold;
            color: #455A64;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 2px 10px;
            background-color: #FAFAFA;
        }
    )");

    // 进程列表样式
    ui->tableProcesses->setStyleSheet(R"(
        QTableWidget {
            background-color: white;
            border: 1px solid #B0BEC5;
            border-radius: 8px;
            gridline-color: #ECEFF1;
        }
        QTableWidget::item {
            padding: 5px;
        }
        QTableWidget::item:selected {
            background-color: #455A64;
            color: white;
        }
        QHeaderView::section {
            background-color: #455A64;
            color: white;
            padding: 8px;
            border: none;
            font-weight: bold;
        }
    )");
    ui->tableProcesses->setAlternatingRowColors(true);

    // 状态栏样式
    ui->statusbar->setStyleSheet(R"(
        QStatusBar {
            background-color: #455A64;
            color: white;
            padding: 5px;
        }
    )");
    ui->labelUpdateTime->setStyleSheet("color: white; padding: 5px;");

    // 设置进程列表列宽
    ui->tableProcesses->setColumnWidth(0, 80);
    ui->tableProcesses->setColumnWidth(1, 150);
    ui->tableProcesses->setColumnWidth(2, 80);
    ui->tableProcesses->setColumnWidth(3, 60);
    ui->tableProcesses->setColumnWidth(4, 60);
    ui->tableProcesses->setColumnWidth(5, 60);
    ui->tableProcesses->setColumnWidth(6, 80);

    // 连接信号
    connect(m_network, &NetworkManager::sysInfoResult, this, &SystemMonitorWindow::onSysInfoResult);
    connect(m_network, &NetworkManager::procListResult, this, &SystemMonitorWindow::onProcListResult);

    connect(ui->btnStartMonitor, &QPushButton::clicked, this, &SystemMonitorWindow::onBtnStartClicked);
    connect(ui->btnStopMonitor, &QPushButton::clicked, this, &SystemMonitorWindow::onBtnStopClicked);
    connect(ui->btnRefresh, &QPushButton::clicked, this, &SystemMonitorWindow::onBtnRefreshClicked);
    connect(ui->spinInterval, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SystemMonitorWindow::onIntervalChanged);

    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        m_network->sendSysInfo();
        m_network->sendProcList();
    });

    // 初始请求
    refresh();
}

SystemMonitorWindow::~SystemMonitorWindow()
{
    delete ui;
}

void SystemMonitorWindow::onSysInfoResult(const SysInfo& info)
{
    updateSysInfo(info);
}

void SystemMonitorWindow::onProcListResult(const QList<ProcessInfo>& processes)
{
    updateProcList(processes);
    ui->labelUpdateTime->setText(QString("最后更新: %1 | 状态: %2")
                                  .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                                  .arg(m_isMonitoring ? "监控中" : "已停止"));
}

void SystemMonitorWindow::onBtnStartClicked()
{
    m_isMonitoring = true;
    m_updateTimer->start(ui->spinInterval->value() * 1000);
    ui->btnStartMonitor->setEnabled(false);
    ui->btnStopMonitor->setEnabled(true);
}

void SystemMonitorWindow::onBtnStopClicked()
{
    m_isMonitoring = false;
    m_updateTimer->stop();
    ui->btnStartMonitor->setEnabled(true);
    ui->btnStopMonitor->setEnabled(false);
}

void SystemMonitorWindow::onBtnRefreshClicked()
{
    refresh();
}

void SystemMonitorWindow::onIntervalChanged(int value)
{
    if (m_isMonitoring) {
        m_updateTimer->setInterval(value * 1000);
    }
}

void SystemMonitorWindow::refresh()
{
    m_network->sendSysInfo();
    m_network->sendProcList();
}

void SystemMonitorWindow::updateSysInfo(const SysInfo& info)
{
    // CPU使用率
    ui->progressCpu->setValue((int)info.cpuUsage);

    // 内存
    ui->labelMemTotalValue->setText(formatSize(info.memTotal));
    ui->labelMemFreeValue->setText(formatSize(info.memFree));

    int memUsedPercent = info.memTotal > 0 ?
        (int)((info.memTotal - info.memFree) * 100 / info.memTotal) : 0;
    ui->progressMem->setValue(memUsedPercent);

    // 磁盘
    ui->labelDiskTotalValue->setText(formatSize(info.diskTotal));
    ui->labelDiskFreeValue->setText(formatSize(info.diskFree));

    int diskUsedPercent = info.diskTotal > 0 ?
        (int)((info.diskTotal - info.diskFree) * 100 / info.diskTotal) : 0;
    ui->progressDisk->setValue(diskUsedPercent);

    // 运行时间
    ui->labelUptime->setText(formatUptime(info.uptime));
}

void SystemMonitorWindow::updateProcList(const QList<ProcessInfo>& processes)
{
    ui->tableProcesses->setRowCount(0);

    for (const ProcessInfo& info : processes) {
        int row = ui->tableProcesses->rowCount();
        ui->tableProcesses->insertRow(row);

        ui->tableProcesses->setItem(row, 0, new QTableWidgetItem(QString::number(info.pid)));
        ui->tableProcesses->setItem(row, 1, new QTableWidgetItem(info.name));
        ui->tableProcesses->setItem(row, 2, new QTableWidgetItem(info.user));
        ui->tableProcesses->setItem(row, 3, new QTableWidgetItem(QString::number(info.cpuUsage, 'f', 1)));
        ui->tableProcesses->setItem(row, 4, new QTableWidgetItem(QString::number(info.memUsage, 'f', 1)));
        ui->tableProcesses->setItem(row, 5, new QTableWidgetItem(info.state));
        ui->tableProcesses->setItem(row, 6, new QTableWidgetItem(QString::number(info.memSize)));
    }
}

QString SystemMonitorWindow::formatSize(quint32 sizeKB)
{
    // sizeKB 是 KB 为单位的值
    if (sizeKB < 1024) return QString("%1 KB").arg(sizeKB);
    if (sizeKB < 1024 * 1024) return QString("%1 MB").arg(sizeKB / 1024);
    return QString("%1 GB").arg(sizeKB / (1024 * 1024));
}

QString SystemMonitorWindow::formatUptime(quint32 seconds)
{
    quint32 days = seconds / 86400;
    quint32 hours = (seconds % 86400) / 3600;
    quint32 minutes = (seconds % 3600) / 60;

    if (days > 0) {
        return QString("%1 天 %2 小时 %3 分钟").arg(days).arg(hours).arg(minutes);
    } else if (hours > 0) {
        return QString("%1 小时 %2 分钟").arg(hours).arg(minutes);
    } else {
        return QString("%1 分钟").arg(minutes);
    }
}