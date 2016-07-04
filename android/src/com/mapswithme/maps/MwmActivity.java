package com.mapswithme.maps;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.location.Location;
import android.media.MediaPlayer;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AlertDialog;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.Serializable;
import java.util.LinkedList;
import java.util.Stack;
import java.util.concurrent.atomic.AtomicBoolean;

import com.mapswithme.maps.Framework.MapObjectListener;
import com.mapswithme.maps.activity.CustomNavigateUpListener;
import com.mapswithme.maps.ads.LikesManager;
import com.mapswithme.maps.api.ParsedMwmRequest;
import com.mapswithme.maps.base.BaseMwmFragmentActivity;
import com.mapswithme.maps.base.OnBackPressListener;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.downloader.MapManager;
import com.mapswithme.maps.downloader.OnmapDownloader;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.location.LocationPredictor;
import com.mapswithme.maps.routing.NavigationController;
import com.mapswithme.maps.routing.RoutingController;
import com.mapswithme.maps.routing.RoutingInfo;
import com.mapswithme.maps.routing.RoutingPlanFragment;
import com.mapswithme.maps.routing.RoutingPlanInplaceController;
import com.mapswithme.maps.search.FloatingSearchToolbarController;
import com.mapswithme.maps.search.SearchActivity;
import com.mapswithme.maps.search.SearchEngine;
import com.mapswithme.maps.search.SearchFragment;
import com.mapswithme.maps.settings.SettingsActivity;
import com.mapswithme.maps.settings.StoragePathManager;
import com.mapswithme.maps.settings.UnitLocale;
import com.mapswithme.maps.sound.TtsPlayer;
import com.mapswithme.maps.widget.FadeView;
import com.mapswithme.maps.widget.menu.MainMenu;
import com.mapswithme.mx.TourFinishedListener;
import com.mapswithme.mx.TourLoadedListener;
import com.mapswithme.util.Animations;
import com.mapswithme.util.BottomSheetHelper;
import com.mapswithme.util.InputUtils;
import com.mapswithme.util.LocationUtils;
import com.mapswithme.util.ThemeUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.Yota;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.sharing.ShareOption;
import com.mapswithme.util.sharing.SharingHelper;
import com.mapswithme.util.statistics.AlohaHelper;
//import com.mapswithme.util.statistics.MytargetHelper;
import com.mapswithme.util.statistics.Statistics;
import com.mobidat.dao.impl.DaoTour;
import com.mobidat.persistence.Tour;
import com.mobidat.wp2.configuration.Parameter;
import com.mobidat.wp2.databaseaccess.DbHelper;
import com.mobidat.wp2.gpsProvider.GPS;
import com.mobidat.wp2.gpsProvider.GPSInfo;
import com.mobidat.wp2.gpsProvider.ILocationReceiver;
import com.mobidat.wp2.missionrecording.MissionAccess;
import com.mobidat.wp2.missionrecording.TimerThread;
import com.mobidat.wp2.missionservice.MissionListener;
import com.mobidat.wp2.missionservice.MissionStatus;

//import ru.mail.android.mytarget.nativeads.NativeAppwallAd;
//import ru.mail.android.mytarget.nativeads.banners.NativeAppwallBanner;

