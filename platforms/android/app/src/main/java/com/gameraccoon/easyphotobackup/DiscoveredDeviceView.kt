package com.gameraccoon.easyphotobackup

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.util.AttributeSet
import android.widget.TextView
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import uniffi.client_ffi.DiscoveredService

class DiscoveredDeviceView
@JvmOverloads
constructor(context: Context, attrs: AttributeSet? = null) :
    androidx.appcompat.widget.LinearLayoutCompat(context, attrs) {
  private var service: DiscoveredService? = null
  private var isPaired = false

  init {
    inflate(context, R.layout.discovered_device, this)
  }

  fun getService(): DiscoveredService? {
    return service
  }

  @OptIn(DelicateCoroutinesApi::class)
  fun setService(service: DiscoveredService, activity: Activity) {
    this.service = service

    val easyPhotoBackupApplication = activity.application as EasyPhotoBackupApplication
    var clientStorage = easyPhotoBackupApplication.getClientStorage()
    if (clientStorage != null) {
      isPaired = clientStorage.isDevicePaired(service.getId())
    }

    // request device name
    val serviceName = service.getName()
    if (serviceName.isEmpty()) {
      GlobalScope.launch {
        val deviceName = service.fetchNameSync()
        activity.runOnUiThread { deviceName?.let { deviceName -> setDeviceName(deviceName) } }
      }
    } else {
      setDeviceName(serviceName)
    }
  }

  private fun setDeviceName(name: String) {
    val deviceName = findViewById<TextView>(R.id.device_name)
    if (isPaired) {
      deviceName.text = context.getString(R.string.status_paired).format(name)
    } else {
      deviceName.text = name
    }
  }

  @SuppressLint("SetTextI18n")
  fun setPort(port: UShort) {
    findViewById<TextView>(R.id.device_address).text = "${service?.getIp()}:$port"
  }

  @SuppressLint("SetTextI18n")
  fun updateOnline(online: Boolean) {
    val onlineText = findViewById<TextView>(R.id.presense_text)
    if (online) {
      onlineText.text = context.getString(R.string.status_online)
      isEnabled = !isPaired
    } else {
      onlineText.text = context.getString(R.string.status_offline)
      isEnabled = false
    }
  }
}
