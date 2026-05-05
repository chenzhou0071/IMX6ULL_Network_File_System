#ifndef FILEVIEWERDIALOG_H
#define FILEVIEWERDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class FileViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileViewerDialog(const QString& fileName, const QString& content, QWidget *parent = nullptr);
    ~FileViewerDialog();

private:
    QString m_fileName;
    QString m_content;
    QTextEdit* m_textEdit;
};

#endif // FILEVIEWERDIALOG_H
