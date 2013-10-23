package com.mapswithme.maps;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.Path;
import android.util.AttributeSet;
import android.util.Log;
import android.widget.ImageView;

public class ArrowImage extends ImageView
{
  static private String TAG = "ArrowImage";

  private float mWidth;
  private float mHeight;

  private final static float RAD_MULT = .33f;
  private final static float SQ2 = (float) Math.sqrt(2);

  private Paint mArrowPaint;
  private Path mArrowPath;
  private Paint mCirclePaint;

  private final static int ALLOW_ANIMATION = 0x1;
  private final static int DO_ANIMATION = 0x2;
  private final static int DRAW_ARROW = 0x4;
  private final static int DRAW_CIRCLE = 0x8;
  private int m_flags = 0;

  private boolean testFlag(int flag)
  {
    return (m_flags & flag) != 0;
  }
  private void setFlag(int flag, boolean value)
  {
    if (value)
      m_flags |= flag;
    else
      m_flags &= (~flag);
  }

  // Animation params
  private float mCurrentAngle;
  private float mAngle;

  private final static long   UPDATE_RATE = 30;
  private final static long   UPDATE_DELAY = 1000 / UPDATE_RATE;
  private final static double ROTATION_SPEED = 120;
  private final static double ROTATION_STEP = ROTATION_SPEED / UPDATE_RATE;
  private final static double ERR = ROTATION_STEP;

  private Runnable mAnimateTask = new Runnable()
  {
    @Override
    public void run()
    {
      if (step())
        postDelayed(this, UPDATE_DELAY);
    }
  };

  public ArrowImage(Context context, AttributeSet attrs)
  {
    super(context, attrs);

    mArrowPaint = new Paint();
    mArrowPaint.setStyle(Style.FILL);
    mArrowPaint.setColor(0xFF333333);
    mArrowPaint.setAntiAlias(true);

    mArrowPath = new Path();

    mCirclePaint = new Paint();
    mCirclePaint.setColor(0xAFFFFFFF);
    mCirclePaint.setAntiAlias(true);
  }

  public void setFlag(Resources res, String packageName, String flag)
  {
    setVisibility(VISIBLE);

    setFlag(DRAW_ARROW, false);

    // The aapt can't process resources with name "do". Hack with renaming.
    if (flag.equals("do"))
      flag = "do_hack";

    final int id = res.getIdentifier(flag, "drawable", packageName);
    if (id > 0)
    {
      setImageDrawable(res.getDrawable(id));
    }
    else
    {
      setImageDrawable(null);

      Log.e(TAG, "Failed to get resource id from: " + flag);
    }

    invalidate();
  }

  public void setDrawCircle(boolean draw)
  {
    setFlag(DRAW_CIRCLE, draw);
  }

  public void setAnimation(boolean allow)
  {
    setFlag(ALLOW_ANIMATION, allow);
  }

  public void setCircleColor(int color)
  {
    mCirclePaint.setColor(color);
  }

  public void setAzimut(double azimut)
  {
    setVisibility(VISIBLE);
    setImageDrawable(null);

    setFlag(DRAW_ARROW, true);
    mAngle = (float) (azimut / Math.PI * 180.0);

    if (testFlag(ALLOW_ANIMATION) && testFlag(DO_ANIMATION))
      animateRotation();
    else
    {
      mCurrentAngle = mAngle; // to skip rotation from 0
      setFlag(DO_ANIMATION, true);
    }

    invalidate();
  }

  private void animateRotation()
  {
    removeCallbacks(mAnimateTask);
    post(mAnimateTask);
  }

  public void clear()
  {
    setVisibility(INVISIBLE);
    setImageDrawable(null);
    setFlag(DRAW_ARROW, false);
  }

  @Override
  protected void onDetachedFromWindow()
  {
    super.onDetachedFromWindow();
    removeCallbacks(mAnimateTask);
  }

  private boolean step()
  {
    final double diff = mAngle - mCurrentAngle;
    if (Math.abs(diff) > ERR)
    {
      // Choosing the shortest way at [0, 360] looped segment
      final double signum = -1 * Math.signum(diff) * Math.signum(Math.abs(diff) - 180);
      mCurrentAngle += signum * ROTATION_STEP;
      if (mCurrentAngle < 0)
        mCurrentAngle += 360;
      else if (mCurrentAngle > 360)
        mCurrentAngle -= 360;

      invalidate();
      return true;
    }
    return false;
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh)
  {
    super.onSizeChanged(w, h, oldw, oldh);

    mWidth = w - getPaddingLeft() - getPaddingRight();
    mHeight = h - getPaddingBottom() - getPaddingTop();

    final float rad = RAD_MULT * Math.min(w, h);
    final float c0 = Math.min(h, w) / 2;

    mArrowPath.reset();
    mArrowPath.moveTo(c0 + rad, c0);
    mArrowPath.lineTo((float) (c0 - rad / SQ2), (float) (c0 + rad / SQ2));
    mArrowPath.lineTo((float) (c0 - rad * SQ2 / 4), c0);
    mArrowPath.lineTo((float) (c0 - rad / SQ2), (float) (c0 - rad / SQ2));
    mArrowPath.lineTo(c0 + rad, c0);
    mArrowPath.close();
  }

  @Override
  protected void onDraw(Canvas canvas)
  {
    super.onDraw(canvas);
    if (mWidth <= 0 || mHeight <= 0)
      return;

    if (testFlag(DRAW_ARROW))
    {
      canvas.save();

      final float w = mWidth;
      final float h = mHeight;
      final float sSide = Math.min(w, h);
      canvas.translate((w - sSide) / 2, (h - sSide) / 2);
      canvas.rotate(-mCurrentAngle, sSide / 2, sSide / 2);

      if (testFlag(DRAW_CIRCLE))
      {
        final float rad = Math.min(w, h) / 2;
        canvas.drawCircle(rad, rad, rad, mCirclePaint);
      }

      canvas.drawPath(mArrowPath, mArrowPaint);
      canvas.restore();
    }
  }
}