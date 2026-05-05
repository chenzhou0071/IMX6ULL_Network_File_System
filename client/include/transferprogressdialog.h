#ifndef TRANSFERPROGRESSDIALOG_H
#define TRANSFERPROGRESSDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QList>

struct TransferTask {
    QString fileName;
    bool isUpload;      // true=上传, false=下载
    quint32 totalSize;
    quint32 transferred;
    quint32 speed;       // bytes/sec
    QString status;      // 等待中/传输中/已完成/已取消
};

namespace Ui {
class TransferProgressDialog;
}

class TransferProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TransferProgressDialog(QWidget *parent = nullptr);
    ~TransferProgressDialog();

    void addTask(const QString& fileName, bool isUpload, quint32 totalSize);
    void updateTask(int index, quint32 transferred, quint32 speed, const QString& status);
    void removeTask(int index);
    void clearCompletedTasks();

private:
    Ui::TransferProgressDialog *ui;
    QList<TransferTask> m_tasks;

    QString formatSize(quint32 size);
    QString formatSpeed(quint32 speed);
    void updateTableRow(int row, const TransferTask& task);
};

#endif // TRANSFERPROGRESSDIALOG_H