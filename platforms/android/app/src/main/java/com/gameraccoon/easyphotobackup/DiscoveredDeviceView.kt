package com.gameraccoon.easyphotobackup

import android.annotation.SuppressLint
import android.content.Context
import android.util.AttributeSet
import android.widget.TextView
import uniffi.client_ffi.DiscoveredService

class DiscoveredDeviceView
@JvmOverloads
constructor(context: Context, attrs: AttributeSet? = null) :
    androidx.appcompat.widget.LinearLayoutCompat(context, attrs) {
  var service: DiscoveredService? = null

  init {
    inflate(context, R.layout.discovered_device, this)
  }

  @SuppressLint("SetTextI18n")
  fun updateOnline(online: Boolean) {
    val onlineText = findViewById<TextView>(R.id.presense_text)
    val address = findViewById<TextView>(R.id.device_address)
    val deviceName = findViewById<TextView>(R.id.device_name)
    if (online && service != null) {
      onlineText.text = context.getString(R.string.status_online)
      var service = service!!
      address.text = "${service.getIp()}:${service.getPort()}"
      deviceName.text = service.getId().toString()
      isEnabled = true
    } else {
      onlineText.text = context.getString(R.string.status_offline)
      isEnabled = false
    }
  }
}
