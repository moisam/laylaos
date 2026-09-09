#include "archive_backend.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QMessageBox>

bool ArchiveBackend::loadArchive(const QString& archivePath)
{
    if (!QFileInfo::exists(archivePath)) return false;
    m_archivePath = archivePath;
    m_fileSystemMap.clear();
    m_isDirectoryMap.clear();

    // Reset structural analytics 
    m_stats = ArchiveStats();
    m_stats.filePath = archivePath;
    m_stats.fileName = QFileInfo(archivePath).fileName();

    parseArchive();
    return true;
}

QString ArchiveBackend::cleanPath(const QString& path) const
{
    QString cleaned = QDir::cleanPath(path);
    if(cleaned.startsWith("./")) cleaned = cleaned.mid(2);
    if(cleaned == "." || cleaned.isEmpty()) return "/";
    if(!cleaned.startsWith("/")) cleaned = "/" + cleaned;
    return cleaned;
}

static bool checkIfRawStream(QString& path)
{
    return (path.endsWith(".gz") && !path.endsWith(".tar.gz") && !path.endsWith(".tgz")) ||
           (path.endsWith(".bz2") && !path.endsWith(".tar.bz2") && !path.endsWith(".tbz2")) ||
           (path.endsWith(".xz") && !path.endsWith(".tar.xz") && !path.endsWith(".txz")) ||
           (path.endsWith(".zst") && !path.endsWith(".tar.zst") && !path.endsWith(".tzst"));
}

void ArchiveBackend::parseArchive()
{
    struct archive* a = archive_read_new();
    archive_read_support_filter_all(a);

    // Enable raw decompression formatting to recognize standalone .gz, .bz2, and .zst streams
    QString pathLower = m_archivePath.toLower();
    if(checkIfRawStream(pathLower))
         archive_read_support_format_raw(a);
    else archive_read_support_format_all(a);

    if(archive_read_open_filename(a, m_archivePath.toLocal8Bit().constData(), 10240) != ARCHIVE_OK)
    {
        archive_read_free(a);
        qDebug() << "backend: failed to parse archive";
        return;
    }

    struct archive_entry* entry;
    m_isDirectoryMap["/"] = true;

    while(archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        QString entryPath = QString::fromUtf8(archive_entry_pathname(entry));
        int64_t entrySize = archive_entry_size(entry);

        // Detect raw streams (which return a generic placeholder name)
        if(archive_format(a) == ARCHIVE_FORMAT_RAW || entryPath == "data")
        {
            // Deduce a clean uncompressed filename from the archive container name
            QFileInfo archiveInfo(m_archivePath);
            QString baseName = archiveInfo.fileName();
            if(baseName.endsWith(".gz", Qt::CaseInsensitive)) baseName.chop(3);
            else if(baseName.endsWith(".bz2", Qt::CaseInsensitive)) baseName.chop(4);
            else if(baseName.endsWith(".zst", Qt::CaseInsensitive)) baseName.chop(4);
            else if(baseName.endsWith(".xz", Qt::CaseInsensitive)) baseName.chop(3);

            if(entryPath == "data" && baseName != "data") entryPath = baseName;

            // Calculate the real uncompressed size by safely iterating the data stream
            entrySize = 0;
            const void* buff = nullptr;
            size_t size = 0;
            la_int64_t offset = 0;
            while(archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK)
            {
                entrySize += size;
            }

            // Since raw files don't store uncompressed sizing headers, 
            // look up the compressed file size on disk as a solid fallback metrics match
            if(entrySize <= 0)
            {
                entrySize = archiveInfo.size();
            }
            
            // Consume the remaining data bits to avoid corrupting the trailing reader stream state
            archive_read_data_skip(a);
        }

        QString fullPath = cleanPath(entryPath);
        bool isDirectory = (archive_entry_filetype(entry) == AE_IFDIR);
        uint16_t mode = archive_entry_mode(entry) & 0777;

        if(fullPath == "/") continue;

        m_isDirectoryMap[fullPath] = isDirectory;

        QFileInfo info(fullPath);
        QString parentPath = cleanPath(info.path());
        QString entryName = info.fileName();

        ArchiveNode node{entryName, isDirectory, entrySize, mode};
        m_fileSystemMap[parentPath].append(node);

        // Aggregate global property info
        if(!isDirectory)
        {
            m_stats.totalUncompressedSize += entrySize;
            m_stats.totalFilesCount++;
        }

        // Backfill intermediate virtual directories
        QString checkParent = parentPath;
        while(checkParent != "/")
        {
            m_isDirectoryMap[checkParent] = true;
            QFileInfo pInfo(checkParent);
            QString grandParent = cleanPath(pInfo.path());
            
            bool found = false;
            for(const auto& existing : m_fileSystemMap[grandParent])
            {
                if (existing.name == pInfo.fileName()) { found = true; break; }
            }

            if(!found)
            {
                m_fileSystemMap[grandParent].append({pInfo.fileName(), true, 0, 0755});
            }
            checkParent = grandParent;
        }
        archive_read_data_skip(a);
    }
    
    // Save layout profiles
    m_stats.formatName = QString::fromUtf8(archive_format_name(a));

    // Estimate total compressed payload from file properties
    m_stats.totalCompressedSize = QFileInfo(m_archivePath).size();

    archive_read_free(a);
}

