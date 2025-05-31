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

  init {
    inflate(context, R.layout.discovered_device, this)
  }

  fun getService(): DiscoveredService? {
    return service
  }

  @OptIn(DelicateCoroutinesApi::class)
  fun setService(service: DiscoveredService, activity: Activity) {
    this.service = service

    // request device name
    if (service.getName().isEmpty()) {
      GlobalScope.launch {
        val deviceName = service.fetchNameSync()
        if (deviceName != null) {
          service.setName(deviceName)
        }
        activity.runOnUiThread {
          deviceName?.let { deviceName ->
            val deviceNameField = findViewById<TextView>(R.id.device_name)
            deviceNameField.text = deviceName
          }
        }
      }
    } else {
      val deviceName = findViewById<TextView>(R.id.device_name)
      deviceName.text = service.getName()
    }
  }

  @SuppressLint("SetTextI18n")
  fun updatePort(port: UShort) {
    findViewById<TextView>(R.id.device_address).text = "${service?.getIp()}:$port"
  }

  @SuppressLint("SetTextI18n")
  fun updateOnline(online: Boolean) {
    val onlineText = findViewById<TextView>(R.id.presense_text)
    val address = findViewById<TextView>(R.id.device_address)
    if (online && service != null) {
      onlineText.text = context.getString(R.string.status_online)
      var service = service!!
      address.text = "${service.getIp()}:${service.getPort()}"
      isEnabled = true
    } else {
      onlineText.text = context.getString(R.string.status_offline)
      isEnabled = false
    }
  }
}
