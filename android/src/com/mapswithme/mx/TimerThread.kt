package com.mobidat.wp2.missionrecording

import android.view.View
import com.mapswithme.maps.MwmActivity
import com.mapswithme.maps.MwmApplication

import com.mobidat.wp2.missionservice.MissionListener
import com.mobidat.wp2.missionservice.MissionService
import com.mobidat.wp2.missionservice.MissionStatus


/**
 * Created by sd on 12.04.16.
 */
internal class TimerThread(private val activity: MwmActivity) : Thread() {

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

    private fun updateTime() {
        val missionStatus = MissionAccess.missionStatus
        if (missionStatus==null){
            return;
        }
        // calculate time
        val recordDuration =
            if (missionStatus != null && missionStatus.isActive) {
                System.currentTimeMillis() - missionStatus.startTime!!
            }else null
        // show time
        activity.runOnUiThread {
            try {
                val textMissionTime = activity.getTextMissionTime()
                val rowMissionTime = activity.getRowMissionTime()
                if (recordDuration == null) {
                    textMissionTime?.setText("???")
                } else {
                    var seconds = recordDuration / 1000
                    var minutes = seconds / 60
                    seconds -= minutes * 60
                    val hours = minutes / 60
                    minutes -= hours * 60
                    var finalText = hours.toString()
                    while (finalText.length < 2) finalText = "0" + finalText
                    var text = minutes.toString()
                    while (text.length < 2) text = "0" + text
                    finalText += ":"+text
                    text = seconds.toString()
                    while (text.length < 2) text = "0" + text
                    finalText += ":"+text
                    textMissionTime?.setText(finalText)
                    rowMissionTime?.visibility= View.VISIBLE
                }
            } catch (_: NullPointerException) {}
        }
    }

    @Synchronized fun update() {
        (this as Object).notify()
    }
}