bool ArchiveBackend::isDir(const QString& virtualPath) const
{
    return m_isDirectoryMap.value(cleanPath(virtualPath), false);
}

QList<ArchiveNode> ArchiveBackend::getDirectoryContents(const QString& virtualPath) const
{
    return m_fileSystemMap.value(cleanPath(virtualPath));
}

bool ArchiveBackend::extractFile(const QString& internalFilePath, const QString& destinationPath)
{
    if(m_archivePath.isEmpty()) return false;

    struct archive* a = archive_read_new();
    struct archive* ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM);
    archive_read_support_filter_all(a);

    // Enable raw decompression formatting to recognize standalone .gz, .bz2, and .zst streams
    QString pathLower = m_archivePath.toLower();
    if(checkIfRawStream(pathLower))
         archive_read_support_format_raw(a);
    else archive_read_support_format_all(a);

    if(archive_read_open_filename(a, m_archivePath.toLocal8Bit().constData(), 10240) != ARCHIVE_OK)
    {
        archive_read_free(a);
        archive_write_free(ext);
        return false;
    }

    struct archive_entry* entry;
    QString cleanTarget = cleanPath(internalFilePath);
    bool isTargetDir = isDir(internalFilePath);

    // Compute extraction target path to map tree structure onto the disk layout
    QFileInfo destInfo(destinationPath);
    QString baseDestDir = isTargetDir ? destinationPath : destInfo.absolutePath();

    while(archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        QString currentEntryPath = cleanPath(QString::fromUtf8(archive_entry_pathname(entry)));
        bool isRaw = (archive_format(a) == ARCHIVE_FORMAT_RAW || currentEntryPath == "data");
        bool match = false;

        if(isRaw)
        {
            // If it's a raw stream, match against our virtual calculated name rather than "data"
            QFileInfo archiveInfo(m_archivePath);
            QString expectedVirtualName = archiveInfo.fileName();
            if (expectedVirtualName.endsWith(".gz", Qt::CaseInsensitive)) expectedVirtualName.chop(3);
            else if (expectedVirtualName.endsWith(".bz2", Qt::CaseInsensitive)) expectedVirtualName.chop(4);
            else if (expectedVirtualName.endsWith(".zst", Qt::CaseInsensitive)) expectedVirtualName.chop(4);
            else if (expectedVirtualName.endsWith(".xz", Qt::CaseInsensitive)) expectedVirtualName.chop(3);

            if(cleanPath(internalFilePath) == cleanPath(expectedVirtualName))
            {
                match = true;
            }
        }
        else
        {
            // Standard multi-file match criteria
            if(isTargetDir)
            {
                if(currentEntryPath == cleanTarget || currentEntryPath.startsWith(cleanTarget + "/"))
                {
                    match = true;
                }
            }
            else
            {
                if(currentEntryPath == cleanTarget) match = true;
            }
        }

        if(!match)
        {
            archive_read_data_skip(a);
            continue;
        }

        // Map path to destination filesystem
        QString finalDiskPath = destinationPath;
        QDir().mkpath(QFileInfo(finalDiskPath).absolutePath());
        archive_entry_set_pathname(entry, finalDiskPath.toLocal8Bit().constData());

        if(archive_write_header(ext, entry) >= ARCHIVE_WARN)
        {
            const void *buff;
            size_t size;
            la_int64_t offset;

            while(archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK)
            {
                archive_write_data_block(ext, buff, size, offset);
            }

            archive_write_finish_entry(ext);
        }
    }

    archive_read_free(a);
    archive_write_free(ext);
    return true;
}

