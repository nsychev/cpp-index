#ifndef INDEXER_HPP
#define INDEXER_HPP

#include <QObject>
#include <map>
#include <set>
#include <QString>
#include <QFileSystemWatcher>
#include <QThread>


const uint32_t BLOCK_SIZE = 8192;


class Indexer : public QObject
{
    Q_OBJECT
public:
    Indexer(QString path, QThread *parent);
    ~Indexer();

public slots:
    void indexDirectory();
    void search();
    void changed(QString);
    void setQuery(QString);

signals:
    void initIndex();
    void initSearch();

    void setProgressBar(int progress);
    void setStatus(QString message);
    void sendFile(QString path);
    void sendChanged(QString path);

    void sendIOError(QString path);

private:
    QFileSystemWatcher watcher;
    QString path;
    QThread* baseThread;
    QString query;

    // path: set<trigrams>
    std::map<QString, std::set<uint32_t>> index;

    QSet<uint32_t> getTrigrams(QString data);
    bool isTextData(const QByteArray&);
    std::set<uint32_t> splitTrigrams(const QByteArray&);

    qint64 totalFiles = 0;
};

#endif // INDEXER_HPP
