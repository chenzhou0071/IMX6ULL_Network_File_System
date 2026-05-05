#include "logviewerwindow.h"
#include "ui_logviewerwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

LogViewerWindow::LogViewerWindow(NetworkManager* network, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LogViewerWindow)
    , m_network(network)
    , m_logDir("/logs")
{
    ui->setupUi(this);
    setWindowTitle("日志查看器");

    // 设置窗口背景色
    setStyleSheet("QMainWindow { background-color: #FAFAFA; }");

    // 动态设置按钮尺寸
    QSize btnSize(80, 35);
    ui->btnRefresh->setMinimumSize(btnSize);
    ui->btnClear->setMinimumSize(btnSize);
    ui->btnSave->setMinimumSize(btnSize);

    // 统一按钮样式（蓝灰）
    QString btnStyle = R"(
        QPushButton {
            background-color: #607D8B;
            color: white;
            border-radius: 8px;
            font-size: 13px;
            padding: 6px 12px;
            border: none;
        }
        QPushButton:hover {
            background-color: #455A64;
        }
        QPushButton:pressed {
            background-color: #37474F;
        }
    )";
    ui->btnRefresh->setStyleSheet(btnStyle);
    ui->btnClear->setStyleSheet(btnStyle);
    ui->btnSave->setStyleSheet(btnStyle);

    // 下拉框样式
    ui->comboLogFile->setStyleSheet(R"(
        QComboBox {
            padding: 6px 10px;
            border: 1px solid #B0BEC5;
            border-radius: 5px;
            background-color: white;
        }
        QComboBox:focus {
            border: 2px solid #607D8B;
        }
        QComboBox::drop-down {
            border: none;
        }
    )");

    // 日志文本框样式
    ui->textLog->setStyleSheet(R"(
        QTextEdit {
            background-color: white;
            border: 2px solid #B0BEC5;
            border-radius: 8px;
            padding: 10px;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            selection-background-color: #607D8B;
        }
    )");

    // 状态栏样式
    ui->statusbar->setStyleSheet(R"(
        QStatusBar {
            background-color: #455A64;
            color: white;
            padding: 5px;
        }
    )");

    // 信号连接
    connect(m_network, &NetworkManager::fileReadResult, this, &LogViewerWindow::onFileReadResult);
    connect(m_network, &NetworkManager::logFileListResult, this, &LogViewerWindow::onLogFileListResult);

    connect(ui->btnRefresh, &QPushButton::clicked, this, &LogViewerWindow::onBtnRefreshClicked);
    connect(ui->btnClear, &QPushButton::clicked, this, &LogViewerWindow::onBtnClearClicked);
    connect(ui->btnSave, &QPushButton::clicked, this, &LogViewerWindow::onBtnSaveClicked);

    connect(ui->comboLogFile, &QComboBox::currentTextChanged, this, &LogViewerWindow::onComboLogFileChanged);

    // 初始请求日志目录文件列表
    requestLogFiles();
}

LogViewerWindow::~LogViewerWindow()
{
    delete ui;
}

void LogViewerWindow::requestLogFiles()
{
    ui->textLog->clear();
    ui->textLog->append(QString("=== 正在加载日志目录: %1 ===").arg(m_logDir));
    ui->comboLogFile->clear();

    // 发送文件列表请求
    m_network->sendLogFileList(m_logDir);
}

void LogViewerWindow::onLogFileListResult(const QList<FileInfo>& files)
{
    m_logFiles = files;
    ui->comboLogFile->clear();

    for (const FileInfo& info : files) {
        if (!info.isDir) {
            ui->comboLogFile->addItem(m_logDir + "/" + info.name);
        }
    }

    if (ui->comboLogFile->count() > 0) {
        ui->textLog->append(QString("找到 %1 个日志文件\n").arg(ui->comboLogFile->count()));
        loadLog(ui->comboLogFile->currentText());
    } else {
        ui->textLog->append("\n未找到日志文件");
    }
}

void LogViewerWindow::loadLog(const QString& path)
{
    if (path.isEmpty()) return;

    m_currentLogPath = path;
    ui->textLog->append(QString("\n=== 正在加载: %1 ===").arg(path));

    // 请求读取日志文件
    m_network->sendFileRead(path);
}

void LogViewerWindow::onBtnRefreshClicked()
{
    // 重新加载目录
    requestLogFiles();
}

void LogViewerWindow::onBtnClearClicked()
{
    ui->textLog->clear();
}

void LogViewerWindow::onBtnSaveClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "保存日志",
        QString("log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "文本文件 (*.txt);;所有文件 (*)"
    );

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开文件进行写入");
        return;
    }

    QTextStream out(&file);
    out << ui->textLog->toPlainText();
    file.close();

    QMessageBox::information(this, "成功", QString("日志已保存到: %1").arg(filePath));
}

void LogViewerWindow::onComboLogFileChanged(const QString& fileName)
{
    loadLog(fileName);
}

void LogViewerWindow::onFileReadResult(bool success, const QString& content, const QString& message)
{
    if (success) {
        appendLogText(content);
    } else {
        appendLogText(QString("--- 读取失败: %1 ---").arg(message));
    }
}

void LogViewerWindow::appendLogText(const QString& text)
{
    ui->textLog->append(text);
    // 自动滚动到底部
    QTextCursor cursor = ui->textLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textLog->setTextCursor(cursor);
}
