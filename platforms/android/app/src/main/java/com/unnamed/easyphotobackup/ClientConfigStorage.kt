package com.unnamed.easyphotobackup

import java.lang.AutoCloseable

class ClientConfigStorage(localStorageDirectory: String) : AutoCloseable {
    private var nativeHandle: Long = open(localStorageDirectory)

    private external fun open(localStorageDirectory: String): Long
    private external fun destroy(handle: Long)

    fun isValid(): Boolean {
        return nativeHandle != 0L
    }

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

    companion object {
        init {
            System.loadLibrary("EasyPhotoBackupFfi")
        }
    }
}
