package com.unnamed.easyphotobackup

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlin.time.Duration.Companion.milliseconds

class DiscoverDevicesActivity : AppCompatActivity() {
  private val nsdClient = SimpleNsdClientNative()
  private lateinit var clientConfigStorage: ClientConfigStorage
  private val discoveredDevices = mutableMapOf<String, DiscoveredDeviceView>()

  private var discoveryJob: Job? = null
  private var discoveryRunning = false

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    clientConfigStorage = ClientConfigStorage(filesDir.absolutePath)
    enableEdgeToEdge()
    setContentView(R.layout.activity_discover_devices)
    ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
      val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
      v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
      insets
    }
  }

  override fun onResume() {
    super.onResume()
    startDiscovery()
  }

  override fun onPause() {
    stopDiscovery()
    super.onPause()
  }

  override fun onDestroy() {
    stopDiscovery()
    discoveredDevices.values.forEach { it.getServerInfo()?.close() }
    discoveredDevices.clear()
    clientConfigStorage.close()
    nsdClient.close()
    super.onDestroy()
  }

  private fun startDiscovery() {
    if (discoveryRunning) {
      return
    }

    discoveryRunning = true
    nsdClient.startDiscovery()

    // for UI we can update a bit faster than usual to get better responsiveness
    discoveryJob = lifecycleScope.launch {
      while (isActive) {
        delay(500.milliseconds)
        updateLayoutFromServices()
      }
    }
  }

  private fun stopDiscovery() {
    if (!discoveryRunning) {
      return
    }

    discoveryRunning = false
    discoveryJob?.cancel()
    discoveryJob = null
    nsdClient.stopDiscovery()
  }

  private fun updateLayoutFromServices() {
    val services = nsdClient.getDiscoveryResults()
    val deviceList = findViewById<ViewGroup>(R.id.device_list) ?: return

    val foundDevices = mutableSetOf<String>()
    for (service in services) {
      val key = getDeviceKey(service)
      foundDevices.add(key)

      val existingDevice = discoveredDevices[key]
      if (existingDevice != null) {
        // the view takes ownership of the fresh connection info and closes the previous one
        existingDevice.setServerInfo(service, clientConfigStorage)
        existingDevice.updateOnline(true)
      } else {
        addDiscoveredDevice(deviceList, service, key)
      }
    }

    for ((key, device) in discoveredDevices) {
      if (key !in foundDevices) {
        device.updateOnline(false)
      }
    }
  }

  private fun addDiscoveredDevice(
      deviceList: ViewGroup,
      serverInfo: ServerConnectionInfo,
      key: String) {
    val device = DiscoveredDeviceView(this)
    device.setServerInfo(serverInfo, clientConfigStorage)
    device.setOnClickListener {
      val info = device.getServerInfo() ?: return@setOnClickListener
      val intent = Intent(this, PairDeviceActivity::class.java)
      intent.putExtra("id", info.getServerId())
      intent.putExtra("ip", info.getIp())
      intent.putExtra("port", info.getPort())
      startActivity(intent)
    }
    device.updateOnline(true)
    deviceList.addView(device)
    discoveredDevices[key] = device
  }

  // a server may restart on a different port, so only its IP and ID identify it
  private fun getDeviceKey(serverInfo: ServerConnectionInfo): String {
    return "${serverInfo.getIp()}:${serverInfo.getServerId().contentToString()}"
  }

  fun onAddByIPClicked(view: View) {
    Toast.makeText(this, "Not implemented yet", Toast.LENGTH_SHORT).show()
  }
}
