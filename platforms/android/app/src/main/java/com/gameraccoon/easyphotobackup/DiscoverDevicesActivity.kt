package com.gameraccoon.easyphotobackup

import android.content.Intent
import android.os.Bundle
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.gameraccoon.core.NSDClient
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import uniffi.client_ffi.DiscoveredService

class DiscoverDevicesActivity : AppCompatActivity() {
  private val nsdClient = NSDClient()

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
    supportActionBar?.setDisplayHomeAsUpEnabled(true)

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
        var uiService = device.getService()
        if (uiService == null) {
          continue
        }

        // we consider servers with the same IP but different port to be the same as long as they
        // have the same ID
        // there may be multiple servers that match this criteria if a server was restarted with a
        // different port
        if (uiService.getIp() == services[j].getIp() &&
            uiService.getId() contentEquals services[j].getId()) {
          serviceFound = true
          device.setPort(services[j].getPort())
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

  fun addDiscoveredDevice(deviceList: ViewGroup, service: DiscoveredService) {
    val device = DiscoveredDeviceView(this)
    device.setService(service, this)
    device.setOnClickListener { v ->
      val context = this
      val intent = Intent(context, PairDeviceActivity::class.java)
      intent.putExtra("id", service.getId())
      intent.putExtra("ip", service.getIp())
      intent.putExtra("port", service.getPort().toInt())
      intent.putExtra("name", service.getName())
      context.startActivity(intent)
    }
    device.updateOnline(true)
    device.setPort(service.getPort())
    deviceList.addView(device)
  }

  fun onAddByIPClicked(view: View) {
    Toast.makeText(this, "Not implemented yet", Toast.LENGTH_SHORT).show()
  }

  override fun onOptionsItemSelected(item: MenuItem): Boolean {
    onBackPressedDispatcher.onBackPressed()
    return true
  }
}
