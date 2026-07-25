package com.unnamed.easyphotobackup

import java.lang.AutoCloseable

class PendingServerBinding internal constructor(
    private var nativeHandle: Long
) : AutoCloseable {

    private external fun destroy(handle: Long)
    private external fun generateShortAuthentificationString(handle: Long) : String

    override fun close() {
        if (nativeHandle != 0L) {
            destroy(nativeHandle)
            nativeHandle = 0
        }
    }

    internal fun getNativeHandle(): Long {
        check(nativeHandle != 0L)
        return nativeHandle
    }

    fun generateShortAuthentificationString(): String {
        check(nativeHandle != 0L)
        return generateShortAuthentificationString(nativeHandle)
    }

    companion object {
        init {
            System.loadLibrary("EasyPhotoBackupFfi")
        }
    }
}
