package com.gameraccoon.easyphotobackup

import android.annotation.SuppressLint
import android.content.Context
import android.util.AttributeSet
import android.widget.TextView

class DiscoveredDeviceView
@JvmOverloads
constructor(context: Context, attrs: AttributeSet? = null) :
    androidx.appcompat.widget.LinearLayoutCompat(context, attrs) {
  var ip: String = ""
  var port: Int = 0
  var id: ByteArray = ByteArray(0)

  init {
    inflate(context, R.layout.discovered_device, this)
  }

  @SuppressLint("SetTextI18n")
  fun updateOnline(online: Boolean) {
    val onlineText = findViewById<TextView>(R.id.online_text)
    val text = findViewById<TextView>(R.id.discovered_device_text)
    if (online) {
      onlineText.text = context.getString(R.string.status_online)
      text.text = "$ip:$port"
      isEnabled = true
    } else {
      onlineText.text = context.getString(R.string.status_offline)
      isEnabled = false
    }
  }
}
