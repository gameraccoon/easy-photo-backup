package com.unnamed.easyphotobackup

class HiddenNsdClientNative {

    private external fun runDiscoveryNative(clientConfigStorageHandle: Long, clientSentFilesStorageHandle: Long, numRequests: Int): LongArray

    fun runDiscovery(clientConfigStorage: ClientConfigStorage, clientSentFilesStorage: ClientSentFilesStorage, numRequests: Int): Array<ServerConnectionInfo> {

        return runDiscoveryNative(clientConfigStorage.getNativeHandle(), clientSentFilesStorage.getNativeHandle(), numRequests)
            .map(::ServerConnectionInfo)
            .toTypedArray()
    }

    companion object {
        init {
            System.loadLibrary("EasyPhotoBackupFfi")
        }
    }
}
