package com.gameraccoon.easyphotobackup

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.gameraccoon.core.NSDClient
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import uniffi.client_ffi.Service

class DiscoverDevicesActivity : AppCompatActivity() {
  val nsdClient = NSDClient()

  @OptIn(DelicateCoroutinesApi::class)
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    // for UI we can update it a bit faster than usual to get better responsiveness
    nsdClient.startDiscovery(1000u)
    enableEdgeToEdge()
    setContentView(R.layout.activity_discover_devices)
    ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
      val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
      v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
      insets
    }

    // start a coroutine to listen for new devices once in 100ms
    GlobalScope.launch {
      while (true) {
        delay(100)
        // run in main thread
        runOnUiThread {
          // update the layout with the new devices
          updateLayoutFromServices()
        }
      }
    }
  }

  private fun updateLayoutFromServices() {
    var services = nsdClient.getServices().toMutableList()
    val deviceList = findViewById<ViewGroup>(R.id.device_list) as ViewGroup
    // go through the list of displayed devices and update the ones we found
    for (i in 0 until deviceList.childCount) {
      val device = deviceList.getChildAt(i) as DiscoveredDeviceView
      var serviceFound = false
      for (j in services.size - 1 downTo 0) {
        // we consider servers with the same IP but different port to be the same as long as they
        // have the same ID
        // there may be multiple servers that match this criteria if a server was restarted with a
        // different port
        if (device.ip == services[j].ip && device.id contentEquals services[j].id) {
          serviceFound = true
          // just in case the port has changed
          device.port = services[j].port.toInt()
          services.removeAt(j)
        }
      }

      if (serviceFound) {
        device.updateOnline(true)
      } else {
        device.updateOnline(false)
      }
    }

    // add remaining services to the list
    for (service in services) {
      addDiscoveredDevice(deviceList, service)
    }
  }

  override fun onDestroy() {
    nsdClient.stopDiscovery(false)
    super.onDestroy()
  }

  fun addDiscoveredDevice(deviceList: ViewGroup, service: Service) {
    val device = DiscoveredDeviceView(this)
    device.ip = service.ip
    device.port = service.port.toInt()
    device.id = service.id
    device.setOnClickListener { v ->
      val context = this
      val intent = Intent(context, PairDeviceActivity::class.java)
      context.startActivity(intent)
    }
    device.updateOnline(true)
    deviceList.addView(device)
  }

  fun onAddByIPClicked(view: View) {
    println("addByIpButtonOnClick")
  }
}