public class MwmActivity extends BaseMwmFragmentActivity
                      implements LocationHelper.LocationListener,
                                 MapObjectListener,
                                 View.OnTouchListener,
                                 //BasePlacePageAnimationController.OnVisibilityChangedListener,
                                 OnClickListener,
                                 MapFragment.MapRenderingListener,
                                 CustomNavigateUpListener,
                                 RoutingController.Container, Framework.PoiVisitedListener, MissionListener, Framework.PossibleTourResumptionListener, TourFinishedListener, TourLoadedListener, ILocationReceiver {

  private static final String TAG = MwmActivity.class.getName();

  public final AtomicBoolean activityIsVisible = new AtomicBoolean(false);

  public static final String EXTRA_TASK = "map_task";
  private static final String EXTRA_CONSUMED = "mwm.extra.intent.processed";
  private static final String EXTRA_UPDATE_COUNTRIES = ".extra.update.countries";

  private static final String[] DOCKED_FRAGMENTS = { SearchFragment.class.getName(),
                                                     //DownloaderFragment.class.getName(),
                                                     //MigrationFragment.class.getName(),
                                                     RoutingPlanFragment.class.getName()
                                                     //EditorHostFragment.class.getName(),
                                                     //ReportFragment.class.getName()
                                                   };
  // Instance state
  private static final String STATE_PP_OPENED = "PpOpened";
  private static final String STATE_MAP_OBJECT = "MapObject";

  // Map tasks that we run AFTER rendering initialized
  private final Stack<MapTask> mTasks = new Stack<>();
  private final StoragePathManager mPathManager = new StoragePathManager();

  private View mMapFrame;

  private MapFragment mMapFragment;
  //private PlacePageView mPlacePage;

  private RoutingPlanInplaceController mRoutingPlanInplaceController;
  private NavigationController mNavigationController;

  private MainMenu mMainMenu;
  private PanelAnimator mPanelAnimator;
  private OnmapDownloader mOnmapDownloader;
  //private MytargetHelper mMytargetHelper;

  private FadeView mFadeView;

  private ImageButton mBtnZoomIn;
  private ImageButton mBtnZoomOut;
  private ImageButton mButtonContinueTourHere;

  //private View mPositionChooser;

  private boolean mIsFragmentContainer;
  private boolean mIsFullscreen;
  private boolean mIsFullscreenAnimating;

  private LocationPredictor mLocationPredictor;
  private FloatingSearchToolbarController mSearchController;
  private LastCompassData mLastCompassData;

  // The first launch of application ever - onboarding screen will be shown.
  private boolean mFirstStart;
  // The first launch after process started. Used to skip "Not follow, no position" state and to run locator.
  private static boolean sColdStart = true;
  private static boolean sLocationStopped;

  private TimerThread timerThread = new TimerThread(this);
  private LinkedList<String> poiMessages = new LinkedList<>();
  private AlertDialog poiDialog;
  private ImageButton mBreakButton;
  private View mRowMissionActivity;
  private TextView mTextMissionActivity;
  private View mLegend;
  private MediaPlayer mediaPlayer;
  private boolean gpsSimulationActive = false;
  private ImageButton mSimulationPauseButton;
  private static MwmActivity instance = null;

  public static MwmActivity getInstance() {
    return instance;
  }

  @Override
  public void onTourFinished() {
    new Thread(){public void run(){
      MwmApplication.gps().pauseEmulation();
      try { Thread.sleep(3000); } catch (InterruptedException ignored) {}
      runOnUiThread(new Runnable(){public void run(){
        finish();
      }});
    }}.start();
  }

  @Override
  public void onTourLoaded(final String name) {
    runOnUiThread(new Runnable(){public void run(){
      ((TextView)findViewById(R.id.textTourName)).setText(name);
    }});
  }

  @Override
  public void onLocationChanged(@Nullable Location location) {}

  @Override
  public void onStatusChanged(@NonNull GPSInfo gpsInfo) {
    Boolean simuActive = gpsInfo.simulationActive;
    if (simuActive!=null){
      updateGpsSimulationActive(simuActive);
    }
  }

  public interface LeftAnimationTrackListener
  {
    void onTrackStarted(boolean collapsed);

    void onTrackFinished(boolean collapsed);

    void onTrackLeftAnimation(float offset);
  }

  private static class LastCompassData
  {
    double magneticNorth;
    double trueNorth;
    double north;

    void update(int rotation, double magneticNorth, double trueNorth)
    {
      this.magneticNorth = LocationUtils.correctCompassAngle(rotation, magneticNorth);
      this.trueNorth = LocationUtils.correctCompassAngle(rotation, trueNorth);
      north = (this.trueNorth >= 0.0) ? this.trueNorth : this.magneticNorth;
    }
  }

  public static Intent createShowMapIntent(Context context, String countryId, boolean doAutoDownload)
  {
    return new Intent(context, DownloadResourcesActivity.class)
               .putExtra(DownloadResourcesActivity.EXTRA_COUNTRY, countryId)
               .putExtra(DownloadResourcesActivity.EXTRA_AUTODOWNLOAD, doAutoDownload);
  }

  public static Intent createUpdateMapsIntent()
  {
    return new Intent(MwmApplication.get(), MwmActivity.class)
               .putExtra(EXTRA_UPDATE_COUNTRIES, true);
  }

  @Override
  public void onRenderingInitialized()
  {
    checkMeasurementSystem();
    checkKitkatMigrationMove();

    runTasks();

    RoutingController.continueSavedTour(MwmActivity.this);
  }

  private void runTasks()
  {
    while (!mTasks.isEmpty())
      mTasks.pop().run(this);
  }

  private static void checkMeasurementSystem()
  {
    UnitLocale.initializeCurrentUnits();
  }

  private void checkKitkatMigrationMove()
  {
    mPathManager.checkKitkatMigration(this);
  }

  @Override
  protected int getFragmentContentResId()
  {
    return (mIsFragmentContainer ? R.id.fragment_container
                                 : super.getFragmentContentResId());
  }

  @Nullable
  public Fragment getFragment(Class<? extends Fragment> clazz)
  {
    if (!mIsFragmentContainer)
      throw new IllegalStateException("Must be called for tablets only!");

    return getSupportFragmentManager().findFragmentByTag(clazz.getName());
  }

  void replaceFragmentInternal(Class<? extends Fragment> fragmentClass, Bundle args)
  {
    super.replaceFragment(fragmentClass, args, null);
  }

  @Override
  public void replaceFragment(@NonNull Class<? extends Fragment> fragmentClass, @Nullable Bundle args, @Nullable Runnable completionListener)
  {
    if (mPanelAnimator.isVisible() && getFragment(fragmentClass) != null)
    {
      if (completionListener != null)
        completionListener.run();
      return;
    }

    mPanelAnimator.show(fragmentClass, args, completionListener);
  }
/*
  private void showBookmarks()
  {
    startActivity(new Intent(this, BookmarkCategoriesActivity.class));
  }
*/
  public void showSearch(String query)
  {
    if (mIsFragmentContainer)
    {
      mSearchController.hide();

      final Bundle args = new Bundle();
      args.putString(SearchActivity.EXTRA_QUERY, query);
      replaceFragment(SearchFragment.class, args, null);
    }
    else
      SearchActivity.start(this, query);
  }
/*
  public void showEditor()
  {
    // TODO(yunikkk) think about refactoring. It probably should be called in editor.
    Editor.nativeStartEdit();
    Statistics.INSTANCE.trackEditorLaunch(false);
    if (mIsFragmentContainer)
      replaceFragment(EditorHostFragment.class, null, null);
    else
      EditorActivity.start(this);
  }
*/
  private void shareMyLocation()
  {
    final Location loc = LocationHelper.INSTANCE.getSavedLocation();
    if (loc != null)
    {
      final String geoUrl = Framework.nativeGetGe0Url(loc.getLatitude(), loc.getLongitude(), Framework.nativeGetDrawScale(), "");
      final String httpUrl = Framework.getHttpGe0Url(loc.getLatitude(), loc.getLongitude(), Framework.nativeGetDrawScale(), "");
      final String body = getString(R.string.my_position_share_sms, geoUrl, httpUrl);
      ShareOption.ANY.share(this, body);
      return;
    }

    new AlertDialog.Builder(MwmActivity.this)
        .setMessage(R.string.unknown_current_position)
        .setCancelable(true)
        .setPositiveButton(android.R.string.ok, null)
        .show();
  }

  @Override
  public void showDownloader(boolean openDownloaded)
  {
    /*
    if (RoutingController.get().checkMigration(this))
      return;

    final Bundle args = new Bundle();
    args.putBoolean(DownloaderActivity.EXTRA_OPEN_DOWNLOADED, openDownloaded);
    if (mIsFragmentContainer)
    {
      SearchEngine.cancelSearch();
      mSearchController.refreshToolbar();
      replaceFragment(MapManager.nativeIsLegacyMode() ? MigrationFragment.class : DownloaderFragment.class, args, null);
    }
    else
    {
      startActivity(new Intent(this, DownloaderActivity.class).putExtras(args));
    }
    */
    Log.w(TAG, "showDownloader: downloading disabled. it seems like map-files are missing!" );
  }

  @Override
  public int getThemeResourceId(String theme)
  {
    if (ThemeUtils.isDefaultTheme(theme))
      return R.style.MwmTheme_MainActivity;

    if (ThemeUtils.isNightTheme(theme))
      return R.style.MwmTheme_Night_MainActivity;

    return super.getThemeResourceId(theme);
  }

  @SuppressLint("InlinedApi")
  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    Log.d(TAG, "onCreate: start");
    super.onCreate(savedInstanceState);
    int i = 1;
    Log.d(TAG, "onCreate: "+(i++));
    activityIsVisible.set(false);

    mIsFragmentContainer = getResources().getBoolean(R.bool.tabletLayout);

    if (!mIsFragmentContainer && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP))
      getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
    Log.d(TAG, "onCreate: "+(i++));

    setContentView(R.layout.activity_map);
    Log.d(TAG, "onCreate: "+(i++));
    initViews();
    Log.d(TAG, "onCreate: "+(i++));

    Statistics.INSTANCE.trackConnectionState();
    Log.d(TAG, "onCreate: "+(i++));

    Framework.nativeSetMapObjectListener(this);
    Log.d(TAG, "onCreate: "+(i++));

    mSearchController = new FloatingSearchToolbarController(this);
    mLocationPredictor = new LocationPredictor(new Handler(), this);
    processIntent(getIntent());
    SharingHelper.prepare();
    Log.d(TAG, "onCreate: "+(i++));

    poiDialog = new AlertDialog.Builder(MwmActivity.this)
      .setTitle("Meldung")
      .setPositiveButton("OK", new DialogInterface.OnClickListener() { public void onClick(DialogInterface dialog, int which) {
        dialog.dismiss();
      }})
      .setOnDismissListener(new DialogInterface.OnDismissListener() { public void onDismiss(DialogInterface dialog) {
        MwmActivity.this.hideStatusBar();
        new Thread(){public void run(){
          try { Thread.sleep(1000); } catch (InterruptedException ignored) {}
          runOnUiThread(new Runnable() { public void run() {
            synchronized (poiMessages){
              if (poiMessages.size()>0){
                if (!poiDialog.isShowing()){
                  showPoiDialogNow(poiMessages.poll());
                }
              }
            }
          }});
        }}.start();
      }})
      .create();
    Framework.nativeSetPoiVisitedListener(this);
    Framework.nativeSetPossibleTourResumptionListener(this);

    hideStatusBar();

    Log.d(TAG, "onCreate: "+(i++));

    MissionAccess.init(this);
    MissionAccess.listeningActivity  = this;

    Log.d(TAG, "onCreate: "+(i++));

    mBreakButton = (ImageButton)findViewById(R.id.breakButton);
    mSimulationPauseButton = (ImageButton)findViewById(R.id.simulationPauseButton);
    mLegend = (View)findViewById(R.id.legend);

    Log.d(TAG, "onCreate: "+(i++));

    new Thread(){ public void run(){
      while (mRowMissionActivity==null){
        runOnUiThread(new Runnable() { public void run() {
          mRowMissionActivity = findViewById(R.id.rowMissionActivity);
          if (mRowMissionActivity!=null){
            mRowMissionActivity.setVisibility(View.GONE);
          }
        }});
        try { Thread.sleep(1000); } catch (InterruptedException ignored) {}
      }
      while (mTextMissionActivity==null){
        runOnUiThread(new Runnable() { public void run() {
          mTextMissionActivity = (TextView)findViewById(R.id.textMissionActivity);
        }});
        try { Thread.sleep(1000); } catch (InterruptedException ignored) {}
      }
      timerThread.start();
    }}.start();

    Log.d(TAG, "onCreate: "+(i++));

    instance = this;
    Log.d(TAG, "onCreate: done");
  }

  private void initViews()
  {
    initMap();
    initYota();
    //initPlacePage();
    initNavigationButtons();

    if (!mIsFragmentContainer)
    {
      mRoutingPlanInplaceController = new RoutingPlanInplaceController(this);
      removeCurrentFragment(false);
    }

    mNavigationController = new NavigationController(this);
    initMenu();
    initOnmapDownloader();
    //initPositionChooser();
  }
