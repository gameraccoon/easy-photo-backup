package com.unnamed.easyphotobackup

object PairingHelpers {
    private external fun requestServerNameNative(serverInfoHandle: Long): String?
    private external fun exchangePairingInformationWithServerNative(serverInfoHandle: Long): Long
    private external fun approveServerNative(
        clientConfigStorageHandle: Long,
        serverInfoHandle: Long,
        pendingBindingHandle: Long
    ): String?
    private external fun removePairedServerNative(
        clientConfigStorageHandle: Long,
        serverInfoHandle: Long
    ): Boolean
    private external fun isServerPairedNative(
        clientConfigStorageHandle: Long,
        serverInfoHandle: Long
    ): Boolean

    fun exchangePairingInformationWithServer(serverInfo: ServerConnectionInfo): PendingServerBinding? {
        val handle = exchangePairingInformationWithServerNative(serverInfo.getNativeHandle())
        if (handle != 0L)
        {
            return PendingServerBinding(handle)
        }
        return null
    }

    fun requestServerName(serverInfo: ServerConnectionInfo): String? {
        return requestServerNameNative(serverInfo.getNativeHandle())
    }

    fun approveServer(
        storage: ClientConfigStorage,
        serverInfo: ServerConnectionInfo,
        pendingBinding: PendingServerBinding
    ) : String? {
        return approveServerNative(
            storage.getNativeHandle(),
            serverInfo.getNativeHandle(),
            pendingBinding.getNativeHandle()
        )
    }

    fun removePairedServer(storage: ClientConfigStorage, serverInfo: ServerConnectionInfo): Boolean {
        return removePairedServerNative(
            storage.getNativeHandle(),
            serverInfo.getNativeHandle()
        )
    }

    fun isServerPaired(storage: ClientConfigStorage, serverInfo: ServerConnectionInfo): Boolean {
        return isServerPairedNative(
            storage.getNativeHandle(),
            serverInfo.getNativeHandle()
        )
    }

    init {
        System.loadLibrary("EasyPhotoBackupFfi")
    }
}
