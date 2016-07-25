#pragma once

#include "drape/drape_global.hpp"
#include "drape/binding_info.hpp"
#include "drape/index_buffer_mutator.hpp"
#include "drape/index_storage.hpp"
#include "drape/attribute_buffer_mutator.hpp"

#include "indexer/feature_decl.hpp"

#include "geometry/screenbase.hpp"
#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/buffer_vector.hpp"

namespace dp
{

//#define DEBUG_OVERLAYS_OUTPUT

enum OverlayRank
{
  OverlayRank0 = 0,
  OverlayRank1,
  OverlayRank2,

  OverlayRanksCount
};

uint64_t constexpr kPriorityMaskZoomLevel = 0xFF0000000000FFFF;
uint64_t constexpr kPriorityMaskManual    = 0x00FFFFFFFF00FFFF;
uint64_t constexpr kPriorityMaskRank      = 0x0000000000FFFFFF;
uint64_t constexpr kPriorityMaskAll = kPriorityMaskZoomLevel |
                                      kPriorityMaskManual |
                                      kPriorityMaskRank;
class OverlayHandle
{
public:
  typedef vector<m2::RectF> Rects;

  OverlayHandle(FeatureID const & id, dp::Anchor anchor,
                uint64_t priority, bool isBillboard);

  virtual ~OverlayHandle() {}

  bool IsVisible() const;
  void SetIsVisible(bool isVisible);

  bool IsBillboard() const;

  virtual m2::PointD GetPivot(ScreenBase const & screen, bool perspective) const;

  virtual bool Update(ScreenBase const & /*screen*/) { return true; }

  virtual m2::RectD GetPixelRect(ScreenBase const & screen, bool perspective) const = 0;
  virtual void GetPixelShape(ScreenBase const & screen, bool perspective, Rects & rects) const = 0;

  double GetPivotZ() const { return m_pivotZ; }
  void SetPivotZ(double pivotZ) { m_pivotZ = pivotZ; }

  double GetExtendingSize() const { return m_extendingSize; }
  void SetExtendingSize(double extendingSize) { m_extendingSize = extendingSize; }
  m2::RectD GetExtendedPixelRect(ScreenBase const & screen) const;
  Rects const & GetExtendedPixelShape(ScreenBase const & screen) const;

  bool IsIntersect(ScreenBase const & screen, ref_ptr<OverlayHandle> const h) const;

  virtual bool IndexesRequired() const { return true; }
  void * IndexStorage(uint32_t size);
  void GetElementIndexes(ref_ptr<IndexBufferMutator> mutator) const;
  virtual void GetAttributeMutation(ref_ptr<AttributeBufferMutator> mutator) const;

  bool HasDynamicAttributes() const;
  void AddDynamicAttribute(BindingInfo const & binding, uint32_t offset, uint32_t count);

  FeatureID const & GetFeatureID() const;
  uint64_t const & GetPriority() const;

  virtual uint64_t GetPriorityMask() const { return kPriorityMaskAll; }
  virtual uint64_t GetPriorityInFollowingMode() const;

  virtual bool IsBound() const { return false; }

  virtual bool Enable3dExtention() const { return true; }

  int GetOverlayRank() const { return m_overlayRank; }
  void SetOverlayRank(int overlayRank) { m_overlayRank = overlayRank; }

  void SetCachingEnable(bool enable);

  int GetDisplacementMode() const { return m_displacementMode; }
  void SetDisplacementMode(int mode) { m_displacementMode = mode; }

#ifdef DEBUG_OVERLAYS_OUTPUT
  virtual string GetOverlayDebugInfo() { return ""; }
#endif

protected:
  FeatureID const m_id;
  dp::Anchor const m_anchor;
  uint64_t const m_priority;

  int m_overlayRank;
  double m_extendingSize;
  double m_pivotZ;

  typedef pair<BindingInfo, MutateRegion> TOffsetNode;
  TOffsetNode const & GetOffsetNode(uint8_t bufferID) const;

  m2::RectD GetPerspectiveRect(m2::RectD const & pixelRect, ScreenBase const & screen) const;
  m2::RectD GetPixelRectPerspective(ScreenBase const & screen) const;

private:
  bool const m_isBillboard;
  bool m_isVisible;
  int m_displacementMode;

  dp::IndexStorage m_indexes;
  struct LessOffsetNode
  {
    bool operator()(TOffsetNode const & node1, TOffsetNode const & node2) const
    {
      return node1.first.GetID() < node2.first.GetID();
    }
  };

  struct OffsetNodeFinder;

  set<TOffsetNode, LessOffsetNode> m_offsets;

  bool m_enableCaching;
  mutable Rects m_extendedShapeCache;
  mutable bool m_extendedShapeDirty;
  mutable m2::RectD m_extendedRectCache;
  mutable bool m_extendedRectDirty;
};

class SquareHandle : public OverlayHandle
{
  using TBase = OverlayHandle;

public:
  SquareHandle(FeatureID const & id, dp::Anchor anchor, m2::PointD const & gbPivot,
               m2::PointD const & pxSize, uint64_t priority, bool isBound, string const & debugStr,
               bool isBillboard = false);

  m2::RectD GetPixelRect(ScreenBase const & screen, bool perspective) const override;
  void GetPixelShape(ScreenBase const & screen, bool perspective, Rects & rects) const override;
  bool IsBound() const override;

#ifdef DEBUG_OVERLAYS_OUTPUT
  virtual string GetOverlayDebugInfo() override;
#endif

private:
  m2::PointD m_gbPivot;
  m2::PointD m_pxHalfSize;
  bool m_isBound;

#ifdef DEBUG_OVERLAYS_OUTPUT
  string m_debugStr;
#endif
};

uint64_t CalculateOverlayPriority(int minZoomLevel, uint8_t rank, float depth);
uint64_t CalculateSpecialModePriority(int specialPriority);

} // namespace dp
