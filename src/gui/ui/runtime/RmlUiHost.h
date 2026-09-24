#pragma once

#include "RmlUiQtInputAdapter.h"

#include <QSize>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class QWindow;
class QFileSystemWatcher;
class QTimer;
namespace Rml { class Context; class ElementDocument; }

namespace ingnomia::ui
{
namespace shell { class ShellController; class ShellRmlBinding; }
namespace hud { class HudRmlBinding; }
namespace inspector { class InspectorRmlBinding; }
namespace management6b { class Management6BRmlBinding; }
#if defined(INGNOMIA_DEVELOPER_UI)
namespace debug { class DebugRmlBinding; }
#endif
class IngnomiaRmlUiRenderer;
class QtRmlFileInterface;
class QtRmlSystemInterface;
class RmlUiHost;

class RmlUiDetachedContext final
{
public:
    ~RmlUiDetachedContext();
    RmlUiDetachedContext( const RmlUiDetachedContext& ) = delete;
    RmlUiDetachedContext& operator=( const RmlUiDetachedContext& ) = delete;

    Rml::Context* context() const noexcept { return m_context; }
    RmlUiQtInputAdapter& input() noexcept { return m_input; }
    QWindow* window() const noexcept { return m_window.data(); }
    const QString& name() const noexcept { return m_contextName; }

private:
    friend class RmlUiHost;
    RmlUiDetachedContext( QWindow* window, QString contextName, Rml::Context* context );

    QPointer<QWindow> m_window;
    QString m_contextName;
    Rml::Context* m_context = nullptr;
    RmlUiQtInputAdapter m_input;
};

class RmlUiHost final
{
public:
    struct Config
    {
        QWindow* window = nullptr;
        QString assetRoot;
        QString fallbackAssetRoot;
        QString contextName = "ingnomia-primary-ui";
        QSize physicalSize = {1, 1};
        float densityIndependentPixelRatio = 1.0f;
        QStringList fontFiles;
        bool enableDebugger = false;
        bool enableHotReload = false;
    };

    RmlUiHost();
    ~RmlUiHost();

    RmlUiHost( const RmlUiHost& ) = delete;
    RmlUiHost& operator=( const RmlUiHost& ) = delete;

    bool initialize( const Config& config );
    bool shutdown();
    bool initialized() const noexcept;

    bool resize( QSize physicalSize, float densityIndependentPixelRatio );
    void setCameraPreviewTexture( unsigned int texture, int width, int height );
    void setCameraPreviewTexture( const std::string& source, unsigned int texture, int width, int height );
    bool update();
    bool render();
    double nextUpdateDelay() const;

    std::unique_ptr<RmlUiDetachedContext> createDetachedContext( QWindow* window,
        const QString& contextName, QSize physicalSize, float densityIndependentPixelRatio );
    bool destroyDetachedContext( std::unique_ptr<RmlUiDetachedContext>& detached );
    bool resizeDetached( RmlUiDetachedContext&, QSize physicalSize, float densityIndependentPixelRatio );
    bool updateDetached( RmlUiDetachedContext& );
    bool renderDetached( RmlUiDetachedContext& );
    Rml::ElementDocument* loadDocument( RmlUiDetachedContext&, const QString& logicalPath, bool show = true );
    void setSystemWindow( QWindow* window );

    Rml::ElementDocument* loadDocument( const QString& logicalPath, bool show = true );
    bool unloadDocument( Rml::ElementDocument* document );
    Rml::Context* context() const noexcept;
    RmlUiQtInputAdapter& input() noexcept;
	using DocumentReloadHandler = std::function<bool()>;
	void setDocumentReloadHandler( DocumentReloadHandler handler ) { m_documentReloadHandler = std::move( handler ); }
	void requestDocumentReload();
	bool processHotReload();
	bool hotReloadEnabled() const noexcept { return static_cast<bool>( m_hotReloadWatcher ); }
	bool toggleDebugger();
    shell::ShellRmlBinding* createShellBinding();
    hud::HudRmlBinding* createHudBinding();
    inspector::InspectorRmlBinding* createInspectorBinding();
    inspector::InspectorRmlBinding* createCreatureInspectorBinding( int cameraSlot, int windowIndex );
    management6b::Management6BRmlBinding* createManagement6BBinding();
#if defined(INGNOMIA_DEVELOPER_UI)
    debug::DebugRmlBinding* createDebugBinding();
#endif

private:
    bool requireGuiThread( const char* operation ) const;
    bool requireCurrentContext( const char* operation ) const;
	void startHotReload( const QString& assetRoot );
	void scanHotReloadFiles( bool detectChanges );
	void queueHotReloadPath( const QString& path );

    QString m_contextName;
    QPointer<QWindow> m_ownerWindow;
    std::unique_ptr<QtRmlSystemInterface> m_system;
    std::unique_ptr<QtRmlFileInterface> m_files;
    std::unique_ptr<IngnomiaRmlUiRenderer> m_renderer;
    Rml::Context* m_context = nullptr;
    RmlUiQtInputAdapter m_input;
    std::unique_ptr<shell::ShellRmlBinding> m_shellBinding;
    std::unique_ptr<hud::HudRmlBinding> m_hudBinding;
    std::unique_ptr<inspector::InspectorRmlBinding> m_inspectorBinding;
    std::vector<std::unique_ptr<inspector::InspectorRmlBinding>> m_creatureInspectorBindings;
    std::vector<RmlUiDetachedContext*> m_detachedContexts;
    std::unique_ptr<management6b::Management6BRmlBinding> m_management6bBinding;
#if defined(INGNOMIA_DEVELOPER_UI)
    std::unique_ptr<debug::DebugRmlBinding> m_debugBinding;
#endif
    bool m_coreInitialized = false;
	std::unique_ptr<QFileSystemWatcher> m_hotReloadWatcher;
	std::unique_ptr<QTimer> m_hotReloadTimer;
	DocumentReloadHandler m_documentReloadHandler;
	QString m_hotReloadRoot;
	QStringList m_hotReloadFiles;
	QStringList m_hotReloadSignatures;
	bool m_reloadStylesPending{};
	bool m_reloadDocumentsPending{};
	bool m_reloadTexturesPending{};
	bool m_hotReloadReady{};
};
} // namespace ingnomia::ui