bool ArchiveBackend::extractAll(const QString& targetDir, 
                                std::function<bool(size_t, const QString&)> progressCallback)
{
    struct archive *a = archive_read_new();
    struct archive *ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | 
                                        ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);
    archive_read_support_filter_all(a);

    // Enable raw decompression formatting to recognize standalone .gz, .bz2, and .zst streams
    QString pathLower = m_archivePath.toLower();
    if(checkIfRawStream(pathLower))
         archive_read_support_format_raw(a);
    else archive_read_support_format_all(a);

    if(archive_read_open_filename(a, m_archivePath.toLocal8Bit().constData(), 10240) != ARCHIVE_OK)
    {
        archive_read_free(a);
        archive_write_free(ext);
        return false;
    }

    struct archive_entry* entry;
    int flags = ARCHIVE_OK;
    size_t processedCount = 0;

    while(archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        QString originalPath = QString::fromUtf8(archive_entry_pathname(entry));
        QString fullPath = targetDir + cleanPath(originalPath);

        // Trigger progress callback. If it returns false, the user pressed 'Cancel'.
        if(progressCallback)
        {
            if(!progressCallback(processedCount, originalPath))
            {
                archive_read_free(a);
                archive_write_free(ext);
                return false; // Early termination due to user cancelling
            }
        }

        archive_entry_set_pathname(entry, fullPath.toLocal8Bit().constData());

        flags = archive_write_header(ext, entry);
        if(flags >= ARCHIVE_WARN)
        {
            const void* buff;
            size_t size;
            la_int64_t offset;

            while(archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK)
            {
                archive_write_data_block(ext, buff, size, offset);
            }

            archive_write_finish_entry(ext);
        }

        processedCount++;
    }

    archive_read_free(a);
    archive_write_free(ext);
    return true;
}

/*
 * Add file(s) or folder(s) by creating a rewritten copy of the archive
 */
bool ArchiveBackend::addItems(const QStringList& absoluteFilePaths, const QString& internalVirtualTargetDir)
{
    if(m_archivePath.isEmpty()) return false;
    QString tempPath = m_archivePath + ".tmp";

    struct archive *old_a = archive_read_new();
    archive_read_support_filter_all(old_a);

    // Enable raw decompression formatting to recognize standalone .gz, .bz2, and .zst streams
    QString pathLower = m_archivePath.toLower();
    if(checkIfRawStream(pathLower))
         archive_read_support_format_raw(old_a);
    else archive_read_support_format_all(old_a);

    if(archive_read_open_filename(old_a, m_archivePath.toLocal8Bit().constData(), 10240) != ARCHIVE_OK)
    {
        archive_read_free(old_a);
        return false;
    }
    
    struct archive* new_a = archive_write_new();
    if(archive_write_set_format_filter_by_ext(new_a, m_archivePath.toLocal8Bit().constData()) != ARCHIVE_OK)
    {
        QMessageBox::critical(nullptr, "Unsupported Format", "Your system's libarchive build cannot write this format.");
        archive_read_free(old_a);
        archive_write_free(new_a);
        return false;
    }

    if(archive_write_open_filename(new_a, tempPath.toLocal8Bit().constData()) != ARCHIVE_OK)
    {
        archive_read_free(old_a);
        archive_write_free(new_a);
        return false;
    }

    // Copy existing archive records entry-by-entry
    struct archive_entry *entry;

    while(archive_read_next_header(old_a, &entry) == ARCHIVE_OK)
    {
        // Write the header metadata to the new stream
        int headerStatus = archive_write_header(new_a, entry);

        if(headerStatus >= ARCHIVE_OK)
        {
            const void* buff = nullptr;
            size_t size = 0;
            la_int64_t offset = 0;

            // Read blocks from old_a, but write sequentially to support compression
            while(archive_read_data_block(old_a, &buff, &size, &offset) == ARCHIVE_OK)
            {
                if(size > 0 && buff != nullptr)
                {
                    archive_write_data(new_a, buff, size);
                }
            }
        }
    }

    // Clean up the read handle after copying is completed
    archive_read_close(old_a);
    archive_read_free(old_a);

    // Append new local files or folders
    for(const auto& path : absoluteFilePaths)
    {
        QFileInfo sourceInfo(path);
        if(!sourceInfo.exists()) continue;

        QList<QPair<QString, QString>> itemsToPack;
        QString baseInternalPrefix = internalVirtualTargetDir;
        if(!baseInternalPrefix.endsWith("/")) baseInternalPrefix += "/";

        if(sourceInfo.isDir())
        {
            QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while(it.hasNext())
            {
                QString localPath = it.next();
                QString relativePath = sourceInfo.dir().relativeFilePath(localPath);
                itemsToPack.append(qMakePair(localPath, baseInternalPrefix + relativePath));
            }
        }
        else
        {
            itemsToPack.append(qMakePair(path, baseInternalPrefix + sourceInfo.fileName()));
        }

        for(const auto& item : itemsToPack)
        {
            QString localFilePath = item.first;
            QString archivePath = item.second;
            QFileInfo info(localFilePath);

            if(archivePath.startsWith("/")) archivePath = archivePath.mid(1);

            struct archive_entry *local_entry = archive_entry_new();
            archive_entry_set_pathname(local_entry, archivePath.toUtf8().constData());
            archive_entry_set_perm(local_entry, 0644);
            archive_entry_set_mtime(local_entry, info.lastModified().toSecsSinceEpoch(), 0);

            if(info.isDir())
            {
                archive_entry_set_filetype(local_entry, AE_IFDIR);
                archive_entry_set_size(local_entry, 0);
                archive_write_header(new_a, local_entry);
            }
            else
            {
                archive_entry_set_filetype(local_entry, AE_IFREG);
                archive_entry_set_size(local_entry, info.size());

                if(archive_write_header(new_a, local_entry) == ARCHIVE_OK)
                {
                    QFile file(localFilePath);
                    if(file.open(QIODevice::ReadOnly))
                    {
                        char chunkBuffer[4096];
                        qint64 readBytes = 0;
                        while((readBytes = file.read(chunkBuffer, sizeof(chunkBuffer))) > 0)
                        {
                            archive_write_data(new_a, chunkBuffer, readBytes);
                        }
                    }
                }
            }

            archive_entry_free(local_entry);
        }
    }
    
    // Flush footers and overwrite original path metadata
    archive_write_close(new_a);
    archive_write_free(new_a);

    QFile::remove(m_archivePath);
    QFile::rename(tempPath, m_archivePath);
    return loadArchive(m_archivePath); // Full reload
}

