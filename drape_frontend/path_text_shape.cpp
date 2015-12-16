#include "drape_frontend/path_text_shape.hpp"
#include "drape_frontend/text_handle.hpp"
#include "drape_frontend/text_layout.hpp"
#include "drape_frontend/visual_params.hpp"
#include "drape_frontend/intrusive_vector.hpp"

#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"
#include "drape/glstate.hpp"
#include "drape/shader_def.hpp"
#include "drape/overlay_handle.hpp"

#include "base/math.hpp"
#include "base/logging.hpp"
#include "base/stl_add.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"
#include "base/matrix.hpp"

#include "geometry/transformations.hpp"

#include "std/algorithm.hpp"
#include "std/vector.hpp"

using m2::Spline;

namespace
{

class PathTextHandle : public df::TextHandle
{
public:
  PathTextHandle(m2::SharedSpline const & spl,
                 df::SharedTextLayout const & layout,
                 float const mercatorOffset, uint64_t priority,
                 ref_ptr<dp::TextureManager> textureManager)
    : TextHandle(FeatureID(), layout->GetText(), dp::Center, priority, textureManager)
    , m_spline(spl)
    , m_layout(layout)
  {
    m_centerPointIter = m_spline.CreateIterator();
    m_centerPointIter.Advance(mercatorOffset);
  }

  bool Update(ScreenBase const & screen) override
  {
    if (!df::TextHandle::Update(screen))
      return false;

    if (m_normals.empty())
      m_normals.resize(4 * m_layout->GetGlyphCount());

    return m_layout->CacheDynamicGeometry(m_centerPointIter, screen, m_normals);
  }

  m2::RectD GetPixelRect(ScreenBase const & screen) const override
  {
    m2::PointD const pixelPivot(screen.GtoP(m_centerPointIter.m_pos));
    m2::RectD result;
    for (gpu::TextDynamicVertex const & v : m_normals)
      result.Add(pixelPivot + glsl::ToPoint(v.m_normal));

    return result;
  }

  void GetPixelShape(ScreenBase const & screen, Rects & rects) const override
  {
    m2::PointD const pixelPivot(screen.GtoP(m_centerPointIter.m_pos));
    for (size_t quadIndex = 0; quadIndex < m_normals.size(); quadIndex += 4)
    {
      m2::RectF r;
      r.Add(pixelPivot + glsl::ToPoint(m_normals[quadIndex].m_normal));
      r.Add(pixelPivot + glsl::ToPoint(m_normals[quadIndex + 1].m_normal));
      r.Add(pixelPivot + glsl::ToPoint(m_normals[quadIndex + 2].m_normal));
      r.Add(pixelPivot + glsl::ToPoint(m_normals[quadIndex + 3].m_normal));
      if (screen.PixelRect().IsIntersect(m2::RectD(r)))
        rects.push_back(r);
    }
  }

  void GetAttributeMutation(ref_ptr<dp::AttributeBufferMutator> mutator,
                            ScreenBase const & screen) const override
  {
    // for visible text paths we always update normals
    SetForceUpdateNormals(IsVisible());
    TextHandle::GetAttributeMutation(mutator, screen);
  }

  uint64_t GetPriorityMask() const override
  {
    return dp::kPriorityMaskManual | dp::kPriorityMaskRank;
  }

private:
  m2::SharedSpline m_spline;
  m2::Spline::iterator m_centerPointIter;

  df::SharedTextLayout m_layout;
};

}

