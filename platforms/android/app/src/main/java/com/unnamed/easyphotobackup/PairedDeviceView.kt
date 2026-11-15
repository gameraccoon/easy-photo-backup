package com.unnamed.easyphotobackup

import android.content.Context
import android.util.AttributeSet
import android.widget.TextView
import androidx.appcompat.widget.LinearLayoutCompat

class PairedDeviceView @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null) :
    LinearLayoutCompat(context, attrs) {
  private var serverInfo: ServerConnectionInfo? = null

  init {
    inflate(context, R.layout.paired_device, this)
  }

  fun getServerInfo(): ServerConnectionInfo? {
    return serverInfo
  }

  fun setServerInfo(serverInfo: ServerConnectionInfo, serverName: String) {
    this.serverInfo = serverInfo
    val deviceName = findViewById<TextView>(R.id.paired_device_name)
    deviceName.text = serverName
    val lastSyncTime = findViewById<TextView>(R.id.last_sync_time)
    lastSyncTime.text = context.getString(R.string.tap_to_unpair)
  }
}
