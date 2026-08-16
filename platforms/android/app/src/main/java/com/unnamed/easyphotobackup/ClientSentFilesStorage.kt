package com.unnamed.easyphotobackup

import java.lang.AutoCloseable

class ClientSentFilesStorage(localStorageDirectory: String) : AutoCloseable {
    private var nativeHandle: Long = open(localStorageDirectory)

    private external fun open(localStorageDirectory: String): Long
    private external fun destroy(handle: Long)
    private external fun getLastActivityJournalRecords(handle: Long, numberOfRecords: Int): LongArray
    private external fun getActivityJournalRecords(handle: Long, beginIdx: Int, endIdx: Int): LongArray
    private external fun addActivityJournalRecord(handle: Long, typeId: Int): Boolean

    internal fun getNativeHandle(): Long {
        check(nativeHandle != 0L)
        return nativeHandle
    }

    override fun close() {
        if (nativeHandle != 0L) {
            destroy(nativeHandle)
            nativeHandle = 0
        }
    }

    fun getLastActivityJournalRecords(numberOfRecords: Int): Pair<Array<ActivityJournalRecord>, Int> {
        check(nativeHandle != 0L)

        val records = getLastActivityJournalRecords(nativeHandle, numberOfRecords)
        check(!records.isEmpty())

        return Pair(records.slice(0..records.size-2).map(::ActivityJournalRecord).toTypedArray(), (records[records.size - 1]).toInt())
    }

    fun getActivityJournalRecords(beginIdx: Int, endIdx: Int): Array<ActivityJournalRecord> {
        check(nativeHandle != 0L)

        return getActivityJournalRecords(nativeHandle, beginIdx, endIdx)
            .map(::ActivityJournalRecord)
            .toTypedArray()
    }

    fun addActivityJournalRecord(typeIdx: Int): Boolean {
        check(nativeHandle != 0L)

        return addActivityJournalRecord(nativeHandle, typeIdx)
    }

    companion object {
        init {
            System.loadLibrary("EasyPhotoBackupFfi")
        }
    }
}
