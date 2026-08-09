package com.unnamed.easyphotobackup

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

class JournalAdapter(private var records: List<String>) :
    RecyclerView.Adapter<JournalAdapter.RecordViewHolder>() {

    class RecordViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val tvRecord: TextView = view.findViewById(R.id.tvRecord)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecordViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_journal_record, parent, false)
        return RecordViewHolder(view)
    }

    override fun onBindViewHolder(holder: RecordViewHolder, position: Int) {
        holder.tvRecord.text = records[position]
    }

    override fun getItemCount(): Int = records.size

    fun updateData(newRecords: List<String>) {
        records = newRecords
        notifyDataSetChanged()
    }
}