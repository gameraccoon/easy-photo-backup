package com.unnamed.easyphotobackup

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.content.pm.ServiceInfo
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.NetworkInfo
import android.os.Build
import android.os.Environment
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat.getSystemService
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import androidx.work.workDataOf
import java.util.Vector
import kotlin.coroutines.cancellation.CancellationException
import androidx.core.content.edit
import androidx.work.ForegroundInfo
import kotlin.time.Duration.Companion.seconds

class FileSendBackgroundWorker(
    appContext: Context,
    params: WorkerParameters
) : CoroutineWorker(appContext, params) {
    private val clientConfigStorage = ClientConfigStorage(appContext.filesDir.absolutePath)
    private val clientSentFilesStorage = ClientSentFilesStorage(appContext.filesDir.absolutePath)

    // this is very test ofc
    private val foldersToSync = arrayOf("DCIM", "Download", "Pictures", "Videos")

    override suspend fun doWork(): Result {
        setForeground(createForegroundInfo())

        val connectivityManager =
            applicationContext.getSystemService(ConnectivityManager::class.java)

        val network = connectivityManager.activeNetwork
        val capabilities = connectivityManager.getNetworkCapabilities(network)

        val isWifi =
            capabilities?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true

        if (!isWifi) {
            setProgress(workDataOf("status" to "no wifi connection"))
            return Result.success()
        }

        val root = Environment.getExternalStorageDirectory().absolutePath
        val prefs = applicationContext.getSharedPreferences("backup_prefs", Context.MODE_PRIVATE)

        return try {
            prefs.edit {
                putString("last_status", "")
            }

            setProgress(workDataOf("status" to "discovering"))

            val discoveryClient = ServerDiscoveryClient()
            discoveryClient.startDiscovery()
            kotlinx.coroutines.delay(1.seconds)
            val discoveryResults = discoveryClient.getDiscoveryResults()
            discoveryClient.stopDiscovery()

            val statuses = Vector<String>()
            val resultStatus: String

            if (!discoveryResults.isEmpty()) {
                for (discoveryResult in discoveryResults) {
                    val serverName = PairingHelpers.requestServerName(discoveryResult)

                    if (!PairingHelpers.isServerPaired(clientConfigStorage, discoveryResult)) {
                        statuses.add("\nSkipped unknown server '$serverName'")
                        clientSentFilesStorage.addActivityJournalRecord(2, serverName.toString())
                        continue
                    }

                    clientSentFilesStorage.addActivityJournalRecord(0, serverName.toString())

                    for (folder in foldersToSync) {
                        setProgress(workDataOf("status" to "sending $folder to $serverName"))

                        val folderPath = "$root/$folder"
                        val sendStatus = FileSendHelpers.sendFiles(clientConfigStorage, clientSentFilesStorage,discoveryResult, folderPath, root)
                        if (sendStatus != null)
                        {
                            statuses.add("\n$folder: $sendStatus")
                        }
                    }
                }

                resultStatus = if (statuses.isEmpty()) {
                    "completed"
                } else {
                    "completed with status: " + statuses.joinToString(", ")
                }
            }
            else {
                resultStatus = "no servers"
                clientSentFilesStorage.addActivityJournalRecord(1, "")
            }

            prefs.edit {
                putString("last_status", resultStatus)
            }

            Result.success()
        } catch (_: CancellationException) {
            // ToDo: stop sending files here
            prefs.edit {
                putString("last_status", "cancelled")
            }
            Result.retry()
        } catch (e: Exception) {
            prefs.edit {
                putString("last_status", "exception caught $e")
            }
            Result.retry()
        }
    }

    private fun createForegroundInfo(): ForegroundInfo {
        val channelId = "sync_channel"
        val notificationId = 1001

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                channelId,
                "File Backup",
                NotificationManager.IMPORTANCE_LOW
            )
            val manager = applicationContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            manager.createNotificationChannel(channel)
        }

        val notification = NotificationCompat.Builder(applicationContext, channelId)
            .setContentTitle("File Backup")
            .setContentText("Synchronizing files in background...")
            .setSmallIcon(android.R.drawable.stat_notify_sync)
            .setOngoing(true)
            .build()

        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ForegroundInfo(notificationId, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
        } else {
            ForegroundInfo(notificationId, notification)
        }
    }
}