/*
  private void initPositionChooser()
  {
    mPositionChooser = findViewById(R.id.position_chooser);
    final Toolbar toolbar = (Toolbar) mPositionChooser.findViewById(R.id.toolbar_position_chooser);
    UiUtils.showHomeUpButton(toolbar);
    toolbar.setNavigationOnClickListener(new OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        showPositionChooser(false);
      }
    });
    mPositionChooser.findViewById(R.id.done).setOnClickListener(new OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        Statistics.INSTANCE.trackEditorLaunch(true);
        showPositionChooser(false);
        if (Framework.nativeIsDownloadedMapAtScreenCenter())
          startActivity(new Intent(MwmActivity.this, FeatureCategoryActivity.class));
        else
          UiUtils.showAlertDialog(getActivity(), R.string.message_invalid_feature_position);
      }
    });
    UiUtils.hide(mPositionChooser);
  }

  public void showPositionChooser(boolean show)
  {
    UiUtils.showIf(show, mPositionChooser);
    setFullscreen(show);
    Framework.nativeTurnChoosePositionMode(show, false /-* isBusiness *-/); //TODO(Android team): set isBusiness correctly
    closePlacePage();
    mSearchController.hide();
  }
*/
  private void initMap()
  {
    mMapFrame = findViewById(R.id.map_fragment_container);

    mFadeView = (FadeView) findViewById(R.id.fade_view);
    mFadeView.setListener(new FadeView.Listener()
    {
      @Override
      public boolean onTouch()
      {
        return mMainMenu.close(true);
      }
    });

    mMapFragment = (MapFragment) getSupportFragmentManager().findFragmentByTag(MapFragment.class.getName());
    if (mMapFragment == null)
    {
      mMapFragment = (MapFragment) MapFragment.instantiate(this, MapFragment.class.getName(), null);
      getSupportFragmentManager()
          .beginTransaction()
          .replace(R.id.map_fragment_container, mMapFragment, MapFragment.class.getName())
          .commit();
    }
    mMapFrame.setOnTouchListener(this);
  }

  private void initNavigationButtons()
  {
    View frame = findViewById(R.id.navigation_buttons);
    mBtnZoomIn = (ImageButton) frame.findViewById(R.id.map_button_plus);
    mBtnZoomIn.setImageResource(ThemeUtils.isNightTheme() ? R.drawable.zoom_in_night
                                                          : R.drawable.zoom_in);
    mBtnZoomIn.setOnClickListener(this);
    mBtnZoomOut = (ImageButton) frame.findViewById(R.id.map_button_minus);
    mBtnZoomOut.setOnClickListener(this);
    mBtnZoomOut.setImageResource(ThemeUtils.isNightTheme() ? R.drawable.zoom_out_night
                                                           : R.drawable.zoom_out);

    mButtonContinueTourHere = (ImageButton)findViewById(R.id.buttonContinueTourHere);
    mButtonContinueTourHere.setVisibility(View.GONE);
    mButtonContinueTourHere.setOnClickListener(this);
  }
/*
  private void initPlacePage()
  {
    mPlacePage = (PlacePageView) findViewById(R.id.info_box);
    mPlacePage.setOnVisibilityChangedListener(this);
    mPlacePage.findViewById(R.id.ll__route).setOnClickListener(this);
  }
*/
  private void initYota()
  {
    if (Yota.isFirstYota())
      findViewById(R.id.yop_it).setOnClickListener(this);
  }
/*
  private boolean closePlacePage()
  {
    if (mPlacePage.getState() == State.HIDDEN)
      return false;

    mPlacePage.hide();
    Framework.nativeDeactivatePopup();
    return true;
  }
*/
  public boolean closeSidePanel()
  {
    if (interceptBackPress())
      return true;

    if (removeCurrentFragment(true))
    {
      InputUtils.hideKeyboard(mFadeView);
      mFadeView.fadeOut(false);
      return true;
    }

    return false;
  }

  public void closeMenu(String statEvent, String alohaStatEvent, @Nullable Runnable procAfterClose)
  {
    Statistics.INSTANCE.trackEvent(statEvent);
    AlohaHelper.logClick(alohaStatEvent);

    mFadeView.fadeOut(false);
    mMainMenu.close(true, procAfterClose);
  }
