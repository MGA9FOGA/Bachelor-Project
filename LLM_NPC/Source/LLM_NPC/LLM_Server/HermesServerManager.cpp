#include "HermesServerManager.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

UHermesServerManager* UHermesServerManager::m_singletonInstance = nullptr;

void UHermesServerManager::Initialize(FSubsystemCollectionBase& _collection)
{
	Super::Initialize(_collection);

	if (m_singletonInstance != nullptr && m_singletonInstance != this)
	{
		return;
	}

	m_singletonInstance = this;

	m_configPath = TEXT("D:/LLM-NPC/LLM_NPC/Deploy/LLMServer/ServerConfig.ini");
	LoadConfigFromFile();

	StartHermesServer();
}


void UHermesServerManager::Deinitialize()
{
	if (m_bShuttingDown)
	{
		return;
	}
	m_bShuttingDown = true;

	if (m_singletonInstance == this)
	{
		m_singletonInstance = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(m_startupPollTimer);
	}

	StopHermesServer();
}

void UHermesServerManager::StartHermesServer()
{
	if (m_bServerRunning)
	{
		return;
	}

	const int32 parallel = 2;
	FString commandLine = FString::Printf(
		TEXT("-m \"%s\" -ngl %d --ctx-size %d -n %d --port %d --temp %f -b %d --parallel %d --cont-batching"),
		*m_modelPath,
		m_hermesNGL, // Aus Config!
		m_hermesContext * parallel, // Gesamt-Kontext = Config-Kontext × Slots
		m_hermesTokens, // Aus Config!
		m_hermesPort, // Aus Config!
		m_hermesTemp, // Aus Config!
		m_hermesBatch, // Aus Config!
		parallel
	);
	
	FString workingDir = FPaths::GetPath(m_serverExePath);

	// Starte Process 
	m_serverProcessHandle = FPlatformProcess::CreateProc(
		*m_serverExePath,
		*commandLine,
		true, // Launch detached (im Hintergrund)
		true, // Launch hidden (kein Fenster!)
		false, // Launch in new process group
		nullptr, // OUT process ID
		0, // Priority Boost
		*workingDir, // Working directory
		nullptr // Pipe handle
	);

	if (!m_serverProcessHandle.IsValid())
	{
		return;
	}

	m_bServerRunning = false;


	PollServerReady(60);   /
}

void UHermesServerManager::StopHermesServer()
{
	if (m_serverProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(m_serverProcessHandle);
		m_bServerRunning = false;
		m_bServerReady = false;
	}
}

void UHermesServerManager::LoadConfigFromFile()
{
	if (!FPaths::FileExists(m_configPath))
	{
		return;
	}
	
	m_serverExePath = GetConfigValue("Hermes", "executable",
								 TEXT("D:/LLM-NPC/LLM_NPC/Deploy/LLMServer/llama-server.exe"));
	m_modelPath = GetConfigValue("Hermes", "model",
								 TEXT("D:/LLM-NPC/LLM_NPC/Deploy/Models/Hermes-3-Llama-3.2-3B.Q4_K_M.gguf"));

	// Load from Hermes section
	m_hermesNGL = GetConfigValue("Hermes", "ngl", 40);
	m_hermesContext = GetConfigValue("Hermes", "context", 1024);
	m_hermesTokens = GetConfigValue("Hermes", "tokens", 128);
	m_hermesPort = GetConfigValue("Hermes", "port", 8080);
	m_hermesTemp = GetConfigValue("Hermes", "temperature", 0.7f);
	m_hermesBatch = GetConfigValue("Hermes", "batch", 128);
}

void UHermesServerManager::PollServerReady(int32 _attemptsLeft)
{
	if (_attemptsLeft <= 0)
	{
		return;
	}

	// Prüf ob Prozess überhaupt noch läuft
	if (!FPlatformProcess::IsProcRunning(m_serverProcessHandle))
	{
		int32 returnCode = -1;
		FPlatformProcess::GetProcReturnCode(m_serverProcessHandle, &returnCode);
		m_bServerRunning = false;
		return;
	}

	// Async HTTP /health check
	FString healthUrl = FString::Printf(TEXT("http://127.0.0.1:%d/health"), m_hermesPort);
	TSharedRef<IHttpRequest> req = FHttpModule::Get().CreateRequest();
	req->SetURL(healthUrl);
	req->SetVerb(TEXT("GET"));

	TWeakObjectPtr<UHermesServerManager> weakThis(this);

	req->OnProcessRequestComplete().BindLambda(
		[weakThis, _attemptsLeft](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
		{
			if (!weakThis.IsValid()) return;
			UHermesServerManager* self = weakThis.Get();

			if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
			{
				self->m_bServerReady = true;
				self->m_bServerRunning = true;
				return;
			}

			// Noch nicht bereit → in 1s nochmal versuchen
			if (UWorld* world = self->GetWorld())
			{
				world->GetTimerManager().SetTimer(
					self->m_startupPollTimer,
					[weakThis, _attemptsLeft]()
					{
						if (weakThis.IsValid())
							weakThis->PollServerReady(_attemptsLeft - 1);
					},
					1.0f,
					false
				);
			}
		});
	req->ProcessRequest();
}

FString UHermesServerManager::GetConfigValue(const FString& _section, const FString& _key,
                                             const FString& _defaultValue) const
{
	FString value = _defaultValue;

	FConfigFile configFile;
	configFile.Read(*m_configPath);
	configFile.GetString(*_section, *_key, value);

	return value;
}

int32 UHermesServerManager::GetConfigValue(const FString& _section, const FString& _key, int32 _defaultValue) const
{
	FString strValue = GetConfigValue(_section, _key, FString::FromInt(_defaultValue));
	return FCString::Atoi(*strValue);
}

float UHermesServerManager::GetConfigValue(const FString& _section, const FString& _key, float _defaultValue) const
{
	FString strValue = GetConfigValue(_section, _key, FString::SanitizeFloat(_defaultValue));
	return FCString::Atof(*strValue);
}
