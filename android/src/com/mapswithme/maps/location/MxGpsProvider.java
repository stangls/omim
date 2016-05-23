package com.mapswithme.maps.location;

import android.app.Application;
import android.location.Location;
import android.os.Handler;
import android.os.Message;
import android.widget.Toast;

import com.mapswithme.maps.MwmActivity;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.util.LocationUtils;
import com.mobidat.wp2.gpsProvider.GPS;
import com.mobidat.wp2.gpsProvider.GPSInfo;
import com.mobidat.wp2.gpsProvider.ILocationReceiver;

/**
 * Created by sd on 08.03.16.
 */
public class MxGpsProvider extends BaseLocationProvider implements ILocationReceiver {

    private final Handler mHandler;
    private GPS gps = MwmApplication.gps();

    public MxGpsProvider(){
        // get a handler for asynchronous thread
        mHandler=new Handler();
    }

    private boolean initGps() {
        if (gps==null) {
            MwmApplication ctx = MwmApplication.get();
            if (ctx!=null) {
                gps = new GPS(ctx, this);
            }
        }
        return (gps!=null);
    }

    @Override
    protected void startUpdates() {
        if (initGps()) {
            gps.start();
        }
    }

    @Override
    protected void stopUpdates() {
        if (gps!=null) {
            gps.stop();
        }
    }

    @Override
    public void onStatusChanged(GPSInfo info) {
        //Toast.makeText(MwmApplication.get(), "GPS-Provider-Service: "+info.getMessage(), Toast.LENGTH_LONG).show();
    }

    @Override
    public void onLocationChanged(final Location location) {
        if (location==null) return;
        // ensure we run on the original thread to avoid synchronization issues and CalledFromWrongThreadException
        mHandler.post(new Runnable() {public void run() {
                LocationHelper.INSTANCE.initMagneticField(location); // maydo
                LocationHelper.INSTANCE.saveLocation(location);
        }});/*
        if (isLocationBetterThanLast(location))
            LocationHelper.INSTANCE.setLastLocation(location);
        else
        {
            // strange. why is this neccessary? (copied from AndroidNativeProvider)
            final Location lastLocation = LocationHelper.INSTANCE.getLastLocation();
            if (lastLocation != null && !LocationUtils.isExpired(lastLocation, LocationHelper.INSTANCE.getLastLocationTime(),
                    LocationUtils.LOCATION_EXPIRATION_TIME_MILLIS_SHORT))
                LocationHelper.INSTANCE.setLastLocation(lastLocation);
        }*/
    }
}
