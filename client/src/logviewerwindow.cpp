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
{
    ui->setupUi(this);
    setWindowTitle("日志查看器");

    // 信号连接
    connect(m_network, &NetworkManager::fileReadResult, this, &LogViewerWindow::onFileReadResult);

    connect(ui->btnRefresh, &QPushButton::clicked, this, &LogViewerWindow::onBtnRefreshClicked);
    connect(ui->btnClear, &QPushButton::clicked, this, &LogViewerWindow::onBtnClearClicked);
    connect(ui->btnSave, &QPushButton::clicked, this, &LogViewerWindow::onBtnSaveClicked);
    connect(ui->comboLogFile, &QComboBox::currentTextChanged, this, &LogViewerWindow::onComboLogFileChanged);

    // 设置日志文件列表
    ui->comboLogFile->clear();
    ui->comboLogFile->addItem("/logs/server.log");
    ui->comboLogFile->addItem("/logs/error.log");
    ui->comboLogFile->addItem("/logs/access.log");

    // 自动加载第一个
    if (ui->comboLogFile->count() > 0) {
        m_currentLogPath = ui->comboLogFile->currentText();
        loadLog(m_currentLogPath);
    }
}

LogViewerWindow::~LogViewerWindow()
{
    delete ui;
}

void LogViewerWindow::loadLog(const QString& path)
{
    m_currentLogPath = path;
    ui->textLog->append(QString("=== 正在加载: %1 ===").arg(path));
    ui->textLog->append("");

    // 请求读取日志文件
    m_network->sendFileRead(path);
}

void LogViewerWindow::onBtnRefreshClicked()
{
    loadLog(m_currentLogPath);
}

void LogViewerWindow::onBtnClearClicked()
{
    ui->textLog->clear();
}

void LogViewerWindow::onBtnSaveClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "保存日志",
        QString("log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
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

    QMessageBox::information(this, "��功", QString("日志已保存到: %1").arg(filePath));
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