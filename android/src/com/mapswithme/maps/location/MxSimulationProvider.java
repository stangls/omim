package com.mapswithme.maps.location;


import android.content.Context;
import android.location.Location;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.format.DateFormat;
import android.util.Log;

import com.mapswithme.maps.MwmActivity;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.util.LocationUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.LinkedList;
import java.util.Locale;

public class MxSimulationProvider extends BaseLocationProvider
{

  private boolean mIsActive;
  public static String providerName="MOBIWORX position provider for simulations";

  private int currentSimulationStep;
  private final int numSimulationSteps;
  private final Location[] simulationData;
  private final int[] simulationDataWaitTimeMS;
  private Thread thread;
  private Handler mHandler;
  private int minWaitTime = 250;

  public MxSimulationProvider(File file) throws IOException {
    this(file,3);
  }
  public MxSimulationProvider(File file, int speedup) throws IOException {

    // get a handler for asynchronous thread
    mHandler=new Handler();

    // read the simulation file
    LinkedList<Location> data = new LinkedList<Location>();
    FileInputStream is = new FileInputStream(file);
    BufferedReader reader = new BufferedReader(new InputStreamReader(is));
    final SimpleDateFormat df = new SimpleDateFormat("dd.MM.yyyy HH:mm:ss");
    try {
      String line;
      while ((line = reader.readLine()) != null) {
        String[] rowData = line.split("\\s+");
        String date = rowData[0];
        String time = rowData[1];
        String longitude = rowData[2];
        String latitude = rowData[3];
        String tasks = rowData[4];
        Log.v( "MxSimulationProvider", "Read line: " + date + " " + time + " " + longitude + " " + latitude + " " + tasks );
        try {
          Date datetime = df.parse(date + " " + time, new ParsePosition(0));
          Log.v("MxSimulationProvider", "Parsed date-time: " + datetime);
          Location l = new Location(this.providerName);
          l.setLongitude(Double.parseDouble(longitude));
          l.setLatitude(Double.parseDouble(latitude));
          l.setAccuracy(10); // meters with 68% confidence
          l.setTime(datetime.getTime());
          data.addLast(l);
        }catch(IllegalArgumentException e){
          Log.e("MxSimulationProvider","Illegal input: '"+line+"' (skipping)");
          e.printStackTrace();
        }
      }
    } finally { is.close(); }

    // create the simulation from data
    if (data.size()>0){
      numSimulationSteps=data.size();
      simulationData = new Location[numSimulationSteps];
      simulationDataWaitTimeMS = new int[numSimulationSteps];
      long lastTime = 0;
      for (int i = 0; i < numSimulationSteps; i++) {
        Location l = data.poll();
        simulationData[i]=l;
        long time = l.getTime();
        try {
          simulationDataWaitTimeMS[i - 1] = (int) (time - lastTime)/speedup;
        }catch(ArrayIndexOutOfBoundsException _){}
        lastTime = time;
      }
      // wait 3 seconds before restarting the simulation
      simulationDataWaitTimeMS[numSimulationSteps-1]=3000;
    }else {
      // if simulation data is empty we create it ...
      numSimulationSteps = 60;
      simulationData = new Location[numSimulationSteps];
      simulationDataWaitTimeMS = new int[numSimulationSteps];

      final double radius = 0.002000;
      for (int i = 0; i < numSimulationSteps; i++) {
        Location l = new Location(this.providerName);
        double angle = Math.PI * 2 * i / numSimulationSteps;
        l.setLongitude(12.115726 + radius * Math.cos(angle));
        l.setLatitude(47.790526 + radius * Math.sin(angle));
        l.setAccuracy((float) Math.round(2 * Math.cos(angle * 3) + 2)); // meters with 68% confidence
        simulationData[i] = l;
        simulationDataWaitTimeMS[i] = 500; // 0.5s
      }
    }
  }

  private Location retrieveCurrentLocation() {
    Location l = simulationData[currentSimulationStep];
    l.setTime(System.currentTimeMillis());
    return l;
  }

  @Override
  protected void startUpdates()
  {
    if (mIsActive)
      return;

    currentSimulationStep = 0;
    thread = new Thread(new Runnable() {
      public void run() {
        while (mIsActive) {
          final Location l = retrieveCurrentLocation();
          // ensure we run on the original thread to avoid synchronization issues and CalledFromWrongThreadException
          mHandler.sendMessage( Message.obtain( mHandler, new Runnable() {
            @Override
            public void run() {
              LocationHelper.INSTANCE.initMagneticField(l); // maydo
              LocationHelper.INSTANCE.setLastLocation(l);
            }
          }));
          int time = simulationDataWaitTimeMS[currentSimulationStep];
          while (time<minWaitTime) {
            Log.d("MxSimulationProvider","skipping a location");
            if (++currentSimulationStep >= numSimulationSteps) {
              currentSimulationStep = 0;
            }
            time += simulationDataWaitTimeMS[currentSimulationStep];
          }
          synchronized (thread) {
            try {
              thread.wait(time);
            } catch (InterruptedException e) { }
          }
          if (++currentSimulationStep >= numSimulationSteps) {
            currentSimulationStep = 0;
          }
        }
      }
    });
    mIsActive = true;
    thread.start();

    final Location newLocation = retrieveCurrentLocation();
    if (isLocationBetterThanLast(newLocation))
      LocationHelper.INSTANCE.setLastLocation(newLocation);
    else
    {
      // strange. why is this neccessary? (copied from AndroidNativeProvider)
      final Location lastLocation = LocationHelper.INSTANCE.getLastLocation();
      if (lastLocation != null && !LocationUtils.isExpired(lastLocation, LocationHelper.INSTANCE.getLastLocationTime(),
                                                           LocationUtils.LOCATION_EXPIRATION_TIME_MILLIS_SHORT))
        LocationHelper.INSTANCE.setLastLocation(lastLocation);
    }
  }

  @Override
  protected void stopUpdates()
  {
    if (!mIsActive)
      return;

    mIsActive = false;
    synchronized (thread) {
      thread.notifyAll();
    }
  }

}