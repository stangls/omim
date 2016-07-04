package com.mapswithme.mx;

/**
 * Created by sd on 27.05.16.
 */
public interface TourStatusListener {
  /** will be called only on changes, i.e. once for each tour. **/
  public void onTourFinished();
  /** will be called only on changes, i.e. not at the beginning. **/
  public void onTourTracking(boolean tracking, boolean tourWasAlreadyLeftOnce);
}
