package com.unnamed.easyphotobackup

import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.unnamed.easyphotobackup.databinding.ActivityJournalBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class JournalActivity : AppCompatActivity() {
    private lateinit var sentFilesStorage: ClientSentFilesStorage
    private lateinit var adapter: JournalAdapter
    private lateinit var binding: ActivityJournalBinding

    private val recordsPerPage = 16

    // should be big enough to always overflow the screen, on the largest device possible
    private val slidingWindowSize = recordsPerPage * 5
    private val preloadingThreshold = recordsPerPage / 2

    private var newestLoadedRecordExclusive = 0
    private var oldestLoadedRecordIndex = 0

    private var isLoadingOlder = false
    private var isLoadingNewer = false

    // can also be true if our last page managed to end on the last record
    private var hasMoreOlderRecords = true
    private var hasMoreNewerRecords = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        binding = ActivityJournalBinding.inflate(layoutInflater)
        setContentView(binding.root)

        sentFilesStorage = ClientSentFilesStorage(filesDir.absolutePath)

        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { _, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            binding.root.setPadding(
                systemBars.left, systemBars.top, systemBars.right, systemBars.bottom
            )
            insets
        }

        adapter = JournalAdapter()
        binding.rvJournal.layoutManager = LinearLayoutManager(this)
        binding.rvJournal.adapter = adapter

        setupInfiniteScroll()
        loadInitialRecords()
    }

    private fun setupInfiniteScroll() {
        binding.rvJournal.addOnScrollListener(object : RecyclerView.OnScrollListener() {
            override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                super.onScrolled(recyclerView, dx, dy)

                val layoutManager = recyclerView.layoutManager as LinearLayoutManager

                if (dy > 0) {
                    val lastVisible = layoutManager.findLastVisibleItemPosition()
                    if (lastVisible >= adapter.itemCount - preloadingThreshold) {
                        requestLoadOlder()
                    }
                } else if (dy < 0) {
                    val firstVisible = layoutManager.findFirstVisibleItemPosition()
                    if (firstVisible <= preloadingThreshold) {
                        requestLoadNewer()
                    }
                }
            }

            override fun onScrollStateChanged(recyclerView: RecyclerView, newState: Int) {
                super.onScrollStateChanged(recyclerView, newState)
                if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                    checkBoundaries()
                }
            }
        })

        binding.swipeRefresh.setOnRefreshListener {
            loadInitialRecords()
        }
    }

    private fun loadInitialRecords() {
        if (isLoadingNewer || isLoadingOlder) {
            binding.swipeRefresh.isRefreshing = false
            return
        }

        isLoadingNewer = true
        binding.swipeRefresh.isRefreshing = true

        lifecycleScope.launch {
            try {
                val (records, cursor) = withContext(Dispatchers.IO) {
                    sentFilesStorage.getLastActivityJournalRecords(slidingWindowSize)
                }

                val stringRecords = records.map { it.asString() }.reversed()
                adapter.setData(stringRecords)

                newestLoadedRecordExclusive = cursor
                oldestLoadedRecordIndex = cursor - records.size
                hasMoreOlderRecords = (records.size == slidingWindowSize)
                hasMoreNewerRecords = false
            } catch (e: Exception) {
                e.printStackTrace()
            } finally {
                isLoadingNewer = false
                binding.swipeRefresh.isRefreshing = false
                binding.rvJournal.post {
                    checkBoundaries()
                }
            }
        }
    }

    private fun checkBoundaries() {
        if (adapter.itemCount == 0) {
            return
        }

        val layoutManager = binding.rvJournal.layoutManager as LinearLayoutManager
        if (layoutManager.childCount == 0) {
            return
        }

        if (!binding.rvJournal.canScrollVertically(1)) {
            requestLoadOlder()
        }
        if (hasMoreNewerRecords && !binding.rvJournal.canScrollVertically(-1)) {
            requestLoadNewer()
        }
    }

    private fun requestLoadOlder() {
        if (isLoadingOlder || isLoadingNewer || !hasMoreOlderRecords) {
            return
        }
        loadOlderRecords()
    }

    private fun requestLoadNewer() {
        if (isLoadingNewer || isLoadingOlder) {
            return
        }
        loadNewerRecords()
    }

    private fun loadOlderRecords() {
        isLoadingOlder = true

        lifecycleScope.launch {
            try {
                val endIdx = oldestLoadedRecordIndex
                val beginIdx = maxOf(0, endIdx - recordsPerPage)

                if (beginIdx >= endIdx) {
                    hasMoreOlderRecords = false
                    return@launch
                }

                val records = withContext(Dispatchers.IO) {
                    sentFilesStorage.getActivityJournalRecords(beginIdx, endIdx)
                }

                val stringRecords = records.map { it.asString() }.reversed()

                if (stringRecords.isEmpty()) {
                    hasMoreOlderRecords = false
                    return@launch
                }

                val layoutManager = binding.rvJournal.layoutManager as LinearLayoutManager
                val anchorPosition = layoutManager.findFirstVisibleItemPosition()
                val anchorView = if (anchorPosition != RecyclerView.NO_POSITION) {
                    layoutManager.findViewByPosition(anchorPosition)
                } else {
                    null
                }
                val anchorOffset = anchorView?.let {
                    layoutManager.getDecoratedTop(it) - binding.rvJournal.paddingTop
                }

                adapter.addRecordsAtBottom(stringRecords)
                oldestLoadedRecordIndex = beginIdx

                if (beginIdx == 0 || records.size < recordsPerPage) {
                    hasMoreOlderRecords = false
                }

                val overflow = adapter.itemCount - slidingWindowSize

                if (overflow > 0) {
                    adapter.removeRecordsFromTop(overflow)
                    newestLoadedRecordExclusive -= overflow
                    hasMoreNewerRecords = true

                    if (anchorPosition != RecyclerView.NO_POSITION && anchorPosition >= overflow && anchorOffset != null) {
                        layoutManager.scrollToPositionWithOffset(
                            anchorPosition - overflow, anchorOffset
                        )
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            } finally {
                isLoadingOlder = false
                binding.rvJournal.post {
                    checkBoundaries()
                }
            }
        }
    }

    private fun loadNewerRecords() {
        isLoadingNewer = true

        lifecycleScope.launch {
            try {
                val beginIdx = newestLoadedRecordExclusive
                val endIdx = beginIdx + recordsPerPage

                val records = withContext(Dispatchers.IO) {
                    sentFilesStorage.getActivityJournalRecords(beginIdx, endIdx)
                }

                val stringRecords = records.map { it.asString() }.reversed()

                if (stringRecords.isEmpty()) {
                    return@launch
                }

                val layoutManager = binding.rvJournal.layoutManager as LinearLayoutManager
                val anchorPosition = layoutManager.findFirstVisibleItemPosition()
                val anchorView = if (anchorPosition != RecyclerView.NO_POSITION) {
                    layoutManager.findViewByPosition(anchorPosition)
                } else {
                    null
                }
                val anchorOffset = anchorView?.let {
                    layoutManager.getDecoratedTop(it) - binding.rvJournal.paddingTop
                }

                adapter.addRecordsAtTop(stringRecords)

                newestLoadedRecordExclusive += records.size

                val overflow = adapter.itemCount - slidingWindowSize

                hasMoreNewerRecords = (records.size == recordsPerPage)

                if (overflow > 0) {
                    adapter.removeRecordsFromBottom(overflow)
                    oldestLoadedRecordIndex += overflow
                    hasMoreOlderRecords = true
                }

                if (anchorPosition != RecyclerView.NO_POSITION && anchorOffset != null) {
                    layoutManager.scrollToPositionWithOffset(
                        anchorPosition + stringRecords.size, anchorOffset
                    )
                }
            } catch (e: Exception) {
                e.printStackTrace()
            } finally {
                isLoadingNewer = false
                binding.rvJournal.post {
                    checkBoundaries()
                }
            }
        }
    }
}

