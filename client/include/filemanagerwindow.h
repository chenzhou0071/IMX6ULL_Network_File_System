#ifndef FILEMANAGERWINDOW_H
#define FILEMANAGERWINDOW_H

#include <QMainWindow>
#include "networkmanager.h"

namespace Ui {
class FileManagerWindow;
}

class FileManagerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit FileManagerWindow(NetworkManager* network, QWidget *parent = nullptr);
    ~FileManagerWindow();

private slots:
    void onFileListResult(const QList<FileInfo>& files);
    void onFileReadResult(bool success, const QString& content, const QString& message);
    void onFileOperationResult(bool success, const QString& message);
    void onUploadProgress(const QString& fileName, quint32 transferred, quint32 total);
    void onDownloadProgress(const QString& fileName, quint32 transferred, quint32 total);
    void onTransferComplete(const QString& fileName, bool success);

    void onBtnBackClicked();
    void onBtnRefreshClicked();
    void onBtnUploadClicked();
    void onBtnDownloadClicked();
    void onBtnDeleteClicked();
    void onBtnViewClicked();
    void onBtnRenameClicked();
    void onTableItemDoubleClicked(int row, int column);
    void onTableSelectionChanged();

private:
    Ui::FileManagerWindow *ui;
    NetworkManager* m_network;
    QString m_currentPath;
    QList<FileInfo> m_files;

    void loadFileList(const QString& path);
    void updateStatusBar();
    QString formatSize(quint32 size);
    QString formatTime(quint32 time);
    void uploadFiles(const QStringList& files);
    void downloadFile(const QString& remotePath, const QString& localPath);
};

#endif // FILEMANAGERWINDOW_H
