package com.gameraccoon.easyphotobackup

import android.content.Intent
import android.os.Bundle
import android.view.MenuItem
import android.view.View
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.preference.PreferenceFragmentCompat

class DeviceSettingsActivity : AppCompatActivity() {
  var deviceId: ByteArray = ByteArray(16)

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    enableEdgeToEdge()
    setContentView(R.layout.settings_activity)
    ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
      val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
      v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
      insets
    }
    supportActionBar?.setDisplayHomeAsUpEnabled(true)

    deviceId = intent.getByteArrayExtra("id")!!

    if (savedInstanceState == null) {
      supportFragmentManager.beginTransaction().replace(R.id.settings, SettingsFragment()).commit()
    }
  }

  class SettingsFragment : PreferenceFragmentCompat() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
      setPreferencesFromResource(R.xml.device_preferences, rootKey)
    }
  }

  override fun onOptionsItemSelected(item: MenuItem): Boolean {
    onBackPressedDispatcher.onBackPressed()
    return true
  }

  fun onRemoveDeviceButtonClicked(view: View) {
    val easyPhotoBackupApplication = application as EasyPhotoBackupApplication
    var clientStorage = easyPhotoBackupApplication.getClientStorage()
    if (clientStorage != null) {
      clientStorage.removePairedServer(deviceId)
      clientStorage.save()
      val intent = Intent(this, MainActivity::class.java)
      intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
      this.startActivity(intent)
    }
  }
}
