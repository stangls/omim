package com.mobidat.wp2.missionrecording

import android.view.View
import com.mapswithme.maps.MwmActivity
/*
import com.mobidat.wp2.missionservice.MissionListener
import com.mobidat.wp2.missionservice.MissionService
import com.mobidat.wp2.missionservice.MissionStatus
*/

/**
 * Created by sd on 12.04.16.
 */
internal class TimerThread(private val activity: MwmActivity) : Thread()/*, MissionListener*/ {
/*
    private val missionService = MissionService(activity)
    private var missionStatus: MissionStatus? = null;

    override fun onMissionStatusChanged(state: MissionStatus) {
        missionStatus = state
    }*/

    override fun run() {
        //missionService.setMissionListener(this)

        while (true) {
            while (activity.activityIsVisible.get()) {
                updateTime()
                synchronized (this) {
                    try {
                        (this as Object).wait(500)
                    } catch (_: InterruptedException) {}
                }
            }
            // wait for activity to become visible
            synchronized (this) {
                try {
                    (this as Object).wait()
                } catch (_: InterruptedException) {}
            }
        }
    }

    private fun updateTime() {/*
        val misState = missionStatus
        if (misState==null){
            return;
        }
        // calculate time
        val recordDuration =
            if (missionStatus != null && misState.isActive) {
                System.currentTimeMillis() - misState.startTime!!
            }else null
        // show time
        activity.runOnUiThread {
            try {
                if (recordDuration == null) {
                    activity.getTextMissionTime()?.visibility= View.GONE
                } else {
                    var seconds = recordDuration / 1000
                    var minutes = seconds / 60
                    seconds -= minutes * 60
                    val hours = minutes / 60
                    minutes -= hours * 60
                    var text : String = hours.toString()
                    while (text.length < 2) text = "0" + text
                    text += " : "+minutes.toString()
                    while (text.length < 2) text = "0" + text
                    text += " : "+seconds.toString()
                    while (text.length < 2) text = "0" + text
                    activity.getTextMissionTime()?.setText(text)
                    activity.getTextMissionTime()?.visibility= View.VISIBLE
                }
            } catch (_: NullPointerException) {
            }
        }*/
    }

    @Synchronized fun update() {
        (this as Object).notify()
    }
}
