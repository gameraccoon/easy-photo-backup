#include <format>
#include <string>

#include <jni.h>

#include "common_shared/cryptography/utils/connection_id_utils.h"

#include "client_shared/client_storage.h"
#include "client_shared/file_send_helpers.h"
#include "client_shared/pairing_helpers.h"
#include "client_shared/server_connection_info.h"
#include "client_shared/server_discovery_client.h"

class ServerDiscoveryClientNative
{
public:
	void startDiscovery()
	{
		mServerDiscoveryClient.startDiscovery();
	}

	[[nodiscard]] std::vector<ServerConnectionInfo> getDiscoveryResults()
	{
		return mServerDiscoveryClient.getDiscoveryResults();
	}

	void stopDiscovery()
	{
		mServerDiscoveryClient.stopDiscovery();
	}

private:
	ServerDiscoveryClient mServerDiscoveryClient;
};

class ServerConnectionInfoNative
{
public:
	ServerConnectionInfoNative(ServerConnectionInfo&& inServerInfo) noexcept
		: serverInfo(std::move(inServerInfo))
	{}

	ServerConnectionInfo serverInfo;
};

class PendingServerBindingNative
{
public:
	PendingServerBindingNative(PendingServerBinding&& inServerBinding) noexcept
		: serverBinding(std::move(inServerBinding))
	{}

	[[nodiscard]] std::string generateShortAuthentificationString() const noexcept
	{
		return serverBinding.generateShortAuthentificationString();
	}

	PendingServerBinding serverBinding;
};

class ClientConfigStorageNative
{
public:
	ClientConfigStorageNative(ClientConfigStorage&& inStorage) noexcept
		: storage(std::move(inStorage))
	{}

	ClientConfigStorage storage;
};

class ClientSentFilesStorageNative
{
public:
	ClientSentFilesStorageNative(ClientSentFilesStorage&& inStorage) noexcept
		: storage(std::move(inStorage))
	{}

	ClientSentFilesStorage storage;
};

class ActivityJournalRecordNative
{
public:
	ActivityJournalRecordNative(ClientSentFilesStorage::ActivityJournalRecord&& inRecord) noexcept
		: record(std::move(inRecord))
	{}

	ClientSentFilesStorage::ActivityJournalRecord record;

	[[nodiscard]] const char* getTypeName() const
	{
		switch (record.type)
		{
		case ClientSentFilesStorage::ActivityJournalRecord::Type::Start:
			return "start";
		case ClientSentFilesStorage::ActivityJournalRecord::Type::Continuation:
			return "continuation";
		case ClientSentFilesStorage::ActivityJournalRecord::Type::EndSuccessfully:
			return "end successfully";
		case ClientSentFilesStorage::ActivityJournalRecord::Type::EndError:
			return "end with error";
		case ClientSentFilesStorage::ActivityJournalRecord::Type::Unknown:
			return "unknown";
		}
	}
};

