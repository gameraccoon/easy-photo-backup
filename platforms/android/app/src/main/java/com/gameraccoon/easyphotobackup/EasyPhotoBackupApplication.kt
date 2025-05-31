package com.gameraccoon.easyphotobackup

import android.app.Application
import java.nio.file.Paths
import uniffi.client_ffi.ClientStorage

class EasyPhotoBackupApplication : Application() {
  private var clientStorage: ClientStorage? = null

  override fun onCreate() {
    super.onCreate()
    clientStorage = ClientStorage(Paths.get(filesDir.absolutePath, "client_storage.bin").toString())
    // ToDo: we need to set it only once
    getClientStorage()!!.setDeviceName("Android device")
  }

  fun getClientStorage(): ClientStorage? {
    return clientStorage
  }
}
