#include "drape_frontend/route_renderer.hpp"

#include "drape/glsl_func.hpp"
#include "drape/utils/projection.hpp"

#include "indexer/scales.hpp"

#include "base/logging.hpp"

namespace df
{

namespace
{

float const halfWidthInPixel[] =
{
  // 1   2     3     4     5     6     7     8     9     10
  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 2.0f, 2.0f,
  //11   12    13    14    15     16    17      18     19
  2.0f, 2.5f, 3.5f, 5.0f, 7.5f, 10.0f, 14.0f, 18.0f, 36.0f,
};

}

RouteGraphics::RouteGraphics(dp::GLState const & state,
                             drape_ptr<dp::VertexArrayBuffer> && buffer,
                             dp::Color const & color)
  : m_state(state)
  , m_buffer(move(buffer))
  , m_color(color)
{}

RouteRenderer::RouteRenderer()
  : m_distanceFromBegin(0.0)
{}

void RouteRenderer::Render(ScreenBase const & screen, ref_ptr<dp::GpuProgramManager> mng,
                           dp::UniformValuesStorage const & commonUniforms)
{
  // half width calculation
  float halfWidth = 0.0;
  double const zoomLevel = my::clamp(fabs(log(screen.GetScale()) / log(2.0)), 1.0, scales::UPPER_STYLE_SCALE);
  double const truncedZoom = trunc(zoomLevel);
  int const index = truncedZoom - 1.0;
  float const lerpCoef = zoomLevel - truncedZoom;

  if (index < scales::UPPER_STYLE_SCALE - 1)
    halfWidth = halfWidthInPixel[index] + lerpCoef * (halfWidthInPixel[index + 1] - halfWidthInPixel[index]);
  else
    halfWidth = halfWidthInPixel[index];

  for (RouteGraphics & graphics : m_routeGraphics)
  {
    dp::UniformValuesStorage uniformStorage;
    glsl::vec4 color = glsl::ToVec4(graphics.m_color);
    uniformStorage.SetFloatValue("u_color", color.r, color.g, color.b, color.a);
    uniformStorage.SetFloatValue("u_halfWidth", halfWidth, halfWidth * screen.GetScale());
    uniformStorage.SetFloatValue("u_clipLength", m_distanceFromBegin);

    ref_ptr<dp::GpuProgram> prg = mng->GetProgram(graphics.m_state.GetProgramIndex());
    prg->Bind();
    dp::ApplyState(graphics.m_state, prg);
    dp::ApplyUniforms(commonUniforms, prg);
    dp::ApplyUniforms(uniformStorage, prg);

    graphics.m_buffer->Render();
  }
}

void RouteRenderer::AddRouteRenderBucket(dp::GLState const & state, drape_ptr<dp::RenderBucket> && bucket,
                                         dp::Color const & color, ref_ptr<dp::GpuProgramManager> mng)
{
  m_routeGraphics.push_back(RouteGraphics());
  RouteGraphics & route = m_routeGraphics.back();

  route.m_state = state;
  route.m_color = color;
  route.m_buffer = bucket->MoveBuffer();
  route.m_buffer->Build(mng->GetProgram(route.m_state.GetProgramIndex()));
}

void RouteRenderer::Clear()
{
  m_routeGraphics.clear();
}

void RouteRenderer::UpdateDistanceFromBegin(double distanceFromBegin)
{
   m_distanceFromBegin = distanceFromBegin;
}

} // namespace df
