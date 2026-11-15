package com.unnamed.easyphotobackup

import java.lang.AutoCloseable

class ServerConnectionInfo internal constructor(
    private var nativeHandle: Long
) : AutoCloseable {

    private external fun destroy(handle: Long)
    private external fun getServerIdNative(handle: Long): ByteArray
    private external fun getIpNative(handle: Long): String
    private external fun getPortNative(handle: Long): Int

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

    fun getServerId(): ByteArray {
        check(nativeHandle != 0L)
        return getServerIdNative(nativeHandle)
    }

    fun getIp(): String {
        check(nativeHandle != 0L)
        return getIpNative(nativeHandle)
    }

    fun getPort(): Int {
        check(nativeHandle != 0L)
        return getPortNative(nativeHandle)
    }

    fun getAddressString(): String {
        return "${getIp()}:${getPort()}"
    }

    companion object {
        @JvmStatic
        private external fun createNative(serverId: ByteArray, ip: String, port: Int): Long

        fun create(serverId: ByteArray, ip: String, port: Int): ServerConnectionInfo {
            return ServerConnectionInfo(createNative(serverId, ip, port))
        }

        init {
            System.loadLibrary("EasyPhotoBackupFfi")
        }
    }
}
