package com.unnamed.easyphotobackup

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.widget.Toast
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.net.toUri
import androidx.core.view.isVisible
import androidx.lifecycle.lifecycleScope
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkInfo
import androidx.work.WorkManager
import com.unnamed.easyphotobackup.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.concurrent.TimeUnit

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    private lateinit var clientConfigStorage: ClientConfigStorage
    private lateinit var clientSentFilesStorage: ClientSentFilesStorage

    private val serverInfos = mutableListOf<ServerConnectionInfo>()
    private var refreshGeneration = 0

    private val storagePermissionCode = 100

    @RequiresApi(Build.VERSION_CODES.R)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        clientConfigStorage = ClientConfigStorage(filesDir.absolutePath)
        clientSentFilesStorage = ClientSentFilesStorage(filesDir.absolutePath)

        ensureAllFilesAccess()

        val prefs = getSharedPreferences("backup_prefs", MODE_PRIVATE)
        binding.statusText.text = getString(R.string.status_initialization)
        WorkManager.getInstance(this)
            .getWorkInfosForUniqueWorkLiveData("file_backup")
            .observe(this) { workInfos ->
                val info = workInfos.firstOrNull() ?: return@observe

                if (info.state == WorkInfo.State.RUNNING) {
                    binding.statusText.text =
                        info.progress.getString("status") ?: "running without status"
                } else {
                    val lastStatus =
                        prefs.getString("last_status", "waiting for the worker thread to start")
                    binding.statusText.text = lastStatus
                }
            }

        binding.syncDebugButton.setOnClickListener {
            val constraints = Constraints.Builder()
                .setRequiredNetworkType(NetworkType.UNMETERED)
                .build()
            val request = PeriodicWorkRequestBuilder<FileSendBackgroundWorker>(
                15, TimeUnit.MINUTES
            ).setConstraints(constraints).build()

            WorkManager.getInstance(this)
                .enqueueUniquePeriodicWork(
                    "file_backup",
                    ExistingPeriodicWorkPolicy.CANCEL_AND_REENQUEUE,
                    request
                )
        }

        binding.pairButton.setOnClickListener {
            val intent = Intent(this, DiscoverDevicesActivity::class.java)
            startActivity(intent)
        }

        binding.fullJournalButton.setOnClickListener {
            val intent = Intent(this, JournalActivity::class.java)
            startActivity(intent)
        }
    }

    override fun onResume() {
        super.onResume()
        refreshPairedDevices()
    }

    override fun onDestroy() {
        serverInfos.forEach { it.close() }
        serverInfos.clear()
        clientConfigStorage.close()
        clientSentFilesStorage.close()
        super.onDestroy()
    }

    @SuppressLint("ObsoleteSdkInt")
    @RequiresApi(Build.VERSION_CODES.R)
    private fun ensureAllFilesAccess() {
        // Even though Android Studio says this check is not needed, it saves the app from crashing
        // on older android versions which otherwise work just fine
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                val intent = Intent(
                    Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    "package:$packageName".toUri()
                )
                startActivity(intent)
            }
        } else {
            // Android 10 (API 29) and below
            val permission = Manifest.permission.READ_EXTERNAL_STORAGE
            if (ContextCompat.checkSelfPermission(
                    this,
                    permission
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                // Request the permission
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(permission),
                    storagePermissionCode
                )
            }
        }
    }

    private fun refreshPairedDevices() {
        val generation = ++refreshGeneration

        lifecycleScope.launch {
            val pairedServers = withContext(Dispatchers.IO) {
                clientConfigStorage.getPairedServers().map { serverInfo ->
                    val serverName =
                        PairingHelpers.getServerName(clientConfigStorage, serverInfo)
                            ?: getString(R.string.unknown_server)
                    Pair(serverInfo, serverName)
                }
            }

            if (generation != refreshGeneration) {
                pairedServers.forEach { (serverInfo, _) -> serverInfo.close() }
                return@launch
            }

            binding.pairedDevices.removeAllViews()
            serverInfos.forEach { it.close() }
            serverInfos.clear()

            binding.noPairedServersText.isVisible = pairedServers.isEmpty()

            serverInfos.addAll(pairedServers.map { (serverInfo, _) -> serverInfo })

            for ((serverInfo, serverName) in pairedServers) {
                val pairedDeviceView = PairedDeviceView(this@MainActivity)
                pairedDeviceView.setServerInfo(serverInfo, serverName)
                pairedDeviceView.setOnClickListener {
                    confirmUnpair(serverInfo, serverName)
                }
                binding.pairedDevices.addView(pairedDeviceView)
            }
        }
    }

    private fun confirmUnpair(serverInfo: ServerConnectionInfo, serverName: String) {
        AlertDialog.Builder(this)
            .setTitle(R.string.remove_server_title)
            .setMessage(getString(R.string.remove_server_message, serverName))
            .setPositiveButton(R.string.yes) { _, _ ->
                lifecycleScope.launch {
                    val success = withContext(Dispatchers.IO) {
                        PairingHelpers.removePairedServer(
                            clientConfigStorage,
                            clientSentFilesStorage,
                            serverInfo
                        )
                    }

                    if (success) {
                        Toast.makeText(
                            this@MainActivity,
                            getString(R.string.server_removed, serverName),
                            Toast.LENGTH_SHORT
                        ).show()
                        refreshPairedDevices()
                    } else {
                        Toast.makeText(
                            this@MainActivity,
                            getString(R.string.could_not_remove_server, serverName),
                            Toast.LENGTH_LONG
                        ).show()
                    }
                }
            }
            .setNegativeButton(R.string.no, null)
            .show()
    }
}
