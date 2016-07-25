#include "drape_frontend/user_event_stream.hpp"
#include "drape_frontend/animation/linear_animation.hpp"
#include "drape_frontend/animation/scale_animation.hpp"
#include "drape_frontend/animation/follow_animation.hpp"
#include "drape_frontend/animation/sequence_animation.hpp"
#include "drape_frontend/animation_constants.hpp"
#include "drape_frontend/animation_system.hpp"
#include "drape_frontend/animation_utils.hpp"
#include "drape_frontend/screen_animations.hpp"
#include "drape_frontend/screen_operations.hpp"
#include "drape_frontend/visual_params.hpp"

#include "indexer/scales.hpp"

#include "platform/platform.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"

#ifdef DEBUG
#define TEST_CALL(action) if (m_testFn) m_testFn(action)
#else
#define TEST_CALL(action)
#endif

namespace df
{

namespace
{

uint64_t const kDoubleTapPauseMs = 250;
uint64_t const kLongTouchMs = 500;
uint64_t const kKineticDelayMs = 500;

float const kForceTapThreshold = 0.75;

size_t GetValidTouchesCount(array<Touch, 2> const & touches)
{
  size_t result = 0;
  if (touches[0].m_id != -1)
    ++result;
  if (touches[1].m_id != -1)
    ++result;

  return result;
}

} // namespace

#ifdef DEBUG
char const * UserEventStream::BEGIN_DRAG = "BeginDrag";
char const * UserEventStream::DRAG = "Drag";
char const * UserEventStream::END_DRAG = "EndDrag";
char const * UserEventStream::BEGIN_SCALE = "BeginScale";
char const * UserEventStream::SCALE = "Scale";
char const * UserEventStream::END_SCALE = "EndScale";
char const * UserEventStream::BEGIN_TAP_DETECTOR = "BeginTap";
char const * UserEventStream::LONG_TAP_DETECTED = "LongTap";
char const * UserEventStream::SHORT_TAP_DETECTED = "ShortTap";
char const * UserEventStream::CANCEL_TAP_DETECTOR = "CancelTap";
char const * UserEventStream::TRY_FILTER = "TryFilter";
char const * UserEventStream::END_FILTER = "EndFilter";
char const * UserEventStream::CANCEL_FILTER = "CancelFilter";
char const * UserEventStream::TWO_FINGERS_TAP = "TwoFingersTap";
char const * UserEventStream::BEGIN_DOUBLE_TAP_AND_HOLD = "BeginDoubleTapAndHold";
char const * UserEventStream::DOUBLE_TAP_AND_HOLD = "DoubleTapAndHold";
char const * UserEventStream::END_DOUBLE_TAP_AND_HOLD = "EndDoubleTapAndHold";
#endif

uint8_t const TouchEvent::INVALID_MASKED_POINTER = 0xFF;

void TouchEvent::PrepareTouches(array<Touch, 2> const & previousTouches)
{
  if (GetValidTouchesCount(m_touches) == 2 && GetValidTouchesCount(previousTouches) > 0)
  {
    if (previousTouches[0].m_id == m_touches[1].m_id)
      Swap();
  }
}

void TouchEvent::SetFirstMaskedPointer(uint8_t firstMask)
{
  m_pointersMask = (m_pointersMask & 0xFF00) | static_cast<uint16_t>(firstMask);
}

uint8_t TouchEvent::GetFirstMaskedPointer() const
{
  return static_cast<uint8_t>(m_pointersMask & 0xFF);
}

void TouchEvent::SetSecondMaskedPointer(uint8_t secondMask)
{
  ASSERT(secondMask == INVALID_MASKED_POINTER || GetFirstMaskedPointer() != INVALID_MASKED_POINTER, ());
  m_pointersMask = (static_cast<uint16_t>(secondMask) << 8) | (m_pointersMask & 0xFF);
}

uint8_t TouchEvent::GetSecondMaskedPointer() const
{
  return static_cast<uint8_t>((m_pointersMask & 0xFF00) >> 8);
}

size_t TouchEvent::GetMaskedCount()
{
  return static_cast<int>(GetFirstMaskedPointer() != INVALID_MASKED_POINTER) +
         static_cast<int>(GetSecondMaskedPointer() != INVALID_MASKED_POINTER);
}

void TouchEvent::Swap()
{
  auto swapIndex = [](uint8_t index) -> uint8_t
  {
    if (index == INVALID_MASKED_POINTER)
      return index;

    return index ^ 0x1;
  };

  swap(m_touches[0], m_touches[1]);
  SetFirstMaskedPointer(swapIndex(GetFirstMaskedPointer()));
  SetSecondMaskedPointer(swapIndex(GetSecondMaskedPointer()));
}

UserEventStream::UserEventStream()
  : m_state(STATE_EMPTY)
  , m_animationSystem(AnimationSystem::Instance())
  , m_startDragOrg(m2::PointD::Zero())
  , m_startDoubleTapAndHold(m2::PointD::Zero())
{
}

void UserEventStream::AddEvent(UserEvent const & event)
{
  lock_guard<mutex> guard(m_lock);
  UNUSED_VALUE(guard);
  m_events.push_back(event);
}

ScreenBase const & UserEventStream::ProcessEvents(bool & modelViewChanged, bool & viewportChanged)
{
  list<UserEvent> events;

  {
    lock_guard<mutex> guard(m_lock);
    UNUSED_VALUE(guard);
    swap(m_events, events);
  }

  m2::RectD const prevPixelRect = GetCurrentScreen().PixelRect();

  m_modelViewChanged = !events.empty() || m_state == STATE_SCALE || m_state == STATE_DRAG;
  for (UserEvent const & e : events)
  {
    bool breakAnim = false;

    switch (e.m_type)
    {
    case UserEvent::EVENT_SCALE:
      breakAnim = SetScale(e.m_scaleEvent.m_pxPoint, e.m_scaleEvent.m_factor, e.m_scaleEvent.m_isAnim);
      TouchCancel(m_touches);
      break;
    case UserEvent::EVENT_RESIZE:
      m_navigator.OnSize(e.m_resize.m_width, e.m_resize.m_height);
      breakAnim = true;
      TouchCancel(m_touches);
      if (m_state == STATE_DOUBLE_TAP_HOLD)
        EndDoubleTapAndHold(m_touches[0]);
      break;
    case UserEvent::EVENT_SET_ANY_RECT:
      breakAnim = SetRect(e.m_anyRect.m_rect, e.m_anyRect.m_isAnim);
      TouchCancel(m_touches);
      break;
    case UserEvent::EVENT_SET_RECT:
      breakAnim = SetRect(e.m_rectEvent.m_rect, e.m_rectEvent.m_zoom, e.m_rectEvent.m_applyRotation, e.m_rectEvent.m_isAnim);
      TouchCancel(m_touches);
      break;
    case UserEvent::EVENT_SET_CENTER:
      breakAnim = SetCenter(e.m_centerEvent.m_center, e.m_centerEvent.m_zoom, e.m_centerEvent.m_isAnim);
      TouchCancel(m_touches);
      break;
    case UserEvent::EVENT_TOUCH:
      breakAnim = ProcessTouch(e.m_touchEvent);
      break;
    case UserEvent::EVENT_ROTATE:
      {
        ScreenBase const & screen = m_navigator.Screen();
        if (screen.isPerspective())
        {
          m2::PointD pt = screen.PixelRectIn3d().Center();
          breakAnim = SetFollowAndRotate(screen.PtoG(screen.P3dtoP(pt)), pt,
                                         e.m_rotate.m_targetAzimut, kDoNotChangeZoom, kDoNotAutoZoom,
                                         true /* isAnim */, false /* isAutoScale */);
        }
        else
        {
          m2::AnyRectD dstRect = GetTargetRect();
          dstRect.SetAngle(e.m_rotate.m_targetAzimut);
          breakAnim = SetRect(dstRect, true);
        }
      }
      break;
    case UserEvent::EVENT_FOLLOW_AND_ROTATE:
      breakAnim = SetFollowAndRotate(e.m_followAndRotate.m_userPos, e.m_followAndRotate.m_pixelZero,
                                     e.m_followAndRotate.m_azimuth, e.m_followAndRotate.m_preferredZoomLevel,
                                     e.m_followAndRotate.m_autoScale,
                                     e.m_followAndRotate.m_isAnim, e.m_followAndRotate.m_isAutoScale);
      TouchCancel(m_touches);
      break;
    case UserEvent::EVENT_AUTO_PERSPECTIVE:
      SetAutoPerspective(e.m_autoPerspective.m_isAutoPerspective);
      break;
    default:
      ASSERT(false, ());
      break;
    }

    if (breakAnim)
      m_modelViewChanged = true;
  }

  ApplyAnimations();

  if (GetValidTouchesCount(m_touches) == 1)
  {
    if (m_state == STATE_WAIT_DOUBLE_TAP)
      DetectShortTap(m_touches[0]);
    else if (m_state == STATE_TAP_DETECTION)
      DetectLongTap(m_touches[0]);
  }

  if (m_modelViewChanged)
    m_animationSystem.UpdateLastScreen(GetCurrentScreen());

  modelViewChanged = m_modelViewChanged;

  double const kEps = 1e-5;
  viewportChanged = !m2::IsEqualSize(prevPixelRect, GetCurrentScreen().PixelRect(), kEps, kEps);
  m_modelViewChanged = false;

  return m_navigator.Screen();
}

void UserEventStream::ApplyAnimations()
{
  if (m_animationSystem.AnimationExists(Animation::MapPlane))
  {
    ScreenBase screen;
    if (m_animationSystem.GetScreen(GetCurrentScreen(), screen))
      m_navigator.SetFromScreen(screen);

    m_modelViewChanged = true;
  }
}

ScreenBase const & UserEventStream::GetCurrentScreen() const
{
  return m_navigator.Screen();
}

bool UserEventStream::SetScale(m2::PointD const & pxScaleCenter, double factor, bool isAnim)
{
  m2::PointD scaleCenter = pxScaleCenter;
  if (m_listener)
    m_listener->CorrectScalePoint(scaleCenter);

  if (isAnim)
  {
    auto const & followAnim = m_animationSystem.FindAnimation<MapFollowAnimation>(Animation::MapFollow);
    if (followAnim != nullptr && followAnim->HasScale())
    {
      // Scaling is not possible if current follow animation does pixel offset.
      if (followAnim->HasPixelOffset() && !followAnim->IsAutoZoom())
        return false;

      // Reset follow animation with scaling if we apply scale explicitly.
      ResetAnimations(Animation::MapFollow);
    }
    
    m2::PointD glbScaleCenter = m_navigator.PtoG(m_navigator.P3dtoP(scaleCenter));
    if (m_listener)
      m_listener->CorrectGlobalScalePoint(glbScaleCenter);

    ScreenBase const & startScreen = GetCurrentScreen();

    auto anim = GetScaleAnimation(startScreen, scaleCenter, glbScaleCenter, factor);
    anim->SetOnFinishAction([this](ref_ptr<Animation> animation)
    {
      if (m_listener)
        m_listener->OnAnimatedScaleEnded();
    });

    m_animationSystem.CombineAnimation(move(anim));
    return false;
  }

  ResetMapPlaneAnimations();

  m_navigator.Scale(scaleCenter, factor);
  if (m_listener)
    m_listener->OnAnimatedScaleEnded();

  return true;
}

bool UserEventStream::SetCenter(m2::PointD const & center, int zoom, bool isAnim)
{
  ScreenBase screen = GetCurrentScreen();
  if (zoom == kDoNotChangeZoom)
  {
    GetTargetScreen(screen);
    screen.MatchGandP3d(center, screen.PixelRectIn3d().Center());
  }
  else
  {
    screen.SetFromParams(center, screen.GetAngle(), GetScale(zoom));
    screen.MatchGandP3d(center, screen.PixelRectIn3d().Center());
  }

  ShrinkAndScaleInto(screen, df::GetWorldRect());

  return SetScreen(screen, isAnim);
}

bool UserEventStream::SetRect(m2::RectD rect, int zoom, bool applyRotation, bool isAnim)
{
  CheckMinGlobalRect(rect, kDefault3dScale);
  CheckMinMaxVisibleScale(rect, zoom, kDefault3dScale);
  m2::AnyRectD targetRect = applyRotation ? ToRotated(m_navigator, rect) : m2::AnyRectD(rect);
  return SetRect(targetRect, isAnim);
}

bool UserEventStream::SetScreen(ScreenBase const & endScreen, bool isAnim)
{
  if (isAnim)
  {
    auto onStartHandler = [this](ref_ptr<Animation> animation)
    {
      if (m_listener)
        m_listener->OnAnimationStarted(animation);
    };

    ScreenBase const & screen = GetCurrentScreen();

    drape_ptr<Animation> anim = GetRectAnimation(screen, endScreen);
    if (!df::IsAnimationAllowed(anim->GetDuration(), screen))
    {
      anim.reset();
      double const moveDuration = PositionInterpolator::GetMoveDuration(screen.GetOrg(),
                                                                        endScreen.GetOrg(), screen);
      if (moveDuration > kMaxAnimationTimeSec)
        anim = GetPrettyMoveAnimation(screen, endScreen);
    }

    if (anim != nullptr)
    {
      anim->SetOnStartAction(onStartHandler);
      m_animationSystem.CombineAnimation(move(anim));
      return false;
    }
  }

  ResetMapPlaneAnimations();
  m_navigator.SetFromScreen(endScreen);
  return true;
}

bool UserEventStream::SetRect(m2::AnyRectD const & rect, bool isAnim)
{
  ScreenBase tmp = GetCurrentScreen();
  tmp.SetFromRects(rect, tmp.PixelRectIn3d());
  tmp.MatchGandP3d(rect.GlobalCenter(), tmp.PixelRectIn3d().Center());

  return SetScreen(tmp, isAnim);
}

bool UserEventStream::InterruptFollowAnimations(bool force)
{
  Animation const * followAnim = m_animationSystem.FindAnimation<MapFollowAnimation>(Animation::MapFollow);

  if (followAnim == nullptr)
    followAnim = m_animationSystem.FindAnimation<SequenceAnimation>(Animation::Sequence, kPrettyFollowAnim.c_str());

  if (followAnim != nullptr)
  {
    if (force || followAnim->CouldBeInterrupted())
    {
      bool const isFollowAnim = followAnim->GetType() == Animation::MapFollow;
      ResetAnimations(followAnim->GetType());
      if (isFollowAnim)
        ResetAnimations(Animation::Arrow);
    }
    else
      return false;
  }
  return true;
}

bool UserEventStream::SetFollowAndRotate(m2::PointD const & userPos, m2::PointD const & pixelPos,
                                         double azimuth, int preferredZoomLevel, double autoScale,
                                         bool isAnim, bool isAutoScale)
{
  // Reset current follow-and-rotate animation if possible.
  if (isAnim && !InterruptFollowAnimations(false /* force */))
    return false;

  ScreenBase const & currentScreen = GetCurrentScreen();
  ScreenBase screen = currentScreen;

  if (preferredZoomLevel == kDoNotChangeZoom && !isAutoScale)
  {
    GetTargetScreen(screen);
    screen.SetAngle(-azimuth);
  }
  else
  {
    screen.SetFromParams(userPos, -azimuth, isAutoScale ? autoScale : GetScale(preferredZoomLevel));
  }
  screen.MatchGandP3d(userPos, pixelPos);

  ShrinkAndScaleInto(screen, df::GetWorldRect());

  if (isAnim)
  {
    auto onStartHandler = [this](ref_ptr<Animation> animation)
    {
      if (m_listener)
        m_listener->OnAnimationStarted(animation);
    };

    drape_ptr<Animation> anim;
    double const moveDuration = PositionInterpolator::GetMoveDuration(currentScreen.GetOrg(), screen.GetOrg(),
                                                                      currentScreen.PixelRectIn3d(),
                                                                      (currentScreen.GetScale() + screen.GetScale()) / 2.0);
    if (moveDuration > kMaxAnimationTimeSec)
    {
      // Run pretty move animation if we are far from userPos.
      anim = GetPrettyFollowAnimation(currentScreen, userPos, screen.GetScale(), -azimuth, pixelPos);
    }
    else
    {
      // Run follow-and-rotate animation.
      anim = GetFollowAnimation(currentScreen, userPos, screen.GetScale(), -azimuth, pixelPos, isAutoScale);
    }

    if (preferredZoomLevel != kDoNotChangeZoom)
    {
      anim->SetCouldBeInterrupted(false);
      anim->SetCouldBeBlended(false);
    }

    anim->SetOnStartAction(onStartHandler);
    m_animationSystem.CombineAnimation(move(anim));
    return false;
  }

  ResetMapPlaneAnimations();
  m_navigator.SetFromScreen(screen);
  return true;
}

void UserEventStream::SetAutoPerspective(bool isAutoPerspective)
{
  if (isAutoPerspective)
    m_navigator.Enable3dMode();
  else
    m_navigator.Disable3dMode();
  m_navigator.SetAutoPerspective(isAutoPerspective);
  return;
}

void UserEventStream::ResetAnimations(Animation::Type animType, bool finishAll)
{
  bool const hasAnimations = m_animationSystem.HasAnimations();
  m_animationSystem.FinishAnimations(animType, true /* rewind */, finishAll);
  if (hasAnimations)
    ApplyAnimations();
}

void UserEventStream::ResetAnimations(Animation::Type animType, string const & customType, bool finishAll)
{
  bool const hasAnimations = m_animationSystem.HasAnimations();
  m_animationSystem.FinishAnimations(animType, customType, true /* rewind */, finishAll);
  if (hasAnimations)
    ApplyAnimations();
}

void UserEventStream::ResetMapPlaneAnimations()
{
  bool const hasAnimations = m_animationSystem.HasAnimations();
  m_animationSystem.FinishObjectAnimations(Animation::MapPlane, false /* rewind */, false /* finishAll */);
  if (hasAnimations)
    ApplyAnimations();
}

m2::AnyRectD UserEventStream::GetCurrentRect() const
{
  return m_navigator.Screen().GlobalRect();
}

void UserEventStream::GetTargetScreen(ScreenBase & screen)
{
  m_animationSystem.FinishAnimations(Animation::KineticScroll, false /* rewind */, false /* finishAll */);
  ApplyAnimations();

  m_animationSystem.GetTargetScreen(m_navigator.Screen(), screen);
}

m2::AnyRectD UserEventStream::GetTargetRect()
{
  ScreenBase targetScreen;
  GetTargetScreen(targetScreen);
  return targetScreen.GlobalRect();
}

bool UserEventStream::ProcessTouch(TouchEvent const & touch)
{
  ASSERT(touch.m_touches[0].m_id != -1, ());

  TouchEvent touchEvent = touch;
  touchEvent.PrepareTouches(m_touches);
  bool isMapTouch = false;

  switch (touchEvent.m_type)
  {
  case TouchEvent::TOUCH_DOWN:
    isMapTouch = TouchDown(touchEvent.m_touches);
    break;
  case TouchEvent::TOUCH_MOVE:
    isMapTouch = TouchMove(touchEvent.m_touches, touch.m_timeStamp);
    break;
  case TouchEvent::TOUCH_CANCEL:
    isMapTouch = TouchCancel(touchEvent.m_touches);
    break;
  case TouchEvent::TOUCH_UP:
    isMapTouch = TouchUp(touchEvent.m_touches);
    break;
  default:
    ASSERT(false, ());
    break;
  }

  return isMapTouch;
}

bool UserEventStream::TouchDown(array<Touch, 2> const & touches)
{
  if (m_listener)
    m_listener->OnTouchMapAction();

  size_t touchCount = GetValidTouchesCount(touches);
  bool isMapTouch = true;
  
  // Interrupt kinetic scroll on touch down.
  m_animationSystem.FinishAnimations(Animation::KineticScroll, false /* rewind */, true /* finishAll */);

  if (touchCount == 1)
  {
    if (!DetectDoubleTap(touches[0]))
    {
      if (m_state == STATE_EMPTY)
      {
        if (!TryBeginFilter(touches[0]))
        {
          BeginTapDetector();
          m_startDragOrg = touches[0].m_location;
        }
        else
        {
          isMapTouch = false;
        }
      }
    }
  }
  else if (touchCount == 2)
  {
    switch (m_state)
    {
    case STATE_EMPTY:
      BeginTwoFingersTap(touches[0], touches[1]);
      break;
    case STATE_FILTER:
      CancelFilter(touches[0]);
      BeginScale(touches[0], touches[1]);
      break;
    case STATE_TAP_DETECTION:
    case STATE_WAIT_DOUBLE_TAP:
    case STATE_WAIT_DOUBLE_TAP_HOLD:
      CancelTapDetector();
      BeginTwoFingersTap(touches[0], touches[1]);
      break;
    case STATE_DOUBLE_TAP_HOLD:
      EndDoubleTapAndHold(touches[0]);
      BeginScale(touches[0], touches[1]);
      break;
    case STATE_DRAG:
      isMapTouch = EndDrag(touches[0], true /* cancelled */);
      BeginScale(touches[0], touches[1]);
      break;
    default:
      break;
    }
  }

  UpdateTouches(touches);
  return isMapTouch;
}

bool UserEventStream::TouchMove(array<Touch, 2> const & touches, double timestamp)
{
  if (m_listener)
    m_listener->OnTouchMapAction();

  double const kDragThreshold = my::sq(VisualParams::Instance().GetDragThreshold());
  size_t touchCount = GetValidTouchesCount(touches);
  bool isMapTouch = true;

  switch (m_state)
  {
  case STATE_EMPTY:
    if (touchCount == 1)
    {
      if (m_startDragOrg.SquareLength(touches[0].m_location) > kDragThreshold)
        BeginDrag(touches[0], timestamp);
      else
        isMapTouch = false;
    }
    else
    {
      BeginScale(touches[0], touches[1]);
    }
    break;
  case STATE_TAP_TWO_FINGERS:
    if (touchCount == 2)
    {
      float const threshold = static_cast<float>(kDragThreshold);
      if (m_twoFingersTouches[0].SquareLength(touches[0].m_location) > threshold ||
          m_twoFingersTouches[1].SquareLength(touches[1].m_location) > threshold)
        BeginScale(touches[0], touches[1]);
      else
        isMapTouch = false;
    }
    break;
  case STATE_FILTER:
    ASSERT_EQUAL(touchCount, 1, ());
    isMapTouch = false;
    break;
  case STATE_TAP_DETECTION:
  case STATE_WAIT_DOUBLE_TAP:
    if (m_startDragOrg.SquareLength(touches[0].m_location) > kDragThreshold)
      CancelTapDetector();
    else
      isMapTouch = false;
    break;
  case STATE_WAIT_DOUBLE_TAP_HOLD:
    if (m_startDragOrg.SquareLength(touches[0].m_location) > kDragThreshold)
      StartDoubleTapAndHold(touches[0]);
    break;
  case STATE_DOUBLE_TAP_HOLD:
    UpdateDoubleTapAndHold(touches[0]);
    break;
  case STATE_DRAG:
    if (touchCount > 1)
    {
      ASSERT_EQUAL(GetValidTouchesCount(m_touches), 1, ());
      EndDrag(m_touches[0], true /* cancelled */);
    }
    else
    {
      Drag(touches[0], timestamp);
    }
    break;
  case STATE_SCALE:
    if (touchCount < 2)
    {
      ASSERT_EQUAL(GetValidTouchesCount(m_touches), 2, ());
      EndScale(m_touches[0], m_touches[1]);
    }
    else
    {
      Scale(touches[0], touches[1]);
    }
    break;
  default:
    ASSERT(false, ());
    break;
  }

  UpdateTouches(touches);
  return isMapTouch;
}

bool UserEventStream::TouchCancel(array<Touch, 2> const & touches)
{
  if (m_listener)
    m_listener->OnTouchMapAction();

  size_t touchCount = GetValidTouchesCount(touches);
  UNUSED_VALUE(touchCount);
  bool isMapTouch = true;
  switch (m_state)
  {
  case STATE_EMPTY:
  case STATE_WAIT_DOUBLE_TAP:
  case STATE_TAP_TWO_FINGERS:
    isMapTouch = false;
    break;
  case STATE_WAIT_DOUBLE_TAP_HOLD:
  case STATE_DOUBLE_TAP_HOLD:
    // Do nothing here.
    break;
  case STATE_FILTER:
    ASSERT_EQUAL(touchCount, 1, ());
    CancelFilter(touches[0]);
    break;
  case STATE_TAP_DETECTION:
    ASSERT_EQUAL(touchCount, 1, ());
    CancelTapDetector();
    isMapTouch = false;
    break;
  case STATE_DRAG:
    ASSERT_EQUAL(touchCount, 1, ());
    isMapTouch = EndDrag(touches[0], true /* cancelled */);
    break;
  case STATE_SCALE:
    ASSERT_EQUAL(touchCount, 2, ());
    EndScale(touches[0], touches[1]);
    break;
  default:
    ASSERT(false, ());
    break;
  }
  UpdateTouches(touches);
  return isMapTouch;
}

bool UserEventStream::TouchUp(array<Touch, 2> const & touches)
{
  if (m_listener)
    m_listener->OnTouchMapAction();

  size_t touchCount = GetValidTouchesCount(touches);
  bool isMapTouch = true;
  switch (m_state)
  {
  case STATE_EMPTY:
    isMapTouch = false;
    // Can be if long tap or double tap detected
    break;
  case STATE_FILTER:
    ASSERT_EQUAL(touchCount, 1, ());
    EndFilter(touches[0]);
    isMapTouch = false;
    break;
  case STATE_TAP_DETECTION:
    if (touchCount == 1)
      EndTapDetector(touches[0]);
    else
      CancelTapDetector();
    break;
  case STATE_TAP_TWO_FINGERS:
    if (touchCount == 2)
      EndTwoFingersTap();
    break;
  case STATE_WAIT_DOUBLE_TAP_HOLD:
    ASSERT_EQUAL(touchCount, 1, ());
    PerformDoubleTap(touches[0]);
    break;
  case STATE_DOUBLE_TAP_HOLD:
    EndDoubleTapAndHold(touches[0]);
    break;
  case STATE_DRAG:
    ASSERT_EQUAL(touchCount, 1, ());
    isMapTouch = EndDrag(touches[0], false /* cancelled */);
    break;
  case STATE_SCALE:
    if (touchCount < 2)
    {
      ASSERT_EQUAL(GetValidTouchesCount(m_touches), 2, ());
      EndScale(m_touches[0], m_touches[1]);
    }
    else
    {
      EndScale(touches[0], touches[1]);
    }
    break;
  default:
    ASSERT(false, ());
    break;
  }

  UpdateTouches(touches);
  return isMapTouch;
}

void UserEventStream::UpdateTouches(array<Touch, 2> const & touches)
{
  m_touches = touches;
}

void UserEventStream::BeginTwoFingersTap(Touch const & t1, Touch const & t2)
{
  TEST_CALL(TWO_FINGERS_TAP);
  ASSERT_EQUAL(m_state, STATE_EMPTY, ());
  m_state = STATE_TAP_TWO_FINGERS;
  m_twoFingersTouches[0] = t1.m_location;
  m_twoFingersTouches[1] = t2.m_location;
}

void UserEventStream::EndTwoFingersTap()
{
  ASSERT_EQUAL(m_state, STATE_TAP_TWO_FINGERS, ());
  m_state = STATE_EMPTY;

  if (m_listener)
    m_listener->OnTwoFingersTap();
}

void UserEventStream::BeginDrag(Touch const & t, double timestamp)
{
  TEST_CALL(BEGIN_DRAG);
  ASSERT_EQUAL(m_state, STATE_EMPTY, ());
  m_state = STATE_DRAG;
  m_startDragOrg = m_navigator.Screen().GetOrg();
  if (m_listener)
    m_listener->OnDragStarted();
  m_navigator.StartDrag(t.m_location);

  if (m_kineticScrollEnabled && !m_scroller.IsActive())
  {
    ResetMapPlaneAnimations();
    m_scroller.InitGrab(m_navigator.Screen(), timestamp);
  }
}

void UserEventStream::Drag(Touch const & t, double timestamp)
{
  TEST_CALL(DRAG);
  ASSERT_EQUAL(m_state, STATE_DRAG, ());
  m_navigator.DoDrag(t.m_location);

  if (m_kineticScrollEnabled && m_scroller.IsActive())
    m_scroller.GrabViewRect(m_navigator.Screen(), timestamp);
}

bool UserEventStream::EndDrag(Touch const & t, bool cancelled)
{
  TEST_CALL(END_DRAG);
  ASSERT_EQUAL(m_state, STATE_DRAG, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnDragEnded(m_navigator.GtoP(m_navigator.Screen().GetOrg()) - m_navigator.GtoP(m_startDragOrg));

  m_startDragOrg = m2::PointD::Zero();
  m_navigator.StopDrag(t.m_location);

  if (cancelled)
  {
    m_scroller.CancelGrab();
    return true;
  }

  if (m_kineticScrollEnabled && m_kineticTimer.TimeElapsedAs<milliseconds>().count() >= kKineticDelayMs)
  {
    drape_ptr<Animation> anim = m_scroller.CreateKineticAnimation(m_navigator.Screen());
    if (anim != nullptr)
    {
      anim->SetOnStartAction([this](ref_ptr<Animation> animation)
      {
        if (m_listener)
          m_listener->OnAnimationStarted(animation);
      });
      m_animationSystem.CombineAnimation(move(anim));
    }
    m_scroller.CancelGrab();
    return false;
  }

  return true;
}

void UserEventStream::BeginScale(Touch const & t1, Touch const & t2)
{
  TEST_CALL(BEGIN_SCALE);

  if (m_state == STATE_SCALE)
  {
    Scale(t1, t2);
    return;
  }

  ASSERT(m_state == STATE_EMPTY || m_state == STATE_TAP_TWO_FINGERS, ());
  m_state = STATE_SCALE;
  m2::PointD touch1 = t1.m_location;
  m2::PointD touch2 = t2.m_location;

  if (m_listener)
  {
    m_listener->OnScaleStarted();
    m_listener->CorrectScalePoint(touch1, touch2);
  }

  m_navigator.StartScale(touch1, touch2);
}

void UserEventStream::Scale(Touch const & t1, Touch const & t2)
{
  TEST_CALL(SCALE);
  ASSERT_EQUAL(m_state, STATE_SCALE, ());

  m2::PointD touch1 = t1.m_location;
  m2::PointD touch2 = t2.m_location;

  if (m_listener)
  {
    if (m_navigator.IsRotatingDuringScale())
      m_listener->OnRotated();

    m_listener->CorrectScalePoint(touch1, touch2);
  }

  m_navigator.DoScale(touch1, touch2);
}

void UserEventStream::EndScale(const Touch & t1, const Touch & t2)
{
  TEST_CALL(END_SCALE);
  ASSERT_EQUAL(m_state, STATE_SCALE, ());
  m_state = STATE_EMPTY;

  m2::PointD touch1 = t1.m_location;
  m2::PointD touch2 = t2.m_location;

  if (m_listener)
  {
    m_listener->CorrectScalePoint(touch1, touch2);
    m_listener->OnScaleEnded();
  }

  m_navigator.StopScale(touch1, touch2);

  m_kineticTimer.Reset();
}

void UserEventStream::BeginTapDetector()
{
  TEST_CALL(BEGIN_TAP_DETECTOR);
  ASSERT_EQUAL(m_state, STATE_EMPTY, ());
  m_state = STATE_TAP_DETECTION;
  m_touchTimer.Reset();
}

void UserEventStream::DetectShortTap(Touch const & touch)
{
  if (DetectForceTap(touch))
    return;

  uint64_t const ms = m_touchTimer.TimeElapsedAs<milliseconds>().count();
  if (ms > kDoubleTapPauseMs)
  {
    m_state = STATE_EMPTY;
    if (m_listener)
      m_listener->OnTap(touch.m_location, false /* isLongTap */);
  }
}

void UserEventStream::DetectLongTap(Touch const & touch)
{
  ASSERT_EQUAL(m_state, STATE_TAP_DETECTION, ());

  if (DetectForceTap(touch))
    return;

  uint64_t const ms = m_touchTimer.TimeElapsedAs<milliseconds>().count();
  if (ms > kLongTouchMs)
  {
    TEST_CALL(LONG_TAP_DETECTED);
    m_state = STATE_EMPTY;
    if (m_listener)
      m_listener->OnTap(touch.m_location, true /* isLongTap */);
  }
}

bool UserEventStream::DetectDoubleTap(Touch const & touch)
{
  uint64_t const ms = m_touchTimer.TimeElapsedAs<milliseconds>().count();
  if (m_state != STATE_WAIT_DOUBLE_TAP || ms > kDoubleTapPauseMs)
    return false;

  m_state = STATE_WAIT_DOUBLE_TAP_HOLD;

  return true;
}
  
void UserEventStream::PerformDoubleTap(Touch const & touch)
{
  ASSERT_EQUAL(m_state, STATE_WAIT_DOUBLE_TAP_HOLD, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnDoubleTap(touch.m_location);
}

bool UserEventStream::DetectForceTap(Touch const & touch)
{
  if (touch.m_force >= kForceTapThreshold)
  {
    m_state = STATE_EMPTY;
    if (m_listener)
      m_listener->OnForceTap(touch.m_location);
    return true;
  }

  return false;
}

void UserEventStream::EndTapDetector(Touch const & touch)
{
  TEST_CALL(SHORT_TAP_DETECTED);
  ASSERT_EQUAL(m_state, STATE_TAP_DETECTION, ());
  m_state = STATE_WAIT_DOUBLE_TAP;
}

void UserEventStream::CancelTapDetector()
{
  TEST_CALL(CANCEL_TAP_DETECTOR);
  ASSERT(m_state == STATE_TAP_DETECTION || m_state == STATE_WAIT_DOUBLE_TAP || m_state == STATE_WAIT_DOUBLE_TAP_HOLD, ());
  m_state = STATE_EMPTY;
}

bool UserEventStream::TryBeginFilter(Touch const & t)
{
  TEST_CALL(TRY_FILTER);
  ASSERT_EQUAL(m_state, STATE_EMPTY, ());
  if (m_listener && m_listener->OnSingleTouchFiltrate(t.m_location, TouchEvent::TOUCH_DOWN))
  {
    m_state = STATE_FILTER;
    return true;
  }

  return false;
}

void UserEventStream::EndFilter(const Touch & t)
{
  TEST_CALL(END_FILTER);
  ASSERT_EQUAL(m_state, STATE_FILTER, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnSingleTouchFiltrate(t.m_location, TouchEvent::TOUCH_UP);
}

void UserEventStream::CancelFilter(Touch const & t)
{
  TEST_CALL(CANCEL_FILTER);
  ASSERT_EQUAL(m_state, STATE_FILTER, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnSingleTouchFiltrate(t.m_location, TouchEvent::TOUCH_CANCEL);
}
  
void UserEventStream::StartDoubleTapAndHold(Touch const & touch)
{
  TEST_CALL(BEGIN_DOUBLE_TAP_AND_HOLD);
  ASSERT_EQUAL(m_state, STATE_WAIT_DOUBLE_TAP_HOLD, ());
  m_state = STATE_DOUBLE_TAP_HOLD;
  m_startDoubleTapAndHold = m_startDragOrg;
  if (m_listener)
    m_listener->OnScaleStarted();
}

void UserEventStream::UpdateDoubleTapAndHold(Touch const & touch)
{
  TEST_CALL(DOUBLE_TAP_AND_HOLD);
  ASSERT_EQUAL(m_state, STATE_DOUBLE_TAP_HOLD, ());
  float const kPowerModifier = 10.0f;
  float const scaleFactor = exp(kPowerModifier * (touch.m_location.y - m_startDoubleTapAndHold.y) / GetCurrentScreen().PixelRectIn3d().SizeY());
  m_startDoubleTapAndHold = touch.m_location;

  m2::PointD scaleCenter = m_startDragOrg;
  if (m_listener)
    m_listener->CorrectScalePoint(scaleCenter);
  m_navigator.Scale(scaleCenter, scaleFactor);
}

void UserEventStream::EndDoubleTapAndHold(Touch const & touch)
{
  TEST_CALL(END_DOUBLE_TAP_AND_HOLD);
  ASSERT_EQUAL(m_state, STATE_DOUBLE_TAP_HOLD, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnScaleEnded();
}

bool UserEventStream::IsInUserAction() const
{
  return m_state == STATE_DRAG || m_state == STATE_SCALE;
}

bool UserEventStream::IsWaitingForActionCompletion() const
{
  return m_state != STATE_EMPTY;
}

void UserEventStream::SetKineticScrollEnabled(bool enabled)
{
  m_kineticScrollEnabled = enabled;
  m_kineticTimer.Reset();
  if (!m_kineticScrollEnabled)
    m_scroller.CancelGrab();
}

}