namespace df
{

PathTextShape::PathTextShape(m2::SharedSpline const & spline,
                             PathTextViewParams const & params)
  : m_spline(spline)
  , m_params(params)
{}

uint64_t PathTextShape::GetOverlayPriority() const
{
  // Overlay priority for path text shapes considers length of the text.
  // Greater test length has more priority, because smaller texts have more chances to be shown along the road.
  // [6 bytes - standard overlay priority][1 byte - reserved][1 byte - length].
  static uint64_t constexpr kMask = ~static_cast<uint64_t>(0xFF);
  uint64_t priority = dp::CalculateOverlayPriority(m_params.m_minVisibleScale, m_params.m_rank, m_params.m_depth);
  priority &= kMask;
  priority |= (static_cast<uint8_t>(m_params.m_text.size()));

  return priority;
}

void PathTextShape::DrawPathTextPlain(ref_ptr<dp::TextureManager> textures,
                                      ref_ptr<dp::Batcher> batcher,
                                      unique_ptr<PathTextLayout> && layout,
                                      buffer_vector<float, 32> const & offsets) const
{
  dp::TextureManager::ColorRegion color;
  textures->GetColorRegion(m_params.m_textFont.m_color, color);

  dp::GLState state(gpu::TEXT_PROGRAM, dp::GLState::OverlayLayer);
  state.SetColorTexture(color.GetTexture());
  state.SetMaskTexture(layout->GetMaskTexture());

  ASSERT(!offsets.empty(), ());
  gpu::TTextStaticVertexBuffer staticBuffer;
  gpu::TTextDynamicVertexBuffer dynBuffer;
  SharedTextLayout layoutPtr(layout.release());
  for (float offset : offsets)
  {
    staticBuffer.clear();
    dynBuffer.clear();

    Spline::iterator iter = m_spline.CreateIterator();
    iter.Advance(offset);
    layoutPtr->CacheStaticGeometry(glsl::vec3(glsl::ToVec2(iter.m_pos), m_params.m_depth),
                                   color, staticBuffer);

    dynBuffer.resize(staticBuffer.size(), gpu::TextDynamicVertex(glsl::vec2(0.0, 0.0)));

    dp::AttributeProvider provider(2, staticBuffer.size());
    provider.InitStream(0, gpu::TextStaticVertex::GetBindingInfo(), make_ref(staticBuffer.data()));
    provider.InitStream(1, gpu::TextDynamicVertex::GetBindingInfo(), make_ref(dynBuffer.data()));

    drape_ptr<dp::OverlayHandle> handle = make_unique_dp<PathTextHandle>(m_spline, layoutPtr, offset,
                                                                         GetOverlayPriority(),
                                                                         textures);
    batcher->InsertListOfStrip(state, make_ref(&provider), move(handle), 4);
  }
}

void PathTextShape::DrawPathTextOutlined(ref_ptr<dp::TextureManager> textures,
                                         ref_ptr<dp::Batcher> batcher,
                                         unique_ptr<PathTextLayout> && layout,
                                         buffer_vector<float, 32> const & offsets) const
{
  dp::TextureManager::ColorRegion color;
  dp::TextureManager::ColorRegion outline;
  textures->GetColorRegion(m_params.m_textFont.m_color, color);
  textures->GetColorRegion(m_params.m_textFont.m_outlineColor, outline);

  dp::GLState state(gpu::TEXT_OUTLINED_PROGRAM, dp::GLState::OverlayLayer);
  state.SetColorTexture(color.GetTexture());
  state.SetMaskTexture(layout->GetMaskTexture());

  ASSERT(!offsets.empty(), ());
  gpu::TTextOutlinedStaticVertexBuffer staticBuffer;
  gpu::TTextDynamicVertexBuffer dynBuffer;
  SharedTextLayout layoutPtr(layout.release());
  for (float offset : offsets)
  {
    staticBuffer.clear();
    dynBuffer.clear();

    Spline::iterator iter = m_spline.CreateIterator();
    iter.Advance(offset);
    layoutPtr->CacheStaticGeometry(glsl::vec3(glsl::ToVec2(iter.m_pos), m_params.m_depth),
                                   color, outline, staticBuffer);

    dynBuffer.resize(staticBuffer.size(), gpu::TextDynamicVertex(glsl::vec2(0.0, 0.0)));

    dp::AttributeProvider provider(2, staticBuffer.size());
    provider.InitStream(0, gpu::TextOutlinedStaticVertex::GetBindingInfo(), make_ref(staticBuffer.data()));
    provider.InitStream(1, gpu::TextDynamicVertex::GetBindingInfo(), make_ref(dynBuffer.data()));

    drape_ptr<dp::OverlayHandle> handle = make_unique_dp<PathTextHandle>(m_spline, layoutPtr, offset,
                                                                         GetOverlayPriority(),
                                                                         textures);
    batcher->InsertListOfStrip(state, make_ref(&provider), move(handle), 4);
  }
}

void PathTextShape::Draw(ref_ptr<dp::Batcher> batcher, ref_ptr<dp::TextureManager> textures) const
{
  // here, typically rotating text (regarding map rotation on screen) is rendered, for exapmle:
  // * street-names
  // * names of running water
  unique_ptr<PathTextLayout> layout = make_unique<PathTextLayout>(strings::MakeUniString(m_params.m_text),
                                                                  m_params.m_textFont.m_size, textures);

  uint32_t glyphCount = layout->GetGlyphCount();
  if (glyphCount == 0)
    return;

  //we leave a little space on either side of the text that would
  //remove the comparison for equality of spline portions
  float const kTextBorder = 4.0f;
  float const textLength = kTextBorder + layout->GetPixelLength();
  float const pathGlbLength = m_spline->GetLength();

  // on next readable scale m_scaleGtoP will be twice
  if (textLength > pathGlbLength * 2.0 * m_params.m_baseGtoPScale)
    return;

  float const kPathLengthScalar = 0.75;
  float const pathLength = kPathLengthScalar * m_params.m_baseGtoPScale * pathGlbLength;

  float const etalonEmpty = max(300 * df::VisualParams::Instance().GetVisualScale(), (double)textLength);
  float const minPeriodSize = etalonEmpty + textLength;
  float const twoTextAndEmpty = minPeriodSize + textLength;

  buffer_vector<float, 32> offsets;
  if (pathLength < twoTextAndEmpty)
  {
    // if we can't place 2 text and empty part on path
    // we place only one text on center of path
    offsets.push_back(pathGlbLength / 2.0f);
  }
  else
  {
    double const textCount = max(floor(pathLength / minPeriodSize), 1.0);
    double const glbTextLen = pathGlbLength / textCount;
    for (double offset = 0.5 * glbTextLen; offset < pathGlbLength; offset += glbTextLen)
      offsets.push_back(offset);
  }

  if (m_params.m_textFont.m_outlineColor == dp::Color::Transparent())
    DrawPathTextPlain(textures, batcher, move(layout), offsets);
  else
    DrawPathTextOutlined(textures, batcher, move(layout), offsets);
}

}
