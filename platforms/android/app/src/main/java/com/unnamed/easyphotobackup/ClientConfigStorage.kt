package com.unnamed.easyphotobackup

import java.lang.AutoCloseable

class ClientConfigStorage(localStorageDirectory: String) : AutoCloseable {
    private var nativeHandle: Long = open(localStorageDirectory)

    private external fun open(localStorageDirectory: String): Long
    private external fun destroy(handle: Long)
    private external fun getPairedServersNative(handle: Long): LongArray

    fun isValid(): Boolean {
        return nativeHandle != 0L
    }

    fun getPairedServers(): Array<ServerConnectionInfo> {
        check(nativeHandle != 0L)

        return getPairedServersNative(nativeHandle)
            .map(::ServerConnectionInfo)
            .toTypedArray()
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
