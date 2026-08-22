#include "IngnomiaRmlUiRenderer.h"

#include <glad/gl.h>

#include <QDebug>
#include <QOpenGLContext>

#include <sstream>

namespace ingnomia::ui
{
class IngnomiaRmlUiRenderer::CameraAwareRenderInterface final : public RenderInterface_GL3
{
public:
    void setCameraPreviewTexture( unsigned int texture, int width, int height )
    {
        setCameraPreviewTexture( "camera-preview://selected", texture, width, height );
    }

    void setCameraPreviewTexture( const std::string& source, unsigned int texture, int width, int height )
    {
        cameraTextures_[source] = { texture, width, height };
    }

    Rml::TextureHandle LoadTexture( Rml::Vector2i& dimensions, const Rml::String& source ) override
    {
        // RmlUi preserves sources beginning with '?' instead of joining them
        // to the document asset path. Keep the unprefixed form accepted too
        // for callers that bypass the texture database.
        std::string key( source.c_str(), source.size() );
        if ( !key.empty() && key.front() == '?' ) key.erase( key.begin() );
        if ( key.rfind( "camera-preview://", 0 ) == 0 )
        {
            const auto it = cameraTextures_.find( key );
            if ( it != cameraTextures_.end() )
            {
                dimensions = { it->second.width, it->second.height };
                return static_cast<Rml::TextureHandle>( it->second.texture );
            }
        }
        return RenderInterface_GL3::LoadTexture( dimensions, source );
    }

    void ReleaseTexture( Rml::TextureHandle texture ) override
    {
        for ( const auto& [source, camera] : cameraTextures_ )
            if ( texture == static_cast<Rml::TextureHandle>( camera.texture ) ) return;
        RenderInterface_GL3::ReleaseTexture( texture );
    }

private:
    struct CameraTexture { unsigned int texture{}; int width{1}; int height{1}; };
    std::unordered_map<std::string, CameraTexture> cameraTextures_;
};

namespace
{
bool requireCurrentContext( const char* operation )
{
    if ( QOpenGLContext::currentContext() ) return true;
    qCritical() << "RmlUi GL3 operation requires a current Qt OpenGL context:" << operation;
    return false;
}
} // namespace

IngnomiaRmlUiRenderer::IngnomiaRmlUiRenderer() : m_renderer( std::make_unique<CameraAwareRenderInterface>() )
{
    if ( !QOpenGLContext::currentContext() )
        qCritical() << "Constructed RmlUi GL3 renderer without a current Qt OpenGL context";
}

IngnomiaRmlUiRenderer::~IngnomiaRmlUiRenderer()
{
    if ( !QOpenGLContext::currentContext() )
        qCritical() << "Destroying RmlUi GL3 renderer without a current Qt OpenGL context";
}

IngnomiaRmlUiRenderer::operator bool() const noexcept
{
    return m_renderer && static_cast<bool>( *m_renderer );
}

Rml::RenderInterface* IngnomiaRmlUiRenderer::interface() noexcept
{
    return m_renderer.get();
}

void IngnomiaRmlUiRenderer::setCameraPreviewTexture( unsigned int texture, int width, int height )
{
    if ( m_renderer ) m_renderer->setCameraPreviewTexture( texture, width, height );
}

void IngnomiaRmlUiRenderer::setCameraPreviewTexture( const std::string& source, unsigned int texture, int width, int height )
{
    if ( m_renderer ) m_renderer->setCameraPreviewTexture( source, texture, width, height );
}

void IngnomiaRmlUiRenderer::setViewport( int width, int height )
{
    if ( !requireCurrentContext( "setViewport" ) ) return;
    m_renderer->SetViewport( width, height );
}

bool IngnomiaRmlUiRenderer::beginFrame()
{
    if ( m_frameActive )
    {
        qCritical() << "RmlUi GL3 BeginFrame called while another UI frame is active";
        return false;
    }
    if ( !requireCurrentContext( "beginFrame" ) ) return false;

    glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &m_before.drawFramebuffer );
    glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &m_before.readFramebuffer );
    glGetIntegerv( GL_CURRENT_PROGRAM, &m_before.program );
    glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &m_before.vertexArray );
    m_lastRestorationReport.clear();
    m_frameActive = true;
    m_renderer->BeginFrame();
    return true;
}

bool IngnomiaRmlUiRenderer::endFrame()
{
    if ( !m_frameActive )
    {
        qCritical() << "RmlUi GL3 EndFrame called without a matching BeginFrame";
        return false;
    }
    if ( !requireCurrentContext( "endFrame" ) )
    {
        m_frameActive = false;
        return false;
    }

    m_renderer->EndFrame();

    BoundaryState after;
    glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &after.drawFramebuffer );
    glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &after.readFramebuffer );
    glGetIntegerv( GL_CURRENT_PROGRAM, &after.program );
    glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &after.vertexArray );

    std::ostringstream report;
    const auto record = [&report]( const char* name, int expected, int actual ) {
        if ( expected == actual ) return;
        if ( report.tellp() > 0 ) report << ", ";
        report << name << ' ' << actual << " -> " << expected;
    };
    record( "draw-framebuffer", m_before.drawFramebuffer, after.drawFramebuffer );
    record( "read-framebuffer", m_before.readFramebuffer, after.readFramebuffer );
    record( "program", m_before.program, after.program );
    record( "vertex-array", m_before.vertexArray, after.vertexArray );
    m_lastRestorationReport = report.str();

    // The upstream backend documents restoration for its raster/blend/stencil
    // subset. Program, VAO, and framebuffer bindings are owned by this host
    // boundary and must be restored explicitly before control returns to the game.
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, static_cast<GLuint>( m_before.drawFramebuffer ) );
    glBindFramebuffer( GL_READ_FRAMEBUFFER, static_cast<GLuint>( m_before.readFramebuffer ) );
    glUseProgram( static_cast<GLuint>( m_before.program ) );
    glBindVertexArray( static_cast<GLuint>( m_before.vertexArray ) );

    m_frameActive = false;
    return true;
}

const std::string& IngnomiaRmlUiRenderer::lastRestorationReport() const noexcept
{
    return m_lastRestorationReport;
}
} // namespace ingnomia::ui
