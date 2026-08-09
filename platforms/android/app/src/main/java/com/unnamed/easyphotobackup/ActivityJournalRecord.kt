package com.unnamed.easyphotobackup

import java.lang.AutoCloseable

class ActivityJournalRecord internal constructor(
    private var nativeHandle: Long
) : AutoCloseable {
    private external fun destroy(handle: Long)
    private external fun asString(handle: Long): String

    override fun close() {
        if (nativeHandle != 0L) {
            destroy(nativeHandle)
            nativeHandle = 0
        }
    }

    fun asString(): String {
        check(nativeHandle != 0L)
        return asString(nativeHandle)
    }

    companion object {
        init {
            System.loadLibrary("EasyPhotoBackupFfi")
        }
    }
}
