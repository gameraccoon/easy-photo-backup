package com.unnamed.easyphotobackup

import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.unnamed.easyphotobackup.databinding.ActivityJournalBinding

class JournalActivity : AppCompatActivity() {
    private lateinit var sentFilesStorage: ClientSentFilesStorage
    private lateinit var adapter: JournalAdapter

    private lateinit var binding: ActivityJournalBinding

    private var lastFetchedRecord: Int = 0
    private var lastScreenRecord: Int = 0
    private var thisScreenRecordsCount = 0

    private val recordsPerPage = 8

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        enableEdgeToEdge()

        binding = ActivityJournalBinding.inflate(layoutInflater)
        setContentView(binding.root)

        sentFilesStorage = ClientSentFilesStorage(filesDir.absolutePath)

        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { _, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            binding.root.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        adapter = JournalAdapter(emptyList())
        binding.rvJournal.adapter = adapter

        getLastRecords()

        binding.btnPrev.setOnClickListener {
            if (lastScreenRecord >= lastFetchedRecord)
            {
                getLastRecords()
                return@setOnClickListener
            }

            lastScreenRecord += recordsPerPage
            getRecords(lastScreenRecord - recordsPerPage, lastScreenRecord)
        }

        binding.btnNext.setOnClickListener {
            if (lastScreenRecord >= recordsPerPage * 2) {
                lastScreenRecord -= recordsPerPage
                getRecords(lastScreenRecord - recordsPerPage, lastScreenRecord)
            }
        }
    }

    fun getLastRecords()
    {
        val pair = sentFilesStorage.getLastActivityJournalRecords(recordsPerPage)
        lastScreenRecord = pair.second
        lastFetchedRecord = pair.second
        val stringRecords = pair.first.map { it.asString() }.reversed()
        thisScreenRecordsCount = stringRecords.size
        adapter.updateData(stringRecords)

        updateBtnNames()
    }

    fun getRecords(beginIdx: Int, endIdx: Int)
    {
        val newRecords = sentFilesStorage.getActivityJournalRecords(beginIdx, endIdx).map { it.asString() }.reversed()
        thisScreenRecordsCount = newRecords.size
        adapter.updateData(newRecords)

        updateBtnNames()
    }

    fun updateBtnNames()
    {
        if (lastScreenRecord >= lastFetchedRecord)
        {
            binding.btnPrev.setText("Update")
        }
        else
        {
            binding.btnPrev.setText("Prev")
        }
    }
}
