package com.gameraccoon.easyphotobackup

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.MenuItem
import android.view.View
import androidx.activity.enableEdgeToEdge
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.preference.EditTextPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import uniffi.client_ffi.ClientStorage
import uniffi.client_ffi.setDirectoryToSync

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
      supportFragmentManager
          .beginTransaction()
          .replace(R.id.settings, SettingsFragment(deviceId))
          .commit()
    }
  }

  class SettingsFragment : PreferenceFragmentCompat {
    private var isDirty = false
    private var clientStorage: ClientStorage? = null
    private var deviceId: ByteArray = ByteArray(16)

    constructor(deviceId: ByteArray) : super() {
      this.deviceId = deviceId
    }

    @RequiresApi(Build.VERSION_CODES.R)
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
      setPreferencesFromResource(R.xml.device_preferences, rootKey)

      val easyPhotoBackupApplication = activity?.application as EasyPhotoBackupApplication
      clientStorage = easyPhotoBackupApplication.getClientStorage()

      val filePathPreference = findPreference<EditTextPreference>("file_path")
      val syncPath = clientStorage?.getServerSyncPath(deviceId)
      if (syncPath == null || syncPath.isEmpty()) {
        filePathPreference?.text = ""
      } else {
        filePathPreference?.text = syncPath
      }

      filePathPreference?.onPreferenceChangeListener =
          Preference.OnPreferenceChangeListener { preference, newValue ->
            setDirectoryToSync(clientStorage!!, deviceId, newValue.toString())
            isDirty = true

            val granted =
                activity?.checkSelfPermission(
                    android.Manifest.permission.MANAGE_EXTERNAL_STORAGE) ==
                    PackageManager.PERMISSION_GRANTED
            if (!granted) {
              val intent =
                  Intent(android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
              this.startActivity(intent)
            }

            true
          }
    }

    override fun onPause() {
      super.onPause()
      if (isDirty) {
        clientStorage!!.save()
        isDirty = false
      }
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
