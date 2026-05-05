#ifndef LOGVIEWERWINDOW_H
#define LOGVIEWERWINDOW_H

#include <QMainWindow>
#include "networkmanager.h"

namespace Ui {
class LogViewerWindow;
}

class LogViewerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LogViewerWindow(NetworkManager* network, QWidget *parent = nullptr);
    ~LogViewerWindow();

private slots:
    void onBtnRefreshClicked();
    void onBtnClearClicked();
    void onBtnSaveClicked();
    void onComboLogFileChanged(const QString& fileName);
    void onFileReadResult(bool success, const QString& content, const QString& message);
    void onLogFileListResult(const QList<FileInfo>& files);

private:
    Ui::LogViewerWindow *ui;
    NetworkManager* m_network;
    QString m_currentLogPath;
    QString m_logDir;
    QList<FileInfo> m_logFiles;

    void loadLog(const QString& path);
    void appendLogText(const QString& text);
    void requestLogFiles();
};

#endif // LOGVIEWERWINDOW_H