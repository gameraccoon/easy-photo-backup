package com.unnamed.easyphotobackup

import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import com.unnamed.easyphotobackup.databinding.ActivityMainBinding
import androidx.core.net.toUri
import androidx.lifecycle.lifecycleScope
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkInfo
import androidx.work.WorkManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.concurrent.TimeUnit

class MainActivity : AppCompatActivity() {

    private lateinit var clientConfigStorage: ClientConfigStorage

    private lateinit var binding: ActivityMainBinding

    @RequiresApi(Build.VERSION_CODES.R)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        clientConfigStorage = ClientConfigStorage(filesDir.absolutePath)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ensureAllFilesAccess()

        val prefs = getSharedPreferences("backup_prefs", MODE_PRIVATE)
        binding.sendFilesStatus.text = "initialization..."
        WorkManager.getInstance(this)
            .getWorkInfosForUniqueWorkLiveData("file_backup")
            .observe(this) { workInfos ->
                val info = workInfos.firstOrNull() ?: return@observe

                if (info.state == WorkInfo.State.RUNNING) {
                    binding.sendFilesStatus.text = info.progress.getString("status") ?: "running without status"
                } else {
                    val lastStatus = prefs.getString("last_status", "waiting for the worker thread to start")
                    binding.sendFilesStatus.text = lastStatus
                }
            }

        binding.syncButton.setOnClickListener {
            val request = PeriodicWorkRequestBuilder<FileSendBackgroundWorker>(
                15, TimeUnit.MINUTES
            ).build()

            WorkManager.getInstance(this)
                .enqueueUniquePeriodicWork(
                    "file_backup",
                    ExistingPeriodicWorkPolicy.REPLACE,
                    request
                )
        }

        binding.discoverButton.setOnClickListener {
            lifecycleScope.launch {
                discoverServers()
            }
        }

        binding.activityJournalButton.setOnClickListener {
            val intent = Intent(this, JournalActivity::class.java)
            startActivity(intent)
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun ensureAllFilesAccess() {
        if (!Environment.isExternalStorageManager()) {
            val intent = Intent(
                Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                "package:$packageName".toUri()
            )
            startActivity(intent)
        }
    }

    private suspend fun discoverServers() {
        binding.discoverButton.isEnabled = false
        binding.serverContainer.removeAllViews()

        val discoveryResults = withContext(Dispatchers.IO) {
            val discoveryClient = ServerDiscoveryClient()
            discoveryClient.startDiscovery()
            Thread.sleep(3000)
            val results = discoveryClient.getDiscoveryResults()
            discoveryClient.stopDiscovery()
            results
        }

        for (discoveryResult in discoveryResults) {
            val isPaired = PairingHelpers.isServerPaired(clientConfigStorage, discoveryResult)

            val serverName = withContext(Dispatchers.IO) {
                PairingHelpers.requestServerName(discoveryResult)
            }

            val row = LinearLayout(this@MainActivity).apply {
                orientation = LinearLayout.HORIZONTAL
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                )
                setPadding(0, 16, 0, 16)
            }

            val nameView = TextView(this@MainActivity).apply {
                text = serverName
                layoutParams = LinearLayout.LayoutParams(
                    0,
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    1f
                )
            }

            val pairButton = Button(this@MainActivity).apply {
                text = "Pair"
            }

            val removeButton = Button(this@MainActivity).apply {
                text = "Remove"
            }

            pairButton.isEnabled = !isPaired
            removeButton.isEnabled = isPaired

            pairButton.setOnClickListener {
                lifecycleScope.launch {
                    pairButton.isEnabled = false

                    val error = withContext(Dispatchers.IO) {
                        val pendingServerBinding = PairingHelpers.exchangePairingInformationWithServer(
                            discoveryResult
                        )

                        if (pendingServerBinding == null) {
                            "Could not pair to '$serverName'"
                        } else {
                            PairingHelpers.approveServer(
                                clientConfigStorage,
                                discoveryResult,
                                pendingServerBinding
                            )?.let {
                                "Could not pair to '$serverName': $it"
                            }
                        }
                    }

                    if (error == null) {
                        Toast.makeText(
                            this@MainActivity,
                            "Paired to '$serverName'",
                            Toast.LENGTH_SHORT
                        ).show()
                        removeButton.isEnabled = true
                    } else {
                        Toast.makeText(
                            this@MainActivity,
                            error,
                            Toast.LENGTH_LONG
                        ).show()
                        pairButton.isEnabled = true
                    }
                }
            }

            removeButton.setOnClickListener {
                AlertDialog.Builder(this@MainActivity)
                    .setTitle("Remove server")
                    .setMessage("Remove '$serverName'?")
                    .setPositiveButton("Yes") { _, _ ->
                        lifecycleScope.launch {
                            val success = withContext(Dispatchers.IO) {
                                PairingHelpers.removePairedServer(
                                    clientConfigStorage,
                                    discoveryResult
                                )
                            }

                            removeButton.isEnabled = !success
                            pairButton.isEnabled = success
                        }
                    }
                    .setNegativeButton("No", null)
                    .show()
            }

            row.addView(nameView)
            row.addView(pairButton)
            row.addView(removeButton)

            binding.serverContainer.addView(row)
        }

        binding.discoverButton.isEnabled = true
    }
}
