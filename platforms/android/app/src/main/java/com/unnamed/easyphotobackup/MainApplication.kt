package com.unnamed.easyphotobackup

import android.app.Application
import android.content.Context
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import java.util.concurrent.TimeUnit

class MainApplication : Application() {
    override fun onCreate() {
        super.onCreate()

        SyncScheduler.start(this)
    }
}

object SyncScheduler {
    fun start(context: Context) {
        val constraints = Constraints.Builder()
            .setRequiredNetworkType(NetworkType.UNMETERED)
            .build()

        val request = PeriodicWorkRequestBuilder<FileSendBackgroundWorker>(
            20, TimeUnit.MINUTES
        ).setConstraints(constraints).build()

        WorkManager.getInstance(context)
            .enqueueUniquePeriodicWork(
                "file_backup",
                ExistingPeriodicWorkPolicy.KEEP,
                request
            )
    }
}