extern "C" JNIEXPORT jlong JNICALL
Java_com_unnamed_easyphotobackup_ServerDiscoveryClient_create(
	JNIEnv* env,
	jobject /*this*/
)
{
	ServerDiscoveryClientNative* obj = new ServerDiscoveryClientNative();
	return reinterpret_cast<jlong>(obj);
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_ServerDiscoveryClient_destroy(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	ServerDiscoveryClientNative* obj = reinterpret_cast<ServerDiscoveryClientNative*>(handle);
	delete obj;
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_ServerDiscoveryClient_startDiscoveryNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	ServerDiscoveryClientNative* obj = reinterpret_cast<ServerDiscoveryClientNative*>(handle);
	obj->startDiscovery();
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_unnamed_easyphotobackup_ServerDiscoveryClient_getDiscoveryResultsNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	ServerDiscoveryClientNative* obj = reinterpret_cast<ServerDiscoveryClientNative*>(handle);
	std::vector<ServerConnectionInfo> discoveryResults = obj->getDiscoveryResults();

	jlongArray result = env->NewLongArray(static_cast<jsize>(discoveryResults.size()));

	std::vector<jlong> handles;
	handles.reserve(discoveryResults.size());

	for (ServerConnectionInfo& discoveryResult : discoveryResults)
	{
		handles.push_back(reinterpret_cast<jlong>(new ServerConnectionInfoNative(std::move(discoveryResult))));
	}

	env->SetLongArrayRegion(
		result,
		0,
		static_cast<jsize>(handles.size()),
		handles.data()
	);

	return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_ServerDiscoveryClient_stopDiscoveryNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	ServerDiscoveryClientNative* obj = reinterpret_cast<ServerDiscoveryClientNative*>(handle);
	obj->stopDiscovery();
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_ServerConnectionInfo_destroy(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	ServerConnectionInfoNative* obj = reinterpret_cast<ServerConnectionInfoNative*>(handle);
	delete obj;
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_PendingServerBinding_destroy(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	PendingServerBindingNative* obj = reinterpret_cast<PendingServerBindingNative*>(handle);
	delete obj;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_unnamed_easyphotobackup_PendingServerBinding_generateShortAuthentificationString(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	PendingServerBindingNative* obj = reinterpret_cast<PendingServerBindingNative*>(handle);
	std::string sas = obj->generateShortAuthentificationString();
	return env->NewStringUTF(sas.c_str());
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_unnamed_easyphotobackup_ClientConfigStorage_open(
	JNIEnv* env,
	jobject /*this*/,
	jstring localStoragePathJStr
)
{
	const char* localStoragePathCStr = env->GetStringUTFChars(localStoragePathJStr, nullptr);
	const std::filesystem::path localStoragePath(localStoragePathCStr);
	env->ReleaseStringUTFChars(localStoragePathJStr, localStoragePathCStr);

	std::optional<ClientConfigStorage> clientConfigStorageResult = ClientConfigStorage::openStorage(localStoragePath);
	if (clientConfigStorageResult.has_value())
	{
		ClientConfigStorageNative* obj = new ClientConfigStorageNative(std::move(*clientConfigStorageResult));
		return reinterpret_cast<jlong>(obj);
	}

	return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_ClientConfigStorage_destroy(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	ClientConfigStorageNative* obj = reinterpret_cast<ClientConfigStorageNative*>(handle);
	delete obj;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_unnamed_easyphotobackup_ClientSentFilesStorage_open(
	JNIEnv* env,
	jobject /*this*/,
	jstring localStoragePathJStr
)
{
	const char* localStoragePathCStr = env->GetStringUTFChars(localStoragePathJStr, nullptr);
	const std::filesystem::path localStoragePath(localStoragePathCStr);
	env->ReleaseStringUTFChars(localStoragePathJStr, localStoragePathCStr);

	std::optional<ClientSentFilesStorage> clientSentFilesStorageResult = ClientSentFilesStorage::openStorage(localStoragePath);
	if (clientSentFilesStorageResult.has_value())
	{
		ClientSentFilesStorageNative* obj = new ClientSentFilesStorageNative(std::move(*clientSentFilesStorageResult));
		return reinterpret_cast<jlong>(obj);
	}

	return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_ClientSentFilesStorage_destroy(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	ClientSentFilesStorageNative* obj = reinterpret_cast<ClientSentFilesStorageNative*>(handle);
	delete obj;
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_ActivityJournalRecord_destroy(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	ActivityJournalRecordNative* obj = reinterpret_cast<ActivityJournalRecordNative*>(handle);
	delete obj;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_unnamed_easyphotobackup_PairingHelpers_requestServerNameNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong serverInfoHandle
)
{
	ServerConnectionInfoNative* info = reinterpret_cast<ServerConnectionInfoNative*>(serverInfoHandle);

	std::optional<std::string> serverName = PairingHelpers::requestServerName(info->serverInfo.address);

	if (!serverName.has_value())
	{
		return nullptr;
	}

	return env->NewStringUTF(serverName->c_str());
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_unnamed_easyphotobackup_PairingHelpers_exchangePairingInformationWithServerNative(
	JNIEnv* env,
	jobject /*myClass*/,
	jlong serverInfoHandle
)
{
	ServerConnectionInfoNative* info = reinterpret_cast<ServerConnectionInfoNative*>(serverInfoHandle);

	std::variant<std::string, PendingServerBinding> result = PairingHelpers::exchangePairInformationWithServer(info->serverInfo);

	if (std::holds_alternative<PendingServerBinding>(result))
	{
		return reinterpret_cast<jlong>(new PendingServerBindingNative(std::move(std::get<PendingServerBinding>(result))));
	}

	return 0;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_unnamed_easyphotobackup_PairingHelpers_approveServerNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientConfigStorageHandle,
	jlong serverInfoHandle,
	jlong pendingServerBindingHandle
)
{
	ClientConfigStorageNative* clientConfigStorage = reinterpret_cast<ClientConfigStorageNative*>(clientConfigStorageHandle);
	ServerConnectionInfoNative* info = reinterpret_cast<ServerConnectionInfoNative*>(serverInfoHandle);
	PendingServerBindingNative* pendingServerBindingNative = reinterpret_cast<PendingServerBindingNative*>(pendingServerBindingHandle);

	clientConfigStorage->storage.addConfirmedServerBinding(
		info->serverInfo.serverId,
		ClientConfigStorage::ServerBinding{
			.serverName = "test_server",
			.connectionId = Cryptography::generateConnectionId(pendingServerBindingNative->serverBinding.staticKeys.publicKey, pendingServerBindingNative->serverBinding.remoteStaticKey),
			.remoteStaticKey = std::move(pendingServerBindingNative->serverBinding.remoteStaticKey),
			.staticKeys = std::move(pendingServerBindingNative->serverBinding.staticKeys),
		}
	);

	return nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_unnamed_easyphotobackup_PairingHelpers_removePairedServerNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientConfigStorageHandle,
	jlong serverInfoHandle
)
{
	ClientConfigStorageNative* clientConfigStorage = reinterpret_cast<ClientConfigStorageNative*>(clientConfigStorageHandle);
	ServerConnectionInfoNative* info = reinterpret_cast<ServerConnectionInfoNative*>(serverInfoHandle);

	return clientConfigStorage->storage.removeConfirmedServerBinding(info->serverInfo.serverId);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_unnamed_easyphotobackup_PairingHelpers_isServerPairedNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientConfigStorageHandle,
	jlong serverInfoHandle
)
{
	ClientConfigStorageNative* clientConfigStorage = reinterpret_cast<ClientConfigStorageNative*>(clientConfigStorageHandle);
	ServerConnectionInfoNative* info = reinterpret_cast<ServerConnectionInfoNative*>(serverInfoHandle);

	return clientConfigStorage->storage.hasConfirmedServerBinding(info->serverInfo.serverId);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_unnamed_easyphotobackup_FileSendHelpers_sendFilesNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientConfigStorageHandle,
	jlong clientSentFilesStorageHandle,
	jlong serverInfoHandle,
	jstring folderPathJStr,
	jstring commonRootPathJStr
)
{
	ClientConfigStorageNative* clientConfigStorage = reinterpret_cast<ClientConfigStorageNative*>(clientConfigStorageHandle);
	ClientSentFilesStorageNative* clientSentFilesStorage = reinterpret_cast<ClientSentFilesStorageNative*>(clientSentFilesStorageHandle);
	ServerConnectionInfoNative* info = reinterpret_cast<ServerConnectionInfoNative*>(serverInfoHandle);

	const char* folderPathChar = env->GetStringUTFChars(folderPathJStr, nullptr);
	std::string folderPath(folderPathChar);
	env->ReleaseStringUTFChars(folderPathJStr, folderPathChar);

	const char* commonRootPathChar = env->GetStringUTFChars(commonRootPathJStr, nullptr);
	std::string commonRootPath(commonRootPathChar);
	env->ReleaseStringUTFChars(commonRootPathJStr, commonRootPathChar);

	std::optional<std::string> result = FileSendHelpers::sendDirectory(clientConfigStorage->storage, clientSentFilesStorage->storage, info->serverInfo, folderPath, commonRootPath);

	if (result.has_value())
	{
		return env->NewStringUTF(result->c_str());
	}

	return nullptr;
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_unnamed_easyphotobackup_ClientSentFilesStorage_getLastActivityJournalRecords(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientSentFilesStorageHandle,
	jint numberOfRecords
)
{
	ClientSentFilesStorageNative* clientSentFilesStorage = reinterpret_cast<ClientSentFilesStorageNative*>(clientSentFilesStorageHandle);

	uint32_t endRecord = 0;
	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = clientSentFilesStorage->storage.getLastActivityJournalRecords(numberOfRecords, endRecord);

	std::vector<jlong> handlesWithEndIdx;
	handlesWithEndIdx.reserve(records.size() + 1);
	for (ClientSentFilesStorage::ActivityJournalRecord& record : records)
	{
		handlesWithEndIdx.push_back(reinterpret_cast<jlong>(new ActivityJournalRecordNative(std::move(record))));
	}

	handlesWithEndIdx.push_back(static_cast<jlong>(endRecord));

	jlongArray result = env->NewLongArray(static_cast<jsize>(handlesWithEndIdx.size()));
	env->SetLongArrayRegion(
		result,
		0,
		static_cast<jsize>(handlesWithEndIdx.size()),
		handlesWithEndIdx.data()
	);

	return result;
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_unnamed_easyphotobackup_ClientSentFilesStorage_getActivityJournalRecords(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientSentFilesStorageHandle,
	jint beginIdx,
	jint endIdx
)
{
	ClientSentFilesStorageNative* clientSentFilesStorage = reinterpret_cast<ClientSentFilesStorageNative*>(clientSentFilesStorageHandle);

	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = clientSentFilesStorage->storage.getActivityJournalRecords(beginIdx, endIdx);

	jlongArray result = env->NewLongArray(static_cast<jsize>(records.size()));

	std::vector<jlong> handles;
	handles.reserve(records.size());
	for (ClientSentFilesStorage::ActivityJournalRecord& record : records)
	{
		handles.push_back(reinterpret_cast<jlong>(new ActivityJournalRecordNative(std::move(record))));
	}

	env->SetLongArrayRegion(
		result,
		0,
		static_cast<jsize>(handles.size()),
		handles.data()
	);

	return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_unnamed_easyphotobackup_ActivityJournalRecord_asString(
	JNIEnv* env,
	jobject /*this*/,
	jlong activityJournalRecordHandle
)
{
	ActivityJournalRecordNative* clientSentFilesStorage = reinterpret_cast<ActivityJournalRecordNative*>(activityJournalRecordHandle);

	std::string result = std::format(
		"{}, filesSent: {}, bytesSent: {}, time: {}",
		clientSentFilesStorage->getTypeName(),
		clientSentFilesStorage->record.filesSent,
		clientSentFilesStorage->record.bytesTransferred,
		clientSentFilesStorage->record.timestampMs
	);

	return env->NewStringUTF(result.c_str());
}
