#include "QtRmlFileInterface.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cstdio>
#include <limits>
#include <memory>

namespace ingnomia::ui
{
namespace
{
struct FileHandle
{
    explicit FileHandle( const QString& path ) : file( path ) {}
    QFile file;
};

QString fromRml( const Rml::String& value )
{
    return QString::fromUtf8( value.data(), static_cast<qsizetype>( value.size() ) );
}
} // namespace

QtRmlFileInterface::QtRmlFileInterface( QString assetRoot, QString fallbackAssetRoot )
{
    const QFileInfo rootInfo( std::move( assetRoot ) );
    m_assetRoot = rootInfo.canonicalFilePath();
    if ( m_assetRoot.isEmpty() || !QFileInfo( m_assetRoot ).isDir() )
        qCritical() << "RmlUi asset root does not exist or is not a directory:" << rootInfo.absoluteFilePath();
	const QFileInfo fallbackInfo( std::move( fallbackAssetRoot ) );
	if ( fallbackInfo.exists() && fallbackInfo.isDir() )
		m_fallbackAssetRoot = fallbackInfo.canonicalFilePath();
}

bool QtRmlFileInterface::valid() const noexcept { return !m_assetRoot.isEmpty(); }
const QString& QtRmlFileInterface::assetRoot() const noexcept { return m_assetRoot; }

QString QtRmlFileInterface::resolve( const Rml::String& logicalPath ) const
{
    if ( !valid() ) return {};
    QString logical = QDir::fromNativeSeparators( fromRml( logicalPath ) );
    while ( logical.startsWith( '/' ) ) logical.remove( 0, 1 );
	const auto resolveUnder = [&]( const QString& root ) -> QString
	{
		if ( root.isEmpty() ) return {};
		const QFileInfo candidateInfo( QDir( root ).filePath( QDir::cleanPath( logical ) ) );
		const QString candidate = candidateInfo.canonicalFilePath();
		if ( candidate.isEmpty() ) return {};
		const QString rootPrefix = QDir::cleanPath( root ) + '/';
    const Qt::CaseSensitivity sensitivity =
#ifdef Q_OS_WIN
        Qt::CaseInsensitive;
#else
        Qt::CaseSensitive;
#endif
		if ( candidate.compare( root, sensitivity ) != 0 && !candidate.startsWith( rootPrefix, sensitivity ) )
    {
        qWarning() << "Rejected RmlUi resource outside asset root:" << logical << "resolved to" << candidate;
        return {};
    }
		return candidate;
	};
	if ( const QString primary = resolveUnder( m_assetRoot ); !primary.isEmpty() ) return primary;
	return resolveUnder( m_fallbackAssetRoot );
}

Rml::FileHandle QtRmlFileInterface::Open( const Rml::String& path )
{
    const QString resolved = resolve( path );
    if ( resolved.isEmpty() )
    {
        qWarning() << "RmlUi resource is missing or rejected:" << fromRml( path );
        return {};
    }
    auto handle = std::make_unique<FileHandle>( resolved );
    if ( !handle->file.open( QIODevice::ReadOnly ) )
    {
        qWarning() << "Failed to open RmlUi resource:" << fromRml( path ) << "resolved to" << resolved;
        return {};
    }
    return reinterpret_cast<Rml::FileHandle>( handle.release() );
}

void QtRmlFileInterface::Close( Rml::FileHandle file )
{
    delete reinterpret_cast<FileHandle*>( file );
}

size_t QtRmlFileInterface::Read( void* buffer, size_t size, Rml::FileHandle file )
{
    if ( !file || !buffer || size == 0 ) return 0;
    const qint64 requested = static_cast<qint64>( qMin<size_t>( size, static_cast<size_t>( std::numeric_limits<qint64>::max() ) ) );
    const qint64 result = reinterpret_cast<FileHandle*>( file )->file.read( static_cast<char*>( buffer ), requested );
    return result > 0 ? static_cast<size_t>( result ) : 0;
}

bool QtRmlFileInterface::Seek( Rml::FileHandle file, long offset, int origin )
{
    if ( !file ) return false;
    QFile& stream = reinterpret_cast<FileHandle*>( file )->file;
    qint64 position = static_cast<qint64>( offset );
    if ( origin == SEEK_CUR ) position += stream.pos();
    else if ( origin == SEEK_END ) position += stream.size();
    else if ( origin != SEEK_SET ) return false;
    return position >= 0 && stream.seek( position );
}

size_t QtRmlFileInterface::Tell( Rml::FileHandle file )
{
    if ( !file ) return 0;
    const qint64 position = reinterpret_cast<FileHandle*>( file )->file.pos();
    return position >= 0 ? static_cast<size_t>( position ) : 0;
}

size_t QtRmlFileInterface::Length( Rml::FileHandle file )
{
    if ( !file ) return 0;
    const qint64 length = reinterpret_cast<FileHandle*>( file )->file.size();
    return length >= 0 ? static_cast<size_t>( length ) : 0;
}
} // namespace ingnomia::ui
