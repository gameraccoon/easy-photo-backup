package com.gameraccoon.easyphotobackup

import android.content.Context
import android.util.AttributeSet
import android.widget.TextView
import androidx.appcompat.widget.LinearLayoutCompat
import uniffi.client_ffi.ServerInfo

class PairedDeviceView @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null) :
    LinearLayoutCompat(context, attrs) {
  private var serverInfo: ServerInfo? = null

  init {
    inflate(context, R.layout.paired_device, this)
  }

  fun setServerInfo(serverInfo: ServerInfo) {
    this.serverInfo = serverInfo
    val deviceName = findViewById<TextView>(R.id.paired_device_name)
    deviceName.text = serverInfo.getName()
    val lastSyncTime = findViewById<TextView>(R.id.last_sync_time)
    lastSyncTime.text = "IDK"
  }
}
