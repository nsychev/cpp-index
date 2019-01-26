#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QTreeWidgetItem>
#include "indexer.hpp"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* = nullptr);
    ~MainWindow() override;

public slots:
    void fileChanged(QString);

private slots:
    void selectDirectory();
    void indexDirectory();
    void search();

    void cancelJob();

    void updateProgressBar(int);
    void updateStatus(QString);

    void initIndex();
    void initSearch();

    void openFile(QTreeWidgetItem*, int);
    void getFile(QString);

    void getIOError(QString);
signals:
    void lineEditClicked();
    void sendQuery(QString);

protected:
    bool eventFilter(QObject*, QEvent*) override;

private:
    Ui::MainWindow *ui;
    QString path;

    Indexer *indexer = nullptr;
    QThread *job = nullptr;

    QMap<QByteArray, QTreeWidgetItem*> nodes;
};

#endif // MAINWINDOW_H
