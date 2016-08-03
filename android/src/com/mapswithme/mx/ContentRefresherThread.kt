package com.mapswithme.mx

import android.app.Activity
import android.app.Application.ActivityLifecycleCallbacks
import android.content.Context
import android.content.ContextWrapper
import android.os.Bundle
import com.mapswithme.maps.Framework
import com.mapswithme.maps.MwmActivity

import com.mapswithme.maps.MwmApplication
import com.mobidat.wp2.configuration.Parameter
import com.pawegio.kandroid.d
import com.pawegio.kandroid.i
import java.io.File

/**
 * Created by sd on 29.07.16.
 */
class ContentRefresherThread(context: Context) : Thread() {

    init{
        name = ContentRefresherThread::class.java.simpleName
        MwmApplication.get().registerActivityLifecycleCallbacks(object : ActivityLifecycleCallbacks {
            override fun onActivityPaused(activity: Activity?) {}
            override fun onActivityDestroyed(activity: Activity?) {}
            override fun onActivitySaveInstanceState(activity: Activity?, outState: Bundle?) {}
            override fun onActivityCreated(activity: Activity?, savedInstanceState: Bundle?) {}
            override fun onActivityResumed(activity: Activity?) { if (running()){ n(); } }
            override fun onActivityStarted(activity: Activity?) { if (running()){ n(); } }
            override fun onActivityStopped(activity: Activity?) { }
        })
    }

    private val updateCheckIntervalMS = 5000L

    private var geometriesLastUpdated: Long = 0
    private val geometriesPath = Parameter.getGlobalDirectory() + "/geometries.xml"


    override fun run() {
        while(true){
            // refresh geometries
            val f = File(geometriesPath);
            if (f.exists()){
                if (f.lastModified()>geometriesLastUpdated){
                    i("Updating geometries.");
                    geometriesLastUpdated = f.lastModified()
                    Framework.nativeLoadGeomsXml(f.absolutePath);
                }
            }else{
                geometriesLastUpdated = 0;
                Framework.nativeLoadGeomsXml("");
            }
            sleep(updateCheckIntervalMS)
            if (!running()){ w(); }
        }
    }

    private fun running(): Boolean = MwmActivity.getInstance()?.activityIsVisible?.get() ?: false

    private fun w() {
        synchronized(this){
            (this as Object).wait()
        }
    }
    private fun n() {
        synchronized(this){ (this as Object).notifyAll() }
    }
}