/*
  private boolean closePositionChooser()
  {
    if (UiUtils.isVisible(mPositionChooser))
    {
      showPositionChooser(false);
      return true;
    }

    return false;
  }
*/
  private void startLocationToPoint(String statisticsEvent, String alohaEvent, final @Nullable MapObject endPoint)
  {
    closeMenu(statisticsEvent, alohaEvent, new Runnable()
    {
      @Override
      public void run()
      {
        RoutingController.get().prepare(endPoint);

        /*if (mPlacePage.isDocked() || !mPlacePage.isFloating())
          closePlacePage();*/
      }
    });
  }

  private void startTour(String statisticsEvent, String alohaEvent) {
    Log.i(TAG, "startTour: close-menu and calling native function");
    closeMenu(statisticsEvent, alohaEvent, new Runnable() {
      @Override
      public void run() {
        //RoutingController.get().prepare(endPoint);

        RoutingController.get().startTour("/storage/emulated/legacy/mobidat/tour.xml", 0, MwmActivity.this);

        /*if (mPlacePage.isDocked() || !mPlacePage.isFloating())
          closePlacePage();*/
      }
    });
  }

  private void toggleMenu()
  {
    if (mMainMenu.isOpen())
      mFadeView.fadeOut(false);
    else
      mFadeView.fadeIn();

    //mMainMenu.toggle(true);
  }

  private void initMenu()
  {
    mMainMenu = new MainMenu((ViewGroup) findViewById(R.id.menu_frame), new MainMenu.Container()
    {
      @Override
      public Activity getActivity()
      {
        return MwmActivity.this;
      }

      @Override
      public void onItemClick(MainMenu.Item item)
      {
        if (mIsFullscreenAnimating)
          return;

        switch (item)
        {
/*
        case TOGGLE:
          if (!mMainMenu.isOpen())
          {
            if (mPlacePage.isDocked() && closePlacePage())
              return;

            if (closeSidePanel())
              return;
          }

          Statistics.INSTANCE.trackEvent(Statistics.EventName.TOOLBAR_MENU);
          AlohaHelper.logClick(AlohaHelper.TOOLBAR_MENU);
          toggleMenu();
          break;


        case ADD_PLACE:
          closePlacePage();
          if (mIsFragmentContainer)
            closeSidePanel();
          closeMenu(Statistics.EventName.MENU_ADD_PLACE, AlohaHelper.MENU_ADD_PLACE, new Runnable()
          {
            @Override
            public void run()
            {
              Statistics.INSTANCE.trackEvent(Statistics.EventName.EDITOR_ADD_CLICK,
                                             Statistics.params().add(Statistics.EventParam.FROM, "main_menu"));
              showPositionChooser(true);
            }
          });
          break;

        case SEARCH:
          RoutingController.get().cancelPlanning();
          closeMenu(Statistics.EventName.TOOLBAR_SEARCH, AlohaHelper.TOOLBAR_SEARCH, new Runnable()
          {
            @Override
            public void run()
            {
              showSearch(mSearchController.getQuery());
            }
          });
          break;

        case P2P:
          startLocationToPoint(Statistics.EventName.MENU_P2P, AlohaHelper.MENU_POINT2POINT, null);
          break;
*/
        case TOUR:
          startTour(Statistics.EventName.MENU_TOUR, AlohaHelper.MENU_TOUR);
          break;
/*
        case BOOKMARKS:
          closeMenu(Statistics.EventName.TOOLBAR_BOOKMARKS, AlohaHelper.TOOLBAR_BOOKMARKS, new Runnable()
          {
            @Override
            public void run()
            {
              showBookmarks();
            }
          });
          break;

        case SHARE:
          closeMenu(Statistics.EventName.MENU_SHARE, AlohaHelper.MENU_SHARE, new Runnable()
          {
            @Override
            public void run()
            {
              shareMyLocation();
            }
          });
          break;

        case DOWNLOADER:
          RoutingController.get().cancelPlanning();
          closeMenu(Statistics.EventName.MENU_DOWNLOADER, AlohaHelper.MENU_DOWNLOADER, new Runnable()
          {
            @Override
            public void run()
            {
              showDownloader(false);
            }
          });
          break;
*/
        case SETTINGS:
          closeMenu(Statistics.EventName.MENU_SETTINGS, AlohaHelper.MENU_SETTINGS, new Runnable()
          {
            @Override
            public void run()
            {
              startActivity(new Intent(getActivity(), SettingsActivity.class));
            }
          });
          break;
/*
        case SHOWCASE:
          closeMenu(Statistics.EventName.MENU_SHOWCASE, AlohaHelper.MENU_SHOWCASE, new Runnable()
          {
            @Override
            public void run()
            {
              mMytargetHelper.displayShowcase();
            }
          });
          break;*/
        }
      }
    });

    if (mIsFragmentContainer)
    {
      mPanelAnimator = new PanelAnimator(this);
      mPanelAnimator.registerListener(mMainMenu.getLeftAnimationTrackListener());
      return;
    }

    /*if (mPlacePage.isDocked())
      mPlacePage.setLeftAnimationTrackListener(mMainMenu.getLeftAnimationTrackListener());*/
  }

  private void initOnmapDownloader()
  {
    mOnmapDownloader = new OnmapDownloader(this);
    if (mIsFragmentContainer)
      mPanelAnimator.registerListener(mOnmapDownloader);
  }

  @Override
  public void onDestroy()
  {
    // TODO move listeners attach-deattach to onStart-onStop since onDestroy isn't guaranteed.
    Framework.nativeRemoveMapObjectListener();
    BottomSheetHelper.free();
    GPS gps = MwmApplication.gps();
    gps.setSimulation(false);
    super.onDestroy();
  }

  private void updateGpsSimulationActive(boolean b) {
    gpsSimulationActive = b;
    if (b){
      mSimulationPauseButton.setImageDrawable(getResources().getDrawable(android.R.drawable.ic_media_pause));
    }else{
      mSimulationPauseButton.setImageDrawable(getResources().getDrawable(android.R.drawable.ic_media_play));
    }
  }

  @Override
  protected void onSaveInstanceState(Bundle outState)
  {
    /*if (mPlacePage.getState() != State.HIDDEN)
    {
      outState.putBoolean(STATE_PP_OPENED, true);
      mPlacePage.saveBookmarkTitle();
      outState.putParcelable(STATE_MAP_OBJECT, mPlacePage.getMapObject());
    }*/
    if (!mIsFragmentContainer && RoutingController.get().isPlanning())
      mRoutingPlanInplaceController.onSaveState(outState);
    RoutingController.get().onSaveState();
    super.onSaveInstanceState(outState);
  }

  @Override
  protected void onRestoreInstanceState(@NonNull Bundle savedInstanceState)
  {
    super.onRestoreInstanceState(savedInstanceState);

    /*if (savedInstanceState.getBoolean(STATE_PP_OPENED))
      mPlacePage.setState(State.PREVIEW);*/

    if (!mIsFragmentContainer && RoutingController.get().isPlanning())
      mRoutingPlanInplaceController.restoreState(savedInstanceState);
  }

  @Override
  protected void onNewIntent(Intent intent)
  {
    super.onNewIntent(intent);
    processIntent(intent);
  }

  private void processIntent(Intent intent)
  {
    if (intent == null)
      return;

    if (intent.hasExtra(EXTRA_TASK))
      addTask(intent);
    else if (intent.hasExtra(EXTRA_UPDATE_COUNTRIES))
      showDownloader(true);
  }

  private void addTask(Intent intent)
  {
    if (intent != null &&
        !intent.getBooleanExtra(EXTRA_CONSUMED, false) &&
        intent.hasExtra(EXTRA_TASK) &&
        ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) == 0))
    {
      final MapTask mapTask = (MapTask) intent.getSerializableExtra(EXTRA_TASK);
      mTasks.add(mapTask);
      intent.removeExtra(EXTRA_TASK);

      if (MapFragment.nativeIsEngineCreated())
        runTasks();

      // mark intent as consumed
      intent.putExtra(EXTRA_CONSUMED, true);
    }
  }

  private void addTask(MapTask task)
  {
    mTasks.add(task);
    if (MapFragment.nativeIsEngineCreated())
      runTasks();
  }

  @Override
  public void onLocationError(int errorCode)
  {
    LocationHelper.nativeOnLocationError(errorCode);

    if (errorCode == LocationHelper.ERROR_DENIED)
    {
      Intent intent = new Intent(android.provider.Settings.ACTION_LOCATION_SOURCE_SETTINGS);
      if (intent.resolveActivity(getPackageManager()) == null)
      {
        intent = new Intent(android.provider.Settings.ACTION_SECURITY_SETTINGS);
        if (intent.resolveActivity(getPackageManager()) == null)
          return;
      }

      final Intent finIntent = intent;
      new AlertDialog.Builder(this)
          .setTitle(R.string.enable_location_service)
          .setMessage(R.string.location_is_disabled_long_text)
          .setPositiveButton(R.string.connection_settings, new DialogInterface.OnClickListener()
          {
            @Override
            public void onClick(DialogInterface dialog, int which)
            {
              startActivity(finIntent);
            }
          })
          .setNegativeButton(R.string.close, null)
          .show();
    }
    else if (errorCode == LocationHelper.ERROR_GPS_OFF)
    {
      Toast.makeText(this, R.string.gps_is_disabled_long_text, Toast.LENGTH_LONG).show();
    }
  }

  @Override
  public void onLocationUpdated(final Location location)
  {
    if (!location.getProvider().equals(LocationHelper.LOCATION_PREDICTOR_PROVIDER))
      mLocationPredictor.reset(location);

    LocationHelper.onLocationUpdated(location);

    /*if (mPlacePage.getState() != State.HIDDEN)
      mPlacePage.refreshLocation(location);*/

    if (!RoutingController.get().isNavigating())
      return;

    RoutingInfo info = Framework.nativeGetRouteFollowingInfo();
    mNavigationController.update(info);
    mMainMenu.updateRoutingInfo(info);

    TtsPlayer.INSTANCE.playTurnNotifications();
  }

  @Override
  public void onCompassUpdated(long time, double magneticNorth, double trueNorth, double accuracy)
  {
    if (mLastCompassData == null)
      mLastCompassData = new LastCompassData();

    mLastCompassData.update(getWindowManager().getDefaultDisplay().getRotation(), magneticNorth, trueNorth);
    MapFragment.nativeCompassUpdated(mLastCompassData.magneticNorth, mLastCompassData.trueNorth, false);

    //mPlacePage.refreshAzimuth(mLastCompassData.north);
    mNavigationController.updateNorth(mLastCompassData.north);
  }

  // Callback from native location state mode element processing.
  @SuppressWarnings("unused")
  public void onMyPositionModeChangedCallback(final int newMode, final boolean routingActive)
  {
    mLocationPredictor.myPositionModeChanged(newMode);
    mMainMenu.getMyPositionButton().update(newMode);

    if (newMode != LocationState.NOT_FOLLOW_NO_POSITION)
      sLocationStopped = false;

    switch (newMode)
    {
    case LocationState.PENDING_POSITION:
      resumeLocation();
      break;

    case LocationState.NOT_FOLLOW_NO_POSITION:
      if (sColdStart)
      {
        LocationState.INSTANCE.switchToNextMode();
        break;
      }

      pauseLocation();

      if (sLocationStopped)
        break;

      sLocationStopped = true;

      if (mMapFragment != null && mMapFragment.isFirstStart())
      {
        mMapFragment.clearFirstStart();
        break;
      }

      String message = String.format("%s\n\n%s", getString(R.string.current_location_unknown_message),
                                                 getString(R.string.current_location_unknown_title));
      new AlertDialog.Builder(this)
                     .setMessage(message)
                     .setNegativeButton(R.string.current_location_unknown_stop_button, null)
                     .setPositiveButton(R.string.current_location_unknown_continue_button, new DialogInterface.OnClickListener()
                     {
                       @Override
                       public void onClick(DialogInterface dialog, int which)
                       {
                         LocationState.INSTANCE.switchToNextMode();
                       }
                     }).show();
      break;

    default:
      LocationHelper.INSTANCE.restart();
      break;
    }

    sColdStart = false;
  }

  @Override
  protected void onResume()
  {
    super.onResume();

    LocationState.INSTANCE.setMyPositionModeListener(this);
    refreshLocationState();

    mSearchController.refreshToolbar();

    mMainMenu.onResume();
    mOnmapDownloader.onResume();

    timerThread.update();
    hideStatusBar();
    findViewById(R.id.map_fragment_container).setVisibility(View.VISIBLE);
  }

  @Override
  public void recreate()
  {
    // Explicitly destroy engine before activity recreation.
    mMapFragment.destroyEngine();
    super.recreate();
  }

  private void initShowcase()
  {
    /*
    NativeAppwallAd.AppwallAdListener listener = new NativeAppwallAd.AppwallAdListener()
    {
      @Override
      public void onLoad(NativeAppwallAd nativeAppwallAd)
      {
        if (nativeAppwallAd.getBanners().isEmpty())
        {
          //mMainMenu.setVisible(MainMenu.Item.SHOWCASE, false);
          return;
        }

        final NativeAppwallBanner menuBanner = nativeAppwallAd.getBanners().get(0);
        mMainMenu.setShowcaseText(menuBanner.getTitle());
        //mMainMenu.setVisible(MainMenu.Item.SHOWCASE, true);
      }

      @Override
      public void onNoAd(String reason, NativeAppwallAd nativeAppwallAd)
      {
        //mMainMenu.setVisible(MainMenu.Item.SHOWCASE, false);
      }

      @Override
      public void onClick(NativeAppwallBanner nativeAppwallBanner, NativeAppwallAd nativeAppwallAd) {}

      @Override
      public void onDismissDialog(NativeAppwallAd nativeAppwallAd) {}
    };
    mMytargetHelper = new MytargetHelper(listener, this);
    */
  }

  @Override
  protected void onResumeFragments()
  {
    super.onResumeFragments();

    if (!RoutingController.get().isNavigating())
    {
      /*
      mFirstStart = FirstStartFragment.showOn(this);
      if (mFirstStart)
      {
        if (LocationState.INSTANCE.isTurnedOn())
          addTask(new MwmActivity.MapTask()
          {
            @Override
            public boolean run(MwmActivity target)
            {
              if (LocationState.INSTANCE.isTurnedOn())
                LocationState.INSTANCE.switchToNextMode();
              return false;
            }
          });
      }
      if (!mFirstStart && !SinglePageNewsFragment.showOn(this))
      {
        if (ViralFragment.shouldDisplay())
          new ViralFragment().show(getSupportFragmentManager(), "");
        else
          LikesManager.INSTANCE.showDialogs(this);
      }
      */
    }

    RoutingController.get().restore();
    //mPlacePage.restore();
  }

  private void adjustZoomButtons()
  {
    final boolean show = showZoomButtons();
    UiUtils.showIf(show, mBtnZoomIn, mBtnZoomOut);

    if (!show)
      return;

    mMapFrame.post(new Runnable()
    {
      @Override
      public void run()
      {
        int height = mMapFrame.getMeasuredHeight();
        /*
        int top = UiUtils.dimen(R.dimen.zoom_buttons_top_required_space);
        int bottom = UiUtils.dimen(R.dimen.zoom_buttons_bottom_max_space);

        int space = (top + bottom < height ? bottom : height - top);

        LinearLayout.LayoutParams lp = (LinearLayout.LayoutParams) mBtnZoomOut.getLayoutParams();
        lp.bottomMargin = space;
        mBtnZoomOut.setLayoutParams(lp);*/
      }
    });
  }

  private static boolean showZoomButtons()
  {
    return true;
    //return RoutingController.get().isNavigating() || Config.showZoomButtons();
  }

  @Override
  protected void onPause()
  {
    LocationState.INSTANCE.removeMyPositionModeListener();
    pauseLocation();
    TtsPlayer.INSTANCE.stop();
    LikesManager.INSTANCE.cancelDialogs();
    mOnmapDownloader.onPause();
    super.onPause();
  }

  private void resumeLocation()
  {
    LocationHelper.INSTANCE.addLocationListener(this, true);
    // Do not turn off the screen while displaying position
    Utils.keepScreenOn(true, getWindow());
    mLocationPredictor.resume();
  }

  private void pauseLocation()
  {
    LocationHelper.INSTANCE.removeLocationListener(this);
    // Enable automatic turning screen off while app is idle
    Utils.keepScreenOn(false, getWindow());
    mLocationPredictor.pause();
  }

  private void refreshLocationState()
  {
    int newMode = LocationState.INSTANCE.getLocationStateMode();
    mMainMenu.getMyPositionButton().update(newMode);

    if (LocationState.INSTANCE.isTurnedOn())
      resumeLocation();
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    initShowcase();
    RoutingController.get().attach(this);
    if (!mIsFragmentContainer)
      mRoutingPlanInplaceController.setStartButton();
    activityIsVisible.set(true);

    timerThread.update();
    hideStatusBar();

    // play an empty sound in the background for BT-connections
    if (mediaPlayer==null){
      mediaPlayer = MediaPlayer.create(this, R.raw.silence);
    }
    mediaPlayer.setLooping(true);
    mediaPlayer.start(); // no need to call prepare(); create() does that for you

    // set this activity as active in routing-controller so that when the tour finishes, the activity can be terminated
    RoutingController.get().setTourFinishedListener(this);
    /*updateGpsSimulationActive(false);
    MwmApplication.gps().setSimulation(false);*/
  }

  public void hideStatusBar() {
    getWindow().getDecorView().setSystemUiVisibility (View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    activityIsVisible.set(false);
    MwmApplication.gps().pauseEmulation();
    //mMytargetHelper.cancel();
    RoutingController.get().detach();
    mediaPlayer.stop();
  }

  @Override
  public void onBackPressed()
  {
    if (mMainMenu.close(true))
    {
      mFadeView.fadeOut(false);
      return;
    }

    if (mSearchController.hide())
    {
      SearchEngine.cancelSearch();
      return;
    }

    if (/*!closePlacePage() &&*/ !closeSidePanel() &&
        !RoutingController.get().cancel() /*&& !closePositionChooser()*/)
    {
      MwmApplication.gps().pauseEmulation();
      super.onBackPressed();
    }
  }

  private boolean interceptBackPress()
  {
    final FragmentManager manager = getSupportFragmentManager();
    for (String tag : DOCKED_FRAGMENTS)
    {
      final Fragment fragment = manager.findFragmentByTag(tag);
      if (fragment != null && fragment.isResumed() && fragment instanceof OnBackPressListener)
        return ((OnBackPressListener) fragment).onBackPressed();
    }

    return false;
  }

  private void removeFragmentImmediate(Fragment fragment)
  {
    FragmentManager fm = getSupportFragmentManager();
    if (fm.isDestroyed())
      return;

    fm.beginTransaction()
      .remove(fragment)
      .commitAllowingStateLoss();
    fm.executePendingTransactions();
  }

  private boolean removeCurrentFragment(boolean animate)
  {
    for (String tag : DOCKED_FRAGMENTS)
      if (removeFragment(tag, animate))
        return true;

    return false;
  }

  private boolean removeFragment(String className, boolean animate)
  {
    if (animate && mPanelAnimator == null)
      animate = false;

    final Fragment fragment = getSupportFragmentManager().findFragmentByTag(className);
    if (fragment == null)
      return false;

    if (animate)
      mPanelAnimator.hide(new Runnable()
      {
        @Override
        public void run()
        {
          removeFragmentImmediate(fragment);
        }
      });
    else
      removeFragmentImmediate(fragment);

    return true;
  }

  @Override
  public void onMapObjectActivated(MapObject object)
  {
    if (MapObject.isOfType(MapObject.API_POINT, object))
    {
      final ParsedMwmRequest request = ParsedMwmRequest.getCurrentRequest();
      if (request == null)
        return;

      request.setPointData(object.getLat(), object.getLon(), object.getTitle(), object.getApiId());
      object.setSubtitle(request.getCallerName(MwmApplication.get()).toString());
    }

    if (
      Framework.nativeIsRoutingActive() &&
      (MapObject.isOfType(MapObject.MY_POSITION, object) || MapObject.isOfType(MapObject.POI, object))
    ) {
      return;
    }

    setFullscreen(false);

    /*mPlacePage.saveBookmarkTitle();
    mPlacePage.setMapObject(object, true);
    mPlacePage.setState(State.PREVIEW);*/

    if (UiUtils.isVisible(mFadeView))
      mFadeView.fadeOut(false);
  }

  @Override
  public void onDismiss(boolean switchFullScreenMode)
  {
    if (switchFullScreenMode)
    {
      if ((mPanelAnimator != null && mPanelAnimator.isVisible()) ||
           UiUtils.isVisible(mSearchController.getToolbar()))
        return;

      setFullscreen(!mIsFullscreen);
    }
    else
    {
      //mPlacePage.hide();
    }
  }

  private void setFullscreen(boolean isFullscreen)
  {
    mIsFullscreen = isFullscreen;
    if (isFullscreen)
    {
      if (mMainMenu.isAnimating())
        return;

      mIsFullscreenAnimating = true;
      Animations.disappearSliding(mMainMenu.getFrame(), Animations.BOTTOM, new Runnable()
      {
        @Override
        public void run()
        {
          final int menuHeight = mMainMenu.getFrame().getHeight();
          adjustCompass(0, menuHeight);
          adjustRuler(0, menuHeight);

          mIsFullscreenAnimating = false;
        }
      });
      if (showZoomButtons())
      {
        Animations.disappearSliding(mBtnZoomOut, Animations.RIGHT, null);
        Animations.disappearSliding(mBtnZoomIn, Animations.RIGHT, null);
      }
    }
    else
    {
      Animations.appearSliding(mMainMenu.getFrame(), Animations.BOTTOM, new Runnable()
      {
        @Override
        public void run()
        {
          adjustCompass(0, 0);
          adjustRuler(0, 0);
        }
      });
      if (showZoomButtons())
      {
        Animations.appearSliding(mBtnZoomOut, Animations.RIGHT, null);
        Animations.appearSliding(mBtnZoomIn, Animations.RIGHT, null);
      }
    }
  }

  /*
  @Override
  public void onPreviewVisibilityChanged(boolean isVisible)
  {
    if (isVisible)
    {
      if (mMainMenu.isAnimating() || mMainMenu.isOpen())
        UiThread.runLater(new Runnable()
        {
          @Override
          public void run()
          {
            if (mMainMenu.close(true))
              mFadeView.fadeOut(false);
          }
        }, MainMenu.ANIMATION_DURATION * 2);
    }
    else
    {
      Framework.nativeDeactivatePopup();
      mPlacePage.saveBookmarkTitle();
      mPlacePage.setMapObject(null, false);
      mMainMenu.show(true);
    }
  }

  @Override
  public void onPlacePageVisibilityChanged(boolean isVisible)
  {
    Statistics.INSTANCE.trackEvent(isVisible ? Statistics.EventName.PP_OPEN
                                             : Statistics.EventName.PP_CLOSE);
    AlohaHelper.logClick(isVisible ? AlohaHelper.PP_OPEN
                                   : AlohaHelper.PP_CLOSE);
  }
  */

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    /*
    case R.id.ll__route:
      mPlacePage.saveBookmarkTitle();
      startLocationToPoint(Statistics.EventName.PP_ROUTE, AlohaHelper.PP_ROUTE, mPlacePage.getMapObject());
      break;
    */
    case R.id.map_button_plus:
      Statistics.INSTANCE.trackEvent(Statistics.EventName.ZOOM_IN);
      AlohaHelper.logClick(AlohaHelper.ZOOM_IN);
      MapFragment.nativeScalePlus();
      break;
    case R.id.map_button_minus:
      Statistics.INSTANCE.trackEvent(Statistics.EventName.ZOOM_OUT);
      AlohaHelper.logClick(AlohaHelper.ZOOM_OUT);
      MapFragment.nativeScaleMinus();
      break;
    case R.id.buttonContinueTourHere:
      Log.d(TAG, "continuing tour at this position");
      Framework.nativeDoContinueTourHere();
      break;
    case R.id.tour_settings:
      startActivity(new Intent(getActivity(), SettingsActivity.class)) ;
    }
  }

  @Override
  public boolean onTouch(View view, MotionEvent event)
  {
    return /*mPlacePage.hideOnTouch() ||*/
           mMapFragment.onTouch(view, event);
  }

  @Override
  public void customOnNavigateUp()
  {
    if (removeCurrentFragment(true))
    {
      InputUtils.hideKeyboard(mMainMenu.getFrame());
      mSearchController.refreshToolbar();
    }
  }

  public interface MapTask extends Serializable
  {
    boolean run(MwmActivity target);
  }

  public static class OpenUrlTask implements MapTask
  {
    private static final long serialVersionUID = 1L;
    private final String mUrl;

    public OpenUrlTask(String url)
    {
      Utils.checkNotNull(url);
      mUrl = url;
    }

    @Override
    public boolean run(MwmActivity target)
    {
      return MapFragment.nativeShowMapForUrl(mUrl);
    }
  }

  public static class ShowCountryTask implements MapTask
  {
    private static final long serialVersionUID = 1L;
    private final String mCountryId;
    private final boolean mDoAutoDownload;

    public ShowCountryTask(String countryId, boolean doAutoDownload)
    {
      mCountryId = countryId;
      mDoAutoDownload = doAutoDownload;
    }

    @Override
    public boolean run(MwmActivity target)
    {
      if (mDoAutoDownload)
        MapManager.warn3gAndDownload(target, mCountryId, null);

      Framework.nativeShowCountry(mCountryId, mDoAutoDownload);
      return true;
    }
  }
