package com.unnamed.easyphotobackup

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

class JournalAdapter(
    private val records: MutableList<String> = mutableListOf()
) : RecyclerView.Adapter<JournalAdapter.RecordViewHolder>() {
    class RecordViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val tvRecord: TextView = view.findViewById(R.id.tvRecord)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecordViewHolder {
        val view =
            LayoutInflater.from(parent.context).inflate(R.layout.item_journal_record, parent, false)

        return RecordViewHolder(view)
    }

    override fun onBindViewHolder(holder: RecordViewHolder, position: Int) {
        holder.tvRecord.text = records[position]
    }

    override fun getItemCount(): Int = records.size

    fun setData(newRecords: List<String>) {
        records.clear()
        records.addAll(newRecords)

        notifyDataSetChanged()
    }

    fun addRecordsAtBottom(newRecords: List<String>) {
        if (newRecords.isEmpty()) {
            return
        }

        val startPosition = records.size

        records.addAll(newRecords)
        notifyItemRangeInserted(startPosition, newRecords.size)
    }

    fun addRecordsAtTop(newRecords: List<String>) {
        if (newRecords.isEmpty()) {
            return
        }

        records.addAll(0, newRecords)

        notifyItemRangeInserted(0, newRecords.size)
    }

    fun removeRecordsFromTop(count: Int) {
        if (count <= 0) {
            return
        }

        val actualCount = minOf(count, records.size)
        records.subList(0, actualCount).clear()

        notifyItemRangeRemoved(0, actualCount)
    }

    fun removeRecordsFromBottom(count: Int) {
        if (count <= 0) {
            return
        }

        val actualCount = minOf(count, records.size)
        val startPosition = records.size - actualCount
        records.subList(startPosition, records.size).clear()

        notifyItemRangeRemoved(startPosition, actualCount)
    }
}
