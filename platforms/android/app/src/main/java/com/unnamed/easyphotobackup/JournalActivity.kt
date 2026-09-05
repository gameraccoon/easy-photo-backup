package com.unnamed.easyphotobackup

import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.unnamed.easyphotobackup.databinding.ActivityJournalBinding
import kotlin.math.max

class JournalActivity : AppCompatActivity() {

    private lateinit var sentFilesStorage: ClientSentFilesStorage
    private lateinit var adapter: JournalAdapter
    private lateinit var binding: ActivityJournalBinding

    private val recordsPerPage = 8

    private var newestLoadedRecordIndex = 0
    private var oldestLoadedRecordIndex = 0

    private var isLoadingOlder = false
    private var isLoadingNewer = false

    // can also be true if our last page managed to end on the last record
    private var hasMoreOlderRecords = true

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

        adapter = JournalAdapter()
        binding.rvJournal.adapter = adapter

        setupInfiniteScroll()
        loadInitialRecords()
    }

    private fun setupInfiniteScroll() {
        binding.rvJournal.addOnScrollListener(
            object : RecyclerView.OnScrollListener() {
                override fun onScrolled(
                    recyclerView: RecyclerView,
                    dx: Int,
                    dy: Int
                ) {
                    super.onScrolled(recyclerView, dx, dy)

                    if (dy <= 0) return
                    if (isLoadingOlder || !hasMoreOlderRecords) return

                    val layoutManager = recyclerView.layoutManager as LinearLayoutManager

                    val lastVisible = layoutManager.findLastVisibleItemPosition()

                    if (lastVisible >= adapter.itemCount - 3) {
                        requestLoadMoreOlder()
                    }
                }

                override fun onScrollStateChanged(
                    recyclerView: RecyclerView,
                    newState: Int
                ) {
                    super.onScrollStateChanged(recyclerView, newState)

                    if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                        processLoadingOlder()
                    }
                }
            }
        )

        binding.swipeRefresh.setOnRefreshListener {
            loadNewerRecords()
        }
    }

    private fun loadInitialRecords() {
        isLoadingNewer = true

        try {
            val (records, cursor) =
                sentFilesStorage.getLastActivityJournalRecords(recordsPerPage)

            val stringRecords = records
                .map { it.asString() }
                .reversed()

            adapter.setData(stringRecords)

            newestLoadedRecordIndex = cursor
            oldestLoadedRecordIndex = cursor - records.size
            hasMoreOlderRecords = records.size == recordsPerPage

        } finally {
            isLoadingNewer = false
        }

        binding.rvJournal.post {
            processLoadingOlder()
        }
    }

    // check if we need to load more older records and start loading them if needed
    private fun processLoadingOlder() {
        if (isLoadingOlder || !hasMoreOlderRecords) {
            return
        }

        if (!binding.rvJournal.canScrollVertically(1)) {
            requestLoadMoreOlder()
        }
    }

    private fun requestLoadMoreOlder() {
        if (isLoadingOlder || !hasMoreOlderRecords) {
            return
        }

        binding.rvJournal.post {
            if (isLoadingOlder || !hasMoreOlderRecords) {
                return@post
            }

            loadMoreOlderRecords()
        }
    }

    private fun loadMoreOlderRecords() {
        if (isLoadingOlder || !hasMoreOlderRecords) {
            return
        }

        isLoadingOlder = true

        try {
            val endIdx = oldestLoadedRecordIndex
            val beginIdx = max(endIdx - recordsPerPage, 0)

            val records = sentFilesStorage.getActivityJournalRecords(beginIdx, endIdx)

            val stringRecords = records
                .map { it.asString() }
                .reversed()

            if (stringRecords.isNotEmpty()) {
                adapter.addRecords(stringRecords)
                oldestLoadedRecordIndex = endIdx - records.size
            }

            hasMoreOlderRecords = records.size == recordsPerPage

        } finally {
            isLoadingOlder = false
        }

        binding.rvJournal.post {
            processLoadingOlder()
        }
    }

    private fun loadNewerRecords() {
        if (isLoadingNewer) {
            binding.swipeRefresh.isRefreshing = false
            return
        }

        isLoadingNewer = true

        try {
            // ToDo: this is wasteful, maybe we should add some API to support this as one call
            val (_, latestCursor) =
                sentFilesStorage.getLastActivityJournalRecords(recordsPerPage)

            if (latestCursor <= newestLoadedRecordIndex) {
                return
            }

            val newRecords =
                sentFilesStorage.getActivityJournalRecords(newestLoadedRecordIndex, latestCursor)
                    .map { it.asString() }
                    .reversed()

            if (newRecords.isNotEmpty()) {
                adapter.addRecordsAtTop(newRecords)
                binding.rvJournal.scrollToPosition(0)
            }

            newestLoadedRecordIndex = latestCursor

        } catch (e: Exception) {
            e.printStackTrace()

        } finally {
            isLoadingNewer = false
            binding.swipeRefresh.isRefreshing = false
        }
    }
}
