#include "transferprogressdialog.h"
#include "ui_transferprogressdialog.h"
#include <QPushButton>
#include <QProgressBar>
#include <QHBoxLayout>

TransferProgressDialog::TransferProgressDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TransferProgressDialog)
{
    ui->setupUi(this);
    setWindowTitle("文件传输");
    resize(700, 400);

    // 设置表格列宽
    ui->tableTransfers->setColumnWidth(0, 150);  // 文件名
    ui->tableTransfers->setColumnWidth(1, 60);   // 方向
    ui->tableTransfers->setColumnWidth(2, 80);   // 大小
    ui->tableTransfers->setColumnWidth(3, 150);  // 进度
    ui->tableTransfers->setColumnWidth(4, 80);   // 速度
    ui->tableTransfers->setColumnWidth(5, 80);   // 状态

    // 允许选择行
    ui->tableTransfers->setSelectionBehavior(QAbstractItemView::SelectRows);
}

TransferProgressDialog::~TransferProgressDialog()
{
    delete ui;
}

void TransferProgressDialog::addTask(const QString& fileName, bool isUpload, quint32 totalSize)
{
    TransferTask task;
    task.fileName = fileName;
    task.isUpload = isUpload;
    task.totalSize = totalSize;
    task.transferred = 0;
    task.speed = 0;
    task.status = "等待中";

    m_tasks.append(task);
    int row = ui->tableTransfers->rowCount();
    ui->tableTransfers->insertRow(row);
    updateTableRow(row, task);
}

void TransferProgressDialog::updateTask(int index, quint32 transferred, quint32 speed, const QString& status)
{
    if (index < 0 || index >= m_tasks.size()) return;

    m_tasks[index].transferred = transferred;
    m_tasks[index].speed = speed;
    m_tasks[index].status = status;

    updateTableRow(index, m_tasks[index]);
}

void TransferProgressDialog::removeTask(int index)
{
    if (index < 0 || index >= m_tasks.size()) return;

    m_tasks.removeAt(index);
    ui->tableTransfers->removeRow(index);
}

void TransferProgressDialog::clearCompletedTasks()
{
    for (int i = m_tasks.size() - 1; i >= 0; i--) {
        if (m_tasks[i].status == "已完成" || m_tasks[i].status == "已取消") {
            m_tasks.removeAt(i);
            ui->tableTransfers->removeRow(i);
        }
    }
}

void TransferProgressDialog::updateTableRow(int row, const TransferTask& task)
{
    // 文件名
    ui->tableTransfers->setItem(row, 0, new QTableWidgetItem(task.fileName));

    // 方向
    ui->tableTransfers->setItem(row, 1, new QTableWidgetItem(task.isUpload ? "上传" : "下载"));

    // 大小
    ui->tableTransfers->setItem(row, 2, new QTableWidgetItem(formatSize(task.totalSize)));

    // 进度条
    QWidget* progressWidget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(progressWidget);
    layout->setContentsMargins(2, 2, 2, 2);

    QProgressBar* progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    int percent = task.totalSize > 0 ? (task.transferred * 100 / task.totalSize) : 0;
    progressBar->setValue(percent);
    progressBar->setTextVisible(true);

    layout->addWidget(progressBar);
    ui->tableTransfers->setCellWidget(row, 3, progressWidget);

    // 速度
    ui->tableTransfers->setItem(row, 4, new QTableWidgetItem(formatSpeed(task.speed)));

    // 状态
    ui->tableTransfers->setItem(row, 5, new QTableWidgetItem(task.status));
}

QString TransferProgressDialog::formatSize(quint32 size)
{
    if (size < 1024) return QString("%1 B").arg(size);
    if (size < 1024 * 1024) return QString("%1 KB").arg(size / 1024);
    return QString("%1 MB").arg(size / (1024 * 1024));
}

QString TransferProgressDialog::formatSpeed(quint32 speed)
{
    if (speed < 1024) return QString("%1 B/s").arg(speed);
    if (speed < 1024 * 1024) return QString("%1 KB/s").arg(speed / 1024);
    return QString("%1 MB/s").arg(speed / (1024 * 1024));
}