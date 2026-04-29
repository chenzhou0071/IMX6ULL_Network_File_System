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

private:
    Ui::LogViewerWindow *ui;
    NetworkManager* m_network;
    QString m_currentLogPath;

    void loadLog(const QString& path);
    void appendLogText(const QString& text);
};

#endif // LOGVIEWERWINDOW_H