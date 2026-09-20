#pragma once

#include "RmlUi_Renderer_GL3.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace ingnomia::ui
{
/// Thin state-boundary adapter around RmlUi 6.2's official GL3 renderer.
/// Construction, destruction, and every method require the Qt OpenGL context
/// that owns the renderer to be current on the calling thread.
class IngnomiaRmlUiRenderer final
{
public:
    IngnomiaRmlUiRenderer();
    ~IngnomiaRmlUiRenderer();

    IngnomiaRmlUiRenderer( const IngnomiaRmlUiRenderer& ) = delete;
    IngnomiaRmlUiRenderer& operator=( const IngnomiaRmlUiRenderer& ) = delete;

    explicit operator bool() const noexcept;
    Rml::RenderInterface* interface() noexcept;
    void setCameraPreviewTexture( unsigned int texture, int width, int height );
    void setCameraPreviewTexture( const std::string& source, unsigned int texture, int width, int height );

    void setViewport( int width, int height );
    bool beginFrame();
    bool endFrame();

    /// Non-empty when EndFrame repaired state the official backend does not
    /// promise to restore. Intended for deterministic integration evidence.
    const std::string& lastRestorationReport() const noexcept;

private:
    struct BoundaryState
    {
        int drawFramebuffer = 0;
        int readFramebuffer = 0;
        int program = 0;
        int vertexArray = 0;
        int arrayBuffer = 0;
        int elementArrayBuffer = 0;
        int activeTexture = 0;
        int texture0 = 0;
        int texture1 = 0;
    };

    class CameraAwareRenderInterface;
    std::unique_ptr<CameraAwareRenderInterface> m_renderer;
    BoundaryState m_before;
    bool m_frameActive = false;
    std::string m_lastRestorationReport;
};
} // namespace ingnomia::ui
