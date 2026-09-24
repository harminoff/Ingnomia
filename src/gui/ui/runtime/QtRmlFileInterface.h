#pragma once

#include <RmlUi/Core/FileInterface.h>

#include <QString>

namespace ingnomia::ui
{
class QtRmlFileInterface final : public Rml::FileInterface
{
public:
    explicit QtRmlFileInterface( QString assetRoot, QString fallbackAssetRoot = {} );

    bool valid() const noexcept;
    const QString& assetRoot() const noexcept;

    Rml::FileHandle Open( const Rml::String& path ) override;
    void Close( Rml::FileHandle file ) override;
    size_t Read( void* buffer, size_t size, Rml::FileHandle file ) override;
    bool Seek( Rml::FileHandle file, long offset, int origin ) override;
    size_t Tell( Rml::FileHandle file ) override;
    size_t Length( Rml::FileHandle file ) override;

private:
    QString resolve( const Rml::String& logicalPath ) const;

    QString m_assetRoot;
    QString m_fallbackAssetRoot;
};
} // namespace ingnomia::ui
