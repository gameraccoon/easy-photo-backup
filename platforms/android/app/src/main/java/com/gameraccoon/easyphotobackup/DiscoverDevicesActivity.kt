package com.gameraccoon.easyphotobackup

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
        nsdClient.startDiscovery()
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
            var foundServiceIndex = -1
            for (j in 0 until services.size) {
                if (device.service == services[j].port.toInt()) {
                    foundServiceIndex = j
                    break
                }
            }

            if (foundServiceIndex != -1) {
                // ToDo: update the text field with "Seen: now"
                services.removeAt(foundServiceIndex)
            } else {
                // ToDo: update the text with the last seen time
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
        device.service = service.port.toInt()
        device.setOnClickListener { v ->
            println("device clicked")
        }
        deviceList.addView(device)
    }

    fun onAddByIPClicked(view: View) {
        println("addByIpButtonOnClick")
    }
}
