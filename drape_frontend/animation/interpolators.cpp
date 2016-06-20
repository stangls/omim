#include "interpolators.hpp"

#include "base/assert.hpp"

#include "interpolations.hpp"

namespace df
{

double CalcAnimSpeedDuration(double pxDiff, double pxSpeed)
{
  double constexpr kEps = 1e-5;

  if (my::AlmostEqualAbs(pxDiff, 0.0, kEps))
    return 0.0;

  return fabs(pxDiff) / pxSpeed;
}

Interpolator::Interpolator(double duration, double delay)
  : m_elapsedTime(0.0)
  , m_duration(duration)
  , m_delay(delay)
  , m_isActive(false)
{
  ASSERT_GREATER_OR_EQUAL(m_duration, 0.0, ());
}

bool Interpolator::IsFinished() const
{
  if (!IsActive())
    return true;

  return m_elapsedTime > (m_duration + m_delay);
}

void Interpolator::Advance(double elapsedSeconds)
{
  m_elapsedTime += elapsedSeconds;
}

void Interpolator::Finish()
{
  m_elapsedTime = m_duration + m_delay + 1.0;
}

bool Interpolator::IsActive() const
{
  return m_isActive;
}

void Interpolator::SetActive(bool active)
{
  m_isActive = active;
}

void Interpolator::SetMaxDuration(double maxDuration)
{
  m_duration = min(m_duration, maxDuration);
}

void Interpolator::SetMinDuration(double minDuration)
{
  m_duration = max(m_duration, minDuration);
}

double Interpolator::GetT() const
{
  if (IsFinished())
    return 1.0;

  return max(m_elapsedTime - m_delay, 0.0) / m_duration;
}

double Interpolator::GetElapsedTime() const
{
  return m_elapsedTime;
}

double Interpolator::GetDuration() const
{
  return m_duration;
}

PositionInterpolator::PositionInterpolator()
  : PositionInterpolator(0.0 /* duration */, 0.0 /* delay */, m2::PointD(), m2::PointD())
{}

PositionInterpolator::PositionInterpolator(double duration, double delay,
                                           m2::PointD const & startPosition,
                                           m2::PointD const & endPosition)
  : Interpolator(duration, delay)
  , m_startPosition(startPosition)
  , m_endPosition(endPosition)
  , m_position(startPosition)
{
  SetActive(m_startPosition != m_endPosition);
}

PositionInterpolator::PositionInterpolator(m2::PointD const & startPosition,
                                           m2::PointD const & endPosition,
                                           ScreenBase const & convertor)
  : PositionInterpolator(0.0 /* delay */, startPosition, endPosition, convertor)
{}

PositionInterpolator::PositionInterpolator(double delay, m2::PointD const & startPosition,
                                           m2::PointD const & endPosition,
                                           ScreenBase const & convertor)
  : Interpolator(PositionInterpolator::GetMoveDuration(startPosition, endPosition, convertor), delay)
  , m_startPosition(startPosition)
  , m_endPosition(endPosition)
  , m_position(startPosition)
{
  SetActive(m_startPosition != m_endPosition);
}

PositionInterpolator::PositionInterpolator(m2::PointD const & startPxPosition,
                                           m2::PointD const & endPxPosition,
                                           m2::RectD const & pixelRect)
  : PositionInterpolator(0.0 /* delay */, startPxPosition, endPxPosition, pixelRect)
{}

PositionInterpolator::PositionInterpolator(double delay, m2::PointD const & startPxPosition,
                                           m2::PointD const & endPxPosition, m2::RectD const & pixelRect)
  : Interpolator(PositionInterpolator::GetPixelMoveDuration(startPxPosition, endPxPosition, pixelRect), delay)
  , m_startPosition(startPxPosition)
  , m_endPosition(endPxPosition)
  , m_position(startPxPosition)
{
  SetActive(m_startPosition != m_endPosition);
}

//static
double PositionInterpolator::GetMoveDuration(m2::PointD const & startPosition,
                                             m2::PointD const & endPosition,
                                             ScreenBase const & convertor)
{
  return GetPixelMoveDuration(convertor.GtoP(startPosition),
                              convertor.GtoP(endPosition),
                              convertor.PixelRect());
}

double PositionInterpolator::GetPixelMoveDuration(m2::PointD const & startPosition,
                                                  m2::PointD const & endPosition,
                                                  m2::RectD const & pixelRect)
{
  double const kMinMoveDuration = 0.2;
  double const kMinSpeedScalar = 0.2;
  double const kMaxSpeedScalar = 7.0;
  double const kEps = 1e-5;

  double const pixelLength = endPosition.Length(startPosition);
  if (pixelLength < kEps)
    return 0.0;

  double const minSize = min(pixelRect.SizeX(), pixelRect.SizeY());
  if (pixelLength < kMinSpeedScalar * minSize)
    return kMinMoveDuration;

  double const pixelSpeed = kMaxSpeedScalar * minSize;
  return CalcAnimSpeedDuration(pixelLength, pixelSpeed);
}

void PositionInterpolator::Advance(double elapsedSeconds)
{
  TBase::Advance(elapsedSeconds);
  m_position = InterpolatePoint(m_startPosition, m_endPosition, GetT());
}

void PositionInterpolator::Finish()
{
  TBase::Finish();
  m_position = m_endPosition;
}

ScaleInterpolator::ScaleInterpolator()
  : ScaleInterpolator(1.0 /* startScale */, 1.0 /* endScale */)
{}

ScaleInterpolator::ScaleInterpolator(double startScale, double endScale)
  : ScaleInterpolator(0.0 /* delay */, startScale, endScale)
{}

ScaleInterpolator::ScaleInterpolator(double delay, double startScale, double endScale)
  : Interpolator(ScaleInterpolator::GetScaleDuration(startScale, endScale), delay)
  , m_startScale(startScale)
  , m_endScale(endScale)
  , m_scale(startScale)
{
  SetActive(m_startScale != m_endScale);
}

// static
double ScaleInterpolator::GetScaleDuration(double startScale, double endScale)
{
  // Resize 2.0 times should be done for 0.2 seconds.
  double constexpr kPixelSpeed = 2.0 / 0.2;

  if (startScale > endScale)
    swap(startScale, endScale);

  return CalcAnimSpeedDuration(endScale / startScale, kPixelSpeed);
}

void ScaleInterpolator::Advance(double elapsedSeconds)
{
  TBase::Advance(elapsedSeconds);
  m_scale = InterpolateDouble(m_startScale, m_endScale, GetT());
}

void ScaleInterpolator::Finish()
{
  TBase::Finish();
  m_scale = m_endScale;
}

AngleInterpolator::AngleInterpolator()
  : AngleInterpolator(0.0 /* startAngle */, 0.0 /* endAngle */)
{}

AngleInterpolator::AngleInterpolator(double startAngle, double endAngle)
  : AngleInterpolator(0.0 /* delay */, startAngle, endAngle)
{}

AngleInterpolator::AngleInterpolator(double delay, double startAngle, double endAngle)
  : Interpolator(AngleInterpolator::GetRotateDuration(startAngle, endAngle), delay)
  , m_startAngle(ang::AngleIn2PI(startAngle))
  , m_endAngle(ang::AngleIn2PI(endAngle))
  , m_angle(m_startAngle)
{
  SetActive(m_startAngle != m_endAngle);
}

AngleInterpolator::AngleInterpolator(double delay, double duration, double startAngle, double endAngle)
  : Interpolator(duration, delay)
  , m_startAngle(ang::AngleIn2PI(startAngle))
  , m_endAngle(ang::AngleIn2PI(endAngle))
  , m_angle(m_startAngle)
{
  SetActive(m_startAngle != m_endAngle);
}

// static
double AngleInterpolator::GetRotateDuration(double startAngle, double endAngle)
{
  double const kRotateDurationScalar = 0.75;
  startAngle = ang::AngleIn2PI(startAngle);
  endAngle = ang::AngleIn2PI(endAngle);
  return kRotateDurationScalar * fabs(ang::GetShortestDistance(startAngle, endAngle)) / math::pi;
}

void AngleInterpolator::Advance(double elapsedSeconds)
{
  TBase::Advance(elapsedSeconds);
  m_angle = m_startAngle + ang::GetShortestDistance(m_startAngle, m_endAngle) * GetT();
}

void AngleInterpolator::Finish()
{
  TBase::Finish();
  m_angle = m_endAngle;
}

} // namespace df
