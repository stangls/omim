#include "drape/debug_rect_renderer.hpp"

#include "drape/gpu_program_manager.hpp"
#include "drape/glfunctions.hpp"
#include "drape/shader_def.hpp"

namespace dp
{

namespace
{

m2::PointF PixelPointToScreenSpace(ScreenBase const & screen, m2::PointF const & pt)
{
  float const szX = static_cast<float>(screen.PixelRect().SizeX());
  float const szY = static_cast<float>(screen.PixelRect().SizeY());
  return m2::PointF(pt.x / szX - 0.5f, -pt.y / szY + 0.5f) * 2.0f;
}

}

DebugRectRenderer & DebugRectRenderer::Instance()
{
  static DebugRectRenderer renderer;
  return renderer;
}

DebugRectRenderer::DebugRectRenderer()
  : m_VAO(0)
  , m_vertexBuffer(0)
  , m_isEnabled(true)
{
}

DebugRectRenderer::~DebugRectRenderer()
{
  ASSERT_EQUAL(m_VAO, 0, ());
  ASSERT_EQUAL(m_vertexBuffer, 0, ());
}

void DebugRectRenderer::Init(ref_ptr<dp::GpuProgramManager> mng)
{
  m_vertexBuffer = GLFunctions::glGenBuffer();
  GLFunctions::glBindBuffer(m_vertexBuffer, gl_const::GLArrayBuffer);

  m_VAO = GLFunctions::glGenVertexArray();
  GLFunctions::glBindVertexArray(m_VAO);

  m_program = mng->GetProgram(gpu::DEBUG_RECT_PROGRAM);
  int8_t attributeLocation = m_program->GetAttributeLocation("a_position");
  ASSERT_NOT_EQUAL(attributeLocation, -1, ());
  GLFunctions::glEnableVertexAttribute(attributeLocation);
  GLFunctions::glVertexAttributePointer(attributeLocation, 2, gl_const::GLFloatType, false, sizeof(float) * 2, 0);

  GLFunctions::glBindBuffer(0, gl_const::GLArrayBuffer);
  GLFunctions::glBindVertexArray(0);
}

void DebugRectRenderer::Destroy()
{
  if (m_vertexBuffer != 0)
  {
    GLFunctions::glDeleteBuffer(m_vertexBuffer);
    m_vertexBuffer = 0;
  }

  if (m_VAO != 0)
  {
    GLFunctions::glDeleteVertexArray(m_VAO);
    m_VAO = 0;
  }

  m_program = nullptr;
}

bool DebugRectRenderer::IsEnabled() const
{
  return m_isEnabled;
}

void DebugRectRenderer::SetEnabled(bool enabled)
{
  m_isEnabled = enabled;
}

void DebugRectRenderer::DrawRect(ScreenBase const & screen, m2::RectF const & rect) const
{
  if (!m_isEnabled)
    return;

  ASSERT(m_program != nullptr, ());
  m_program->Bind();

  GLFunctions::glBindBuffer(m_vertexBuffer, gl_const::GLArrayBuffer);
  GLFunctions::glBindVertexArray(m_VAO);

  GLFunctions::glDisable(gl_const::GLDepthTest);

  array<m2::PointF, 5> vertices;
  vertices[0] = PixelPointToScreenSpace(screen, rect.LeftBottom());
  vertices[1] = PixelPointToScreenSpace(screen, rect.LeftTop());
  vertices[2] = PixelPointToScreenSpace(screen, rect.RightTop());
  vertices[3] = PixelPointToScreenSpace(screen, rect.RightBottom());
  vertices[4] = vertices[0];

  GLFunctions::glBufferData(gl_const::GLArrayBuffer, vertices.size() * sizeof(vertices[0]),
                            vertices.data(), gl_const::GLStaticDraw);

  GLFunctions::glDrawArrays(gl_const::GLLineStrip, 0, vertices.size());

  GLFunctions::glEnable(gl_const::GLDepthTest);

  GLFunctions::glBindBuffer(0, gl_const::GLArrayBuffer);
  GLFunctions::glBindVertexArray(0);
}

} // namespace dp