/*
 * Delete file(s) or folder(s) by omitting matched paths from the stream
 */
bool ArchiveBackend::removeItems(const QStringList& internalVirtualFilePaths)
{
    if(m_archivePath.isEmpty()) return false;
    QString tempPath = m_archivePath + ".tmp";

    // Allocate reading instance handle to ensure processing starts from entry index 0
    struct archive *old_a = archive_read_new();
    archive_read_support_filter_all(old_a);

    // Enable raw decompression formatting to recognize standalone .gz, .bz2, and .zst streams
    QString pathLower = m_archivePath.toLower();
    if(checkIfRawStream(pathLower))
         archive_read_support_format_raw(old_a);
    else archive_read_support_format_all(old_a);
    
    if(archive_read_open_filename(old_a, m_archivePath.toLocal8Bit().constData(), 10240) != ARCHIVE_OK)
    {
        archive_read_free(old_a);
        return false;
    }
    
    struct archive *new_a = archive_write_new();
    if(archive_write_set_format_filter_by_ext(new_a, m_archivePath.toLocal8Bit().constData()) != ARCHIVE_OK)
    {
        QMessageBox::critical(nullptr, "Unsupported Format", "Your system's libarchive build cannot write this format.");
        archive_read_free(old_a);
        archive_write_free(new_a);
        return false;
    }

    if(archive_write_open_filename(new_a, tempPath.toLocal8Bit().constData()) != ARCHIVE_OK)
    {
        archive_read_free(old_a);
        archive_write_free(new_a);
        return false;
    }

    struct archive_entry *entry;
    while(archive_read_next_header(old_a, &entry) == ARCHIVE_OK)
    {
        QString currentPath = cleanPath(QString::fromUtf8(archive_entry_pathname(entry)));
        
        bool shouldOmit = false;
        for(const auto& removeTarget : internalVirtualFilePaths)
        {
            QString cleanTarget = cleanPath(removeTarget);
            if(currentPath == cleanTarget || currentPath.startsWith(cleanTarget + "/"))
            {
                shouldOmit = true;
                break;
            }
        }

        if(shouldOmit)
        {
            archive_read_data_skip(old_a);
            continue;
        }

        // Write the header metadata to the new stream
        int headerStatus = archive_write_header(new_a, entry);

        if(headerStatus >= ARCHIVE_OK)
        {
            const void *buff = nullptr;
            size_t size = 0;
            la_int64_t offset = 0;

            // Read blocks from old_a, but write sequentially to support compression
            while(archive_read_data_block(old_a, &buff, &size, &offset) == ARCHIVE_OK)
            {
                if(size > 0 && buff != nullptr)
                {
                    archive_write_data(new_a, buff, size);
                }
            }
        }
    }

    archive_read_close(old_a);
    archive_read_free(old_a);
    archive_write_close(new_a);
    archive_write_free(new_a);

    QFile::remove(m_archivePath);
    QFile::rename(tempPath, m_archivePath);
    return loadArchive(m_archivePath);
}

