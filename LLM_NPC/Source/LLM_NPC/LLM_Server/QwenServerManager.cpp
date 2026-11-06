#include "QwenServerManager.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

UQwenServerManager* UQwenServerManager::m_singletonInstance = nullptr;

void UQwenServerManager::Initialize(FSubsystemCollectionBase& _collection)
{
	Super::Initialize(_collection);
	if (m_bInitialized)
	{
		return;
	}

	if (m_singletonInstance != nullptr && m_singletonInstance != this)
	{
		m_bInitialized = true; // Mark as done aber nicht starten
		return;
	}

	m_configPath = TEXT("D:/LLM-NPC/LLM_NPC/Deploy/LLMServer/ServerConfig.ini");
	m_singletonInstance = this;
	m_bInitialized = true;

	LoadConfigFromFile();

	StartQwenServer();
}


void UQwenServerManager::Deinitialize()
{
	if (m_bShuttingDown) return;
	m_bShuttingDown = true;

	if (m_singletonInstance == this)
		m_singletonInstance = nullptr;

	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(m_startupPollTimer);

	StopQwenServer();
}

void UQwenServerManager::StartQwenServer()
{
	if (m_bServerRunning)
	{
		return;
	}
	
	const int32 parallel = 2;
	FString commandLine = FString::Printf(
		TEXT("-m \"%s\" -ngl %d --ctx-size %d -n %d --port %d --temp %f -b %d --parallel %d --cont-batching"),
		*m_modelPath,
		m_qwenNGL, // Aus Config!
		m_qwenContext * parallel, // Gesamt-Kontext = Config-Kontext × Slots
		m_qwenTokens, // Aus Config!
		m_qwenPort, // Aus Config!
		m_qwenTemp, // Aus Config!
		m_qwenBatch, // Aus Config!
		parallel
	);

	FString workingDir = FPaths::GetPath(m_serverExePath);
	
	m_serverProcessHandle = FPlatformProcess::CreateProc(
		*m_serverExePath,
		*commandLine,
		true,
		true,
		false,
		nullptr,
		0,
		*workingDir,
		nullptr
	);

	if (!m_serverProcessHandle.IsValid())
	{
		return;
	}

	PollServerReady(60);   
}

void UQwenServerManager::StopQwenServer()
{
	if (m_serverProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(m_serverProcessHandle);
		m_bServerRunning = false;
		m_bEnabled = false;
	}
}

void UQwenServerManager::PollServerReady(int32 _attemptsLeft)
{
	if (_attemptsLeft <= 0)
	{
		return;
	}

	if (!FPlatformProcess::IsProcRunning(m_serverProcessHandle))
	{
		int32 returnCode = -1;
		FPlatformProcess::GetProcReturnCode(m_serverProcessHandle, &returnCode);
		m_bServerRunning = false;
		return;
	}

	FString healthUrl = FString::Printf(TEXT("http://127.0.0.1:%d/health"), m_qwenPort);
	TSharedRef<IHttpRequest> req = FHttpModule::Get().CreateRequest();
	req->SetURL(healthUrl);
	req->SetVerb(TEXT("GET"));

	TWeakObjectPtr<UQwenServerManager> weakThis(this);

	req->OnProcessRequestComplete().BindLambda(
		[weakThis, _attemptsLeft]
	(FHttpRequestPtr, FHttpResponsePtr response, bool bSuccess)
		{
			if (!weakThis.IsValid()) return;
			UQwenServerManager* self = weakThis.Get();

			if (bSuccess && response.IsValid() && response->GetResponseCode() == 200)
			{
				self->m_bServerRunning = true;
				self->m_bEnabled = true;
				return;
			}

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

void UQwenServerManager::LoadConfigFromFile()
{

	if (!FPaths::FileExists(m_configPath))
	{
		return;
	}
	
	m_serverExePath = GetConfigValue("Qwen", "executable",
								 TEXT("D:/LLM-NPC/LLM_NPC/Deploy/LLMServer/llama-server.exe"));
	m_modelPath = GetConfigValue("Qwen", "model",
								 TEXT("D:/LLM-NPC/LLM_NPC/Deploy/Models/Hermes-3-Llama-3.2-3B.Q4_K_M.gguf"));

	// Load from Qwen section
	m_qwenNGL = GetConfigValue("Qwen", "ngl", 33);
	m_qwenContext = GetConfigValue("Qwen", "context", 512);
	m_qwenTokens = GetConfigValue("Qwen", "tokens", 64);
	m_qwenPort = GetConfigValue("Qwen", "port", 8081);
	m_qwenTemp = GetConfigValue("Qwen", "temperature", 0.3f);
	m_qwenBatch = GetConfigValue("Qwen", "batch", 64);

	UE_LOG(LogTemp, Warning,
	       TEXT("[Qwen] Config Loaded: ngl=%d, context=%d, tokens=%d, port=%d"),
	       m_qwenNGL, m_qwenContext, m_qwenTokens, m_qwenPort);
}

FString UQwenServerManager::GetConfigValue(const FString& _section, const FString& _key,
                                           const FString& _defaultValue) const
{
	FString value = _defaultValue;

	FConfigFile configFile;
	configFile.Read(*m_configPath);
	configFile.GetString(*_section, *_key, value);
	
	return value;
}

int32 UQwenServerManager::GetConfigValue(const FString& _section, const FString& _key, int32 _defaultValue) const
{
	FString strValue = GetConfigValue(_section, _key, FString::FromInt(_defaultValue));
	return FCString::Atoi(*strValue);
}

float UQwenServerManager::GetConfigValue(const FString& _section, const FString& _key, float _defaultValue) const
{
	FString strValue = GetConfigValue(_section, _key, FString::SanitizeFloat(_defaultValue));
	return FCString::Atof(*strValue);
}

void UQwenServerManager::EnableServer()
{
	if (!m_bEnabled)
	{
		m_bEnabled = true;
		StartQwenServer();
	}
}

void UQwenServerManager::DisableServer()
{
	if (m_bEnabled)
	{
		m_bEnabled = false;
		StopQwenServer();
	}
}
