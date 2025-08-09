package com.gameraccoon.easyphotobackup

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import uniffi.client_ffi.processSendingFiles

class MainActivity : AppCompatActivity() {
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    enableEdgeToEdge()
    setContentView(R.layout.activity_main)
    ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
      val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
      v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
      insets
    }

    val easyPhotoBackupApplication = application as EasyPhotoBackupApplication
    var clientStorage = easyPhotoBackupApplication.getClientStorage()
    if (clientStorage != null) {
      val pairedDevices = findViewById<ViewGroup>(R.id.paired_devices)
      pairedDevices.visibility = View.VISIBLE
      clientStorage.getPairedServers().forEach { serverInfo ->
        val pairedDeviceView = PairedDeviceView(this)
        pairedDeviceView.setServerInfo(serverInfo)
        pairedDeviceView.setOnClickListener { v ->
          val context = this
          val intent = Intent(context, DeviceSettingsActivity::class.java)
          intent.putExtra("id", serverInfo.getId())
          context.startActivity(intent)
        }
        pairedDevices.addView(pairedDeviceView)
      }
    }
  }

  fun onAddDeviceButtonClicked(view: View) {
    val context = this
    val intent = Intent(context, DiscoverDevicesActivity::class.java)
    context.startActivity(intent)
  }

  fun onTestSendFilesButtonClicked(view: View) {
    val easyPhotoBackupApplication = application as EasyPhotoBackupApplication
    val clientStorage = easyPhotoBackupApplication.getClientStorage()
    if (clientStorage != null) {
      val string = processSendingFiles(clientStorage)
      if (!string.isEmpty()) {
        Toast.makeText(this, string, Toast.LENGTH_LONG).show()
        println(string)
      }
    }
  }
}
