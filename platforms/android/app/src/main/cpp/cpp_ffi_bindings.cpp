#include <format>
#include <string>

#include <jni.h>

#include "common_shared/cryptography/utils/connection_id_utils.h"
#include "common_shared/requests/pairing_utils.h"

#include "client_shared/client_storage.h"
#include "client_shared/file_send_helpers.h"
#include "client_shared/pairing_helpers.h"
#include "client_shared/server_connection_info.h"
#include "client_shared/hidden_nsd_client_wrapper.h"
#include "client_shared/simple_nsd_client_wrapper.h"

class SimpleNsdClientWrapperNative
{
public:
	void startDiscovery()
	{
		mWrapper.startDiscovery();
	}

	[[nodiscard]] std::vector<ServerConnectionInfo> getDiscoveryResults()
	{
		return mWrapper.getDiscoveryResults();
	}

	void stopDiscovery()
	{
		mWrapper.stopDiscovery();
	}

private:
	SimpleNsdClientWrapper mWrapper;
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
};

extern "C" JNIEXPORT jlong JNICALL
Java_com_unnamed_easyphotobackup_SimpleNsdClientNative_create(
	JNIEnv* env,
	jobject /*this*/
)
{
	SimpleNsdClientWrapperNative* obj = new SimpleNsdClientWrapperNative();
	return reinterpret_cast<jlong>(obj);
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_SimpleNsdClientNative_destroy(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	SimpleNsdClientWrapperNative* obj = reinterpret_cast<SimpleNsdClientWrapperNative*>(handle);
	delete obj;
}

extern "C" JNIEXPORT void JNICALL
Java_com_unnamed_easyphotobackup_SimpleNsdClientNative_startDiscoveryNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	SimpleNsdClientWrapperNative* obj = reinterpret_cast<SimpleNsdClientWrapperNative*>(handle);
	obj->startDiscovery();
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_unnamed_easyphotobackup_SimpleNsdClientNative_getDiscoveryResultsNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	SimpleNsdClientWrapperNative* obj = reinterpret_cast<SimpleNsdClientWrapperNative*>(handle);
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
Java_com_unnamed_easyphotobackup_SimpleNsdClientNative_stopDiscoveryNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong handle
)
{
	SimpleNsdClientWrapperNative* obj = reinterpret_cast<SimpleNsdClientWrapperNative*>(handle);
	obj->stopDiscovery();
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_unnamed_easyphotobackup_HiddenNsdClientNative_runDiscoveryNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientConfigStorageHandle,
	jlong clientSentFilesStorageHandle,
	jint numRequests
)
{
	ClientConfigStorageNative* clientConfigStorage = reinterpret_cast<ClientConfigStorageNative*>(clientConfigStorageHandle);
	ClientSentFilesStorageNative* clientSentFilesStorage = reinterpret_cast<ClientSentFilesStorageNative*>(clientSentFilesStorageHandle);

	std::vector<HiddenNsdClient::NsdServerData> serverData;
	std::vector<ClientConfigStorage::ServerIdentificationHashData> serverHashes;
	clientConfigStorage->storage.getAllIdentificationHashes(ClientConfigStorage::IdentificationTagType::Nsd, serverHashes);
	serverData.reserve(serverHashes.size());
	for (ClientConfigStorage::ServerIdentificationHashData& hashData : serverHashes)
	{
		std::optional<uint32_t> indexResult = clientSentFilesStorage->storage.consumeNextIdentificationKeyIndicesInPeriod(hashData.serverIdx, ClientConfigStorage::IdentificationTagType::Nsd, Cryptography::IdentificationTagsUtils::getCurrentDayIndex(), numRequests);
		if (indexResult.has_value())
		{
			serverData.emplace_back(
				std::move(hashData.hash),
				*indexResult
			);
		}
	}

	std::vector<ServerConnectionInfo> discoveryResults = HiddenNsdClientWrapper::runDiscovery(serverData, numRequests);

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
	jlong pendingServerBindingHandle,
	jstring serverNameJStr
)
{
	ClientConfigStorageNative* clientConfigStorage = reinterpret_cast<ClientConfigStorageNative*>(clientConfigStorageHandle);
	ServerConnectionInfoNative* info = reinterpret_cast<ServerConnectionInfoNative*>(serverInfoHandle);
	PendingServerBindingNative* pendingServerBindingNative = reinterpret_cast<PendingServerBindingNative*>(pendingServerBindingHandle);

	const char* serverNameChar = env->GetStringUTFChars(serverNameJStr, nullptr);
	std::string serverName(serverNameChar);
	env->ReleaseStringUTFChars(serverNameJStr, serverNameChar);

	clientConfigStorage->storage.addConfirmedServerBinding(
		info->serverInfo.serverId,
		ClientConfigStorage::ServerBinding{
			.serverName = std::move(serverName),
			.nsdIdentificationHash = PairingUtils::generateNsdIdentificationHash(pendingServerBindingNative->serverBinding.handshakeHash),
			.encryptedRequestIdentificationHash = PairingUtils::generateEncryptedRequestIdentificationHash(pendingServerBindingNative->serverBinding.handshakeHash),
			.remoteStaticKey = std::move(pendingServerBindingNative->serverBinding.remoteStaticKey),
			.staticKeys = std::move(pendingServerBindingNative->serverBinding.staticKeys),
		}
	);

	// ToDo: pass back the error

	return nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_unnamed_easyphotobackup_PairingHelpers_removePairedServerNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientConfigStorageHandle,
	jlong clientSentFilesStorageHandle,
	jlong serverInfoHandle
)
{
	ClientConfigStorageNative* clientConfigStorage = reinterpret_cast<ClientConfigStorageNative*>(clientConfigStorageHandle);
	ClientSentFilesStorageNative* clientSentFilesStorage = reinterpret_cast<ClientSentFilesStorageNative*>(clientSentFilesStorageHandle);
	ServerConnectionInfoNative* info = reinterpret_cast<ServerConnectionInfoNative*>(serverInfoHandle);

	auto serverIdx = clientConfigStorage->storage.removeConfirmedServerBinding(info->serverInfo.serverId);
	if (serverIdx.has_value())
	{
		clientSentFilesStorage->storage.deleteServer(*serverIdx);
		return true;
	}

	return false;
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
Java_com_unnamed_easyphotobackup_PairingHelpers_getServerNameNative(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientConfigStorageHandle,
	jlong serverInfoHandle
)
{
	ClientConfigStorageNative* obj = reinterpret_cast<ClientConfigStorageNative*>(clientConfigStorageHandle);
	ServerConnectionInfoNative* info = reinterpret_cast<ServerConnectionInfoNative*>(serverInfoHandle);
	std::optional<ClientConfigStorage::ServerBinding> serverBinding = obj->storage.getConfirmedServerBinding(info->serverInfo.serverId);
	if (serverBinding.has_value())
	{
		return env->NewStringUTF(serverBinding->serverName.c_str());
	}

	return nullptr;
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

extern "C" JNIEXPORT jboolean JNICALL
Java_com_unnamed_easyphotobackup_ClientSentFilesStorage_addActivityJournalRecord(
	JNIEnv* env,
	jobject /*this*/,
	jlong clientSentFilesStorageHandle,
	jint eventIdx,
	jstring additionalInfoJStr
)
{
	ClientSentFilesStorageNative* clientSentFilesStorage = reinterpret_cast<ClientSentFilesStorageNative*>(clientSentFilesStorageHandle);

	ClientSentFilesStorage::ActivityJournalRecord::Type recordType = ClientSentFilesStorage::ActivityJournalRecord::Type::Unknown;
	switch (eventIdx)
	{
	case 0:
		recordType = ClientSentFilesStorage::ActivityJournalRecord::Type::FoundServer;
		break;
	case 1:
		recordType = ClientSentFilesStorage::ActivityJournalRecord::Type::NoServers;
		break;
	case 2:
		recordType = ClientSentFilesStorage::ActivityJournalRecord::Type::UnknownServer;
		break;
	default:
		break;
	}

	const char* additionalInfoChar = env->GetStringUTFChars(additionalInfoJStr, nullptr);
	std::string additionalInfo(additionalInfoChar);
	env->ReleaseStringUTFChars(additionalInfoJStr, additionalInfoChar);

	ClientSentFilesStorage::ActivityJournalRecord activityRecord{
		.timestampMs = ClientSentFilesStorage::ActivityJournalRecord::convertTimeToMs(std::chrono::system_clock::now()),
		.bytesTransferred = 0,
		.filesCount = 0,
		.type = recordType,
		.additionalInfo = std::move(additionalInfo),
	};

	return clientSentFilesStorage->storage.addActivityJournalRecord(std::move(activityRecord));
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

	std::string result = clientSentFilesStorage->record.asString();

	return env->NewStringUTF(result.c_str());
}
