package com.mobidat.wp2.missionrecording

import android.app.Activity
import android.app.Application
import android.content.Context
import android.os.Bundle
import com.mapswithme.maps.MwmActivity

import com.mobidat.wp2.missionservice.MissionListener
import com.mobidat.wp2.missionservice.MissionService
import com.mobidat.wp2.missionservice.MissionStatus
import com.pawegio.kandroid.*


/**
 * Created by sd on 06.04.16.
 */
class MissionAccess : MissionListener {

    override fun onMissionStatusChanged(state: MissionStatus) {
        missionStatus = state
        d("onMissionStatusChanged: active="+state.isActive()+" profile="+state.getProfile());
        listeningActivity?.onMissionStatusChanged(state)
    }

    companion object {
        private val instance = MissionAccess()
        @JvmField var listeningActivity: MissionListener? = null

        lateinit var missionService : MissionService
        @JvmField var missionStatus: MissionStatus? = null

        @JvmStatic public fun init(ctx: Context) {
            missionStatus = null
            missionService = MissionService(ctx)
            missionService.setMissionListener(instance)
        }
    }
}
