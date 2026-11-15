package com.unnamed.easyphotobackup

import android.content.Context
import android.util.AttributeSet
import android.widget.TextView
import androidx.appcompat.widget.LinearLayoutCompat

class DiscoveredDeviceView @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null) :
    LinearLayoutCompat(context, attrs) {
  private var serverInfo: ServerConnectionInfo? = null
  private var isPaired = false

  init {
    inflate(context, R.layout.discovered_device, this)
  }

  fun getServerInfo(): ServerConnectionInfo? {
    return serverInfo
  }

  fun setServerInfo(serverInfo: ServerConnectionInfo, clientConfigStorage: ClientConfigStorage) {
    if (this.serverInfo !== serverInfo) {
      this.serverInfo?.close()
      this.serverInfo = serverInfo
    }
    isPaired = PairingHelpers.isServerPaired(clientConfigStorage, serverInfo)

    val serverName = if (isPaired) {
      PairingHelpers.getServerName(clientConfigStorage, serverInfo)
          ?: context.getString(R.string.unknown_server)
    } else {
      context.getString(R.string.unknown_server)
    }

    setDeviceName(serverName)
    setAddress(serverInfo.getAddressString())
  }

  private fun setDeviceName(name: String) {
    val deviceName = findViewById<TextView>(R.id.device_name)
    if (isPaired) {
      deviceName.text = context.getString(R.string.server_status_paired).format(name)
    } else {
      deviceName.text = name
    }
  }

  private fun setAddress(address: String) {
    findViewById<TextView>(R.id.device_address).text = address
  }

  fun updateOnline(online: Boolean) {
    val onlineText = findViewById<TextView>(R.id.presence_text)
    if (online) {
      onlineText.text = context.getString(R.string.server_status_online)
      isEnabled = !isPaired
    } else {
      onlineText.text = context.getString(R.string.server_status_offline)
      isEnabled = false
    }
  }
}
