#ifndef ARCHIVE_BACKEND_H
#define ARCHIVE_BACKEND_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <memory>
#include <functional>

struct ArchiveNode
{
    QString name;
    bool isDirectory = false;
    int64_t size = 0;
    uint16_t mode = 0;
};

struct ArchiveStats
{
    QString fileName;
    QString filePath;
    QString formatName;
    int64_t totalUncompressedSize = 0;
    int64_t totalCompressedSize = 0;
    size_t totalFilesCount = 0;
};

class ArchiveBackend
{
public:
    ArchiveBackend() = default;
    ~ArchiveBackend() = default;

    bool loadArchive(const QString& archivePath);
    QList<ArchiveNode> getDirectoryContents(const QString& virtualPath) const;
    bool extractFile(const QString& internalFilePath, const QString& destinationPath);

    bool extractAll(const QString& targetDir, std::function<bool(size_t, const QString&)> progressCallback = nullptr);

    bool addItems(const QStringList& absoluteFilePaths, const QString& internalVirtualTargetDir);
    bool removeItems(const QStringList& internalVirtualFilePaths);

    bool isDir(const QString& virtualPath) const;
    ArchiveStats getStats() const { return m_stats; }
    bool hasActiveArchive() const { return !m_archivePath.isEmpty(); }

private:
    QString m_archivePath;
    QMap<QString, QList<ArchiveNode>> m_fileSystemMap;
    QMap<QString, bool> m_isDirectoryMap;
    ArchiveStats m_stats;

    void parseArchive();
    QString cleanPath(const QString& path) const;
};

#endif      /* ARCHIVE_BACKEND_H */
