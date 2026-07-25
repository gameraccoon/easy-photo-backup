package com.unnamed.easyphotobackup

import java.lang.AutoCloseable

class ServerDiscoveryClient : AutoCloseable {
    private var nativeHandle: Long = create()

    private external fun create(): Long
    private external fun destroy(handle: Long)
    private external fun startDiscoveryNative(handle: Long)
    private external fun getDiscoveryResultsNative(handle: Long): LongArray
    private external fun stopDiscoveryNative(handle: Long)

    override fun close() {
        if (nativeHandle != 0L) {
            destroy(nativeHandle)
            nativeHandle = 0
        }
    }

    fun startDiscovery() {
        check(nativeHandle != 0L)
        startDiscoveryNative(nativeHandle)
    }

    fun getDiscoveryResults(): Array<ServerConnectionInfo> {
        check(nativeHandle != 0L)

        return getDiscoveryResultsNative(nativeHandle)
            .map(::ServerConnectionInfo)
            .toTypedArray()
    }

    fun stopDiscovery() {
        check(nativeHandle != 0L)
        stopDiscoveryNative(nativeHandle)
    }

    companion object {
        init {
            System.loadLibrary("EasyPhotoBackupFfi")
        }
    }
}
