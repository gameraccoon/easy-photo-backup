package com.unnamed.easyphotobackup

import android.content.Context
import android.os.Environment
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import androidx.work.workDataOf
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.Vector
import kotlin.coroutines.cancellation.CancellationException
import androidx.core.content.edit

class FileSendBackgroundWorker(
    appContext: Context,
    params: WorkerParameters
) : CoroutineWorker(appContext, params) {
    private val clientConfigStorage = ClientConfigStorage(appContext.filesDir.absolutePath)
    private val clientSentFilesStorage = ClientSentFilesStorage(appContext.filesDir.absolutePath)

    // this is very test ofc
    private val foldersToSync = arrayOf("DCIM", "Download", "Pictures", "Videos")

    override suspend fun doWork(): Result {

        val root = Environment.getExternalStorageDirectory().absolutePath

        val prefs = applicationContext.getSharedPreferences("backup_prefs", Context.MODE_PRIVATE)

        return try {
            setProgress(workDataOf(
                "status" to "discovering"
            ))

            val discoveryClient = ServerDiscoveryClient()
            discoveryClient.startDiscovery()
            withContext(Dispatchers.IO) {
                Thread.sleep(3 * 1000)
            }
            val discoveryResults = discoveryClient.getDiscoveryResults()
            discoveryClient.stopDiscovery()

            val statuses = Vector<String>()
            val resultStatus: String

            if (!discoveryResults.isEmpty()) {
                for (discoveryResult in discoveryResults) {
                    val serverName = PairingHelpers.requestServerName(discoveryResult)

                    if (!PairingHelpers.isServerPaired(clientConfigStorage, discoveryResult)) {
                        statuses.add("\nSkipped unknown server '$serverName'")
                        continue
                    }

                    for (folder in foldersToSync) {
                        setProgress(workDataOf(
                            "status" to "sending $folder to $serverName"
                        ))

                        val folderPath = "$root/$folder"
                        val sendStatus = FileSendHelpers.sendFiles(clientConfigStorage, clientSentFilesStorage,discoveryResult, folderPath, root)
                        if (sendStatus != null)
                        {
                            statuses.add(sendStatus)
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
}
