package com.unnamed.easyphotobackup

import java.lang.AutoCloseable

class ServerConnectionInfo internal constructor(
    private var nativeHandle: Long
) : AutoCloseable {

    private external fun destroy(handle: Long)

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

    companion object {
        init {
            System.loadLibrary("EasyPhotoBackupFfi")
        }
    }
}
