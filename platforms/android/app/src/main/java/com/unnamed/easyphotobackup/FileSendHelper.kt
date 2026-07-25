package com.unnamed.easyphotobackup

object FileSendHelpers {
    private external fun sendFilesNative(
        storageHandle: Long,
        sentFilesHandle: Long,
        serverInfoHandle: Long,
        folder: String,
        commonRoot: String
    ): String?
    fun sendFiles(
        storage: ClientConfigStorage,
        sentFiles: ClientSentFilesStorage,
        server: ServerConnectionInfo,
        folder: String,
        commonRoot: String
    ): String? {
        return sendFilesNative(
            storage.getNativeHandle(),
            sentFiles.getNativeHandle(),
            server.getNativeHandle(),
            folder,
            commonRoot
        )
    }

    init {
        System.loadLibrary("EasyPhotoBackupFfi")
    }
}