/*
  public static class ShowAuthorizationTask implements MapTask
  {
    @Override
    public boolean run(MwmActivity target)
    {
      final DialogFragment fragment = (DialogFragment) Fragment.instantiate(target, AuthDialogFragment.class.getName());
      fragment.show(target.getSupportFragmentManager(), AuthDialogFragment.class.getName());
      return true;
    }
  }
*/
  public void adjustCompass(int offsetX, int offsetY)
  {
    if (mMapFragment == null || !mMapFragment.isAdded())
      return;

    mMapFragment.setupCompass((mPanelAnimator != null && mPanelAnimator.isVisible()) ? offsetX : 0, offsetY, true);

    if (mLastCompassData != null)
      MapFragment.nativeCompassUpdated(mLastCompassData.magneticNorth, mLastCompassData.trueNorth, true);
  }

  public void adjustRuler(int offsetX, int offsetY)
  {
    if (mMapFragment == null || !mMapFragment.isAdded())
      return;

    mMapFragment.setupRuler(offsetX, offsetY, true);
  }
/*
  @Override
  public void onCategoryChanged(int bookmarkId, int newCategoryId)
  {
    // TODO remove that hack after bookmarks will be refactored completely
    Framework.nativeOnBookmarkCategoryChanged(newCategoryId, bookmarkId);
    mPlacePage.setMapObject(BookmarkManager.INSTANCE.getBookmark(newCategoryId, bookmarkId), true);
  }
*/
  @Override
  public FragmentActivity getActivity()
  {
    return this;
  }

  public MainMenu getMainMenu()
  {
    return mMainMenu;
  }

  @Override
  public void showSearch()
  {
    showSearch("");
  }

  @Override
  public void updateMenu()
  {
    if (RoutingController.get().isNavigating())
    {
      mMainMenu.setState(MainMenu.State.NAVIGATION, false);
      return;
    }

    if (mIsFragmentContainer)
    {
      //mMainMenu.setEnabled(MainMenu.Item.P2P, !RoutingController.get().isPlanning());
      mMainMenu.setEnabled(MainMenu.Item.TOUR, !RoutingController.get().isPlanning());
      //mMainMenu.setEnabled(MainMenu.Item.SEARCH, !RoutingController.get().isWaitingPoiPick());
    }
    else if (RoutingController.get().isPlanning())
    {
      mMainMenu.setState(MainMenu.State.ROUTE_PREPARE, false);
      return;
    }

    mMainMenu.setState(MainMenu.State.MENU, false);
  }

  @Override
  public void showRoutePlan(boolean show, @Nullable Runnable completionListener)
  {
    if (show)
    {
      mSearchController.hide();

      if (mIsFragmentContainer)
      {
        replaceFragment(RoutingPlanFragment.class, null, completionListener);
      }
      else
      {
        mRoutingPlanInplaceController.show(true);
        if (completionListener != null)
          completionListener.run();
      }
    }
    else
    {
      if (mIsFragmentContainer)
        closeSidePanel();
      else
        mRoutingPlanInplaceController.show(false);

      if (completionListener != null)
        completionListener.run();
    }

    //mPlacePage.refreshViews();
  }

  @Override
  public void showNavigation(boolean show)
  {
    adjustZoomButtons();
    //mPlacePage.refreshViews();
    mNavigationController.show(show);
    mOnmapDownloader.updateState(false);
  }

  @Override
  public void updatePoints()
  {
    if (mIsFragmentContainer)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
        fragment.updatePoints();
    }
    else
    {
      mRoutingPlanInplaceController.updatePoints();
    }
  }

  @Override
  public void updateBuildProgress(int progress, int router)
  {
    if (mIsFragmentContainer)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
        fragment.updateBuildProgress(progress, router);
    }
    else
    {
      mRoutingPlanInplaceController.updateBuildProgress(progress, router);
    }
  }

  boolean isFirstStart()
  {
    boolean res = mFirstStart;
    mFirstStart = false;
    return res;
  }

  public TextView getTextMissionTime() {
    try{
      return (TextView) findViewById(R.id.textMissionTime);
    }catch(NullPointerException ignored){
      return null;
    }
  }
  public View getRowMissionTime() {
    try{
      return findViewById(R.id.rowMissionTime);
    }catch(NullPointerException ignored){
      return null;
    }
  }

  @Override
  public void onPossibleTourResumption(final boolean isPossible) {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        mButtonContinueTourHere.setVisibility(isPossible ? View.VISIBLE : View.GONE);
      }
    });
  }

  @Override
  public void onPoiVisited(final String message) {
    runOnUiThread(new Runnable() {public void run() {
      // show dialog if not yet visible otherwise save to list of poi-messages
      synchronized (poiMessages){
        if (poiDialog.isShowing() || poiMessages.size()>0){
          poiMessages.add(message);
        }else{
          showPoiDialogNow(message);
        }
      }
    }});
  }

  private void showPoiDialogNow(final String message) {
    synchronized (poiMessages){
      poiDialog.setMessage(message);
      poiDialog.show();
      new Thread(){public void run(){
        MwmApplication.get().playNotificationSound();
        try { Thread.sleep(1000); } catch (InterruptedException ignored) {}
        TtsPlayer.INSTANCE.playCustomMessage(message);
      }}.start();
    }
  }

  @Override
  public void onMissionStatusChanged(@NonNull MissionStatus missionStatus) {
    updateButtons();
    updateActivityDisplay(missionStatus);
    timerThread.update();
  }

  private void updateActivityDisplay(final MissionStatus missionStatus) {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (mRowMissionActivity==null || mTextMissionActivity==null) return;
        com.mobidat.persistence.Activity act = missionStatus.getActivity();
        if (act!=null){
          mTextMissionActivity.setText(act.getName());
          mRowMissionActivity.setVisibility(View.VISIBLE);
        }else{
          mRowMissionActivity.setVisibility(View.GONE);
        }
      }
    });
  }

  private void updateButtons() {
    MissionStatus ms = MissionAccess.missionStatus;
    if (ms==null){
      mBreakButton.setVisibility(View.GONE);
      return;
    }else{
      mBreakButton.setVisibility(View.VISIBLE);
    }
    if (ms.getActivity()==null){
      mBreakButton.setImageDrawable(getResources().getDrawable(android.R.drawable.ic_media_pause));
    }else{
      mBreakButton.setImageDrawable(getResources().getDrawable(android.R.drawable.ic_media_play));
    }
  }

  public void breakButtonClicked(View button) {
    MissionStatus ms = MissionAccess.missionStatus;
    if (ms!=null){
      if (ms.getActivity()==null){
        try{
          startActivity(new Intent("com.mobidat.wp2.missionrecording.BREAK"));
        }catch(ActivityNotFoundException _){
          Toast.makeText(this, R.string.activityNotFound_missionRecording,Toast.LENGTH_LONG);
        }
      }else{
        MissionAccess.missionService.setActivity(null);
      }
    }
  }

  public void legendButtonClicked(View button){
    mLegend.setVisibility( mLegend.getVisibility()==View.GONE?View.VISIBLE:View.GONE );
  }

  public void gpsPause(View ignored){
    if (gpsSimulationActive){
      MwmApplication.gps().pauseEmulation();
    }else{
      MwmApplication.gps().setEmulation(true);
    }
  }
  public void gpsStop(View ignored){
    MwmApplication.gps().setSimulation(false);
  }

  public void gpsRestart(View ignored) {
    String baseDir = Parameter.getGlobalDirectory();
    final LinkedList<File> simuFiles = new LinkedList<>();
    final LinkedList<String> simuNames = new LinkedList<>();
    File simuLog = new File(baseDir + "/simulation.log");
    if (simuLog.exists()) {
      simuFiles.add(simuLog);
      simuNames.add("simulation.log");
    }
    final String tourPrefix = "Tour: ";
    DbHelper.block(new Runnable() {
      @Override
      public void run() {
        for (Tour t : DaoTour.getInstance().getAll()) {
          simuFiles.add(new File(t.getFile()));
          simuNames.add(tourPrefix+t.getName());
        }
      }
    });
    final String[] simulationNames = simuNames.toArray(new String[simuNames.size()]);
    new AlertDialog.Builder(this)
      .setTitle("Simulationsdaten wählen")
      .setItems(simulationNames, new DialogInterface.OnClickListener() {
        @Override
        public void onClick(DialogInterface dialog, int which) {
          if (simuNames.get(which).startsWith(tourPrefix)){
            MwmApplication.gps().useSimulationTour(simuFiles.get(which).getAbsolutePath());
          }else{
            MwmApplication.gps().useSimulationLog();
          }
          MwmApplication.gps().restartEmulation();
          dialog.dismiss();
        }
      }).setOnDismissListener(new DialogInterface.OnDismissListener() {
        @Override
        public void onDismiss(DialogInterface dialog) {
          MwmActivity.this.hideStatusBar();
        }
      })
      .show();
  }

}
