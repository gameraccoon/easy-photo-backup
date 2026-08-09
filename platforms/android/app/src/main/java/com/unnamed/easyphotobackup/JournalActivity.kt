package com.unnamed.easyphotobackup

import android.os.Bundle
import android.widget.Button
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.recyclerview.widget.RecyclerView
import com.unnamed.easyphotobackup.databinding.ActivityJournalBinding

class JournalActivity : AppCompatActivity() {
    private lateinit var sentFilesStorage: ClientSentFilesStorage
    private lateinit var adapter: JournalAdapter

    private lateinit var binding: ActivityJournalBinding

    private var lastRecord: Int = 0

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

        val pair = sentFilesStorage.getLastActivityJournalRecords(10)
        lastRecord = pair.second

        val stringRecords = pair.first.map { it.asString() }
        adapter.updateData(stringRecords)

        binding.btnPrev.setOnClickListener {
            lastRecord += 10
            adapter.updateData(sentFilesStorage.getActivityJournalRecords(lastRecord - 10, lastRecord).map { it.asString() })
        }

        binding.btnNext.setOnClickListener {
            lastRecord -= 10
            if (lastRecord < 10)
            {
                lastRecord = 10
            }
            adapter.updateData(sentFilesStorage.getActivityJournalRecords(lastRecord - 10, lastRecord).map { it.asString() })
        }
    }
}
