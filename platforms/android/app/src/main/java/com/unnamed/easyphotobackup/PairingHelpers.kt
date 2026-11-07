package com.unnamed.easyphotobackup

object PairingHelpers {
    private external fun exchangePairingInformationWithServerNative(serverInfoHandle: Long): Long
    private external fun approveServerNative(
        clientConfigStorageHandle: Long,
        serverInfoHandle: Long,
        pendingBindingHandle: Long,
        serverName: String
    ): String?
    private external fun removePairedServerNative(
        clientConfigStorageHandle: Long,
        clientSentFilesStorageHandle: Long,
        serverInfoHandle: Long
    ): Boolean
    private external fun isServerPairedNative(
        clientConfigStorageHandle: Long,
        serverInfoHandle: Long
    ): Boolean
    private external fun getServerNameNative(
        clientConfigStorageHandle: Long,
        serverInfoHandle: Long
    ): String?

    fun exchangePairingInformationWithServer(serverInfo: ServerConnectionInfo): PendingServerBinding? {
        val handle = exchangePairingInformationWithServerNative(serverInfo.getNativeHandle())
        if (handle != 0L)
        {
            return PendingServerBinding(handle)
        }
        return null
    }

    fun approveServer(
        storage: ClientConfigStorage,
        serverInfo: ServerConnectionInfo,
        pendingBinding: PendingServerBinding,
        serverName: String
    ) : String? {
        return approveServerNative(
            storage.getNativeHandle(),
            serverInfo.getNativeHandle(),
            pendingBinding.getNativeHandle(),
            serverName
        )
    }

    // note that this function can lock if there is a write operation to any of the storages
    fun removePairedServer(configStorage: ClientConfigStorage, sentFilesStorage: ClientSentFilesStorage, serverInfo: ServerConnectionInfo): Boolean {
        return removePairedServerNative(
            configStorage.getNativeHandle(),
            sentFilesStorage.getNativeHandle(),
            serverInfo.getNativeHandle()
        )
    }

    fun isServerPaired(storage: ClientConfigStorage, serverInfo: ServerConnectionInfo): Boolean {
        return isServerPairedNative(
            storage.getNativeHandle(),
            serverInfo.getNativeHandle()
        )
    }

    fun getServerName(storage: ClientConfigStorage, serverInfo: ServerConnectionInfo): String? {
        return getServerNameNative(
            storage.getNativeHandle(),
            serverInfo.getNativeHandle()
        )
    }

    init {
        System.loadLibrary("EasyPhotoBackupFfi")
    }
}
