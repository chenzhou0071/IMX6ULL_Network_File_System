#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QDebug>

// 日志文件路径
static QString logFilePath;

// 自定义消息处理函数
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QFile file(logFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        QString level;

        switch (type) {
            case QtDebugMsg:    level = "DEBUG"; break;
            case QtInfoMsg:     level = "INFO"; break;
            case QtWarningMsg:  level = "WARN"; break;
            case QtCriticalMsg: level = "ERROR"; break;
            case QtFatalMsg:    level = "FATAL"; break;
        }

        out << "[" << timestamp << "] [" << level << "] " << msg << "\n";
        file.close();
    }
}

int main(int argc, char *argv[])
{
    // 创建日志目录
    QDir logDir("logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    // 设置日志文件路径（每天一个文件）
    logFilePath = QString("logs/client_%1.log")
                      .arg(QDateTime::currentDateTime().toString("yyyyMMdd"));

    // 安装自定义消息处理器
    qInstallMessageHandler(customMessageHandler);

    qDebug() << "=== Client started ===";

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
