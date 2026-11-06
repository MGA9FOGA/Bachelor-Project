#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HermesServerManager.generated.h"

UCLASS()
class LLM_NPC_API UHermesServerManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& _collection) override;
	virtual void Deinitialize() override;
	
private:
	void StartHermesServer();
	void StopHermesServer();
	void LoadConfigFromFile();
	
	void PollServerReady(int32 _attemptsLeft);
	
	FString GetConfigValue(const FString& _section, const FString& _key, const FString& _defaultValue) const;
	int32 GetConfigValue(const FString& _section, const FString& _key, int32 _defaultValue) const;
	float GetConfigValue(const FString& _section, const FString& _key, float _defaultValue) const;
	
	FProcHandle m_serverProcessHandle;
	FTimerHandle m_startupPollTimer;
	
	static UHermesServerManager* m_singletonInstance;
	

	bool m_bServerRunning = false;
	bool m_bServerReady	 = false;
	bool m_bShuttingDown = false;
	
	int32  m_hermesNGL     = 40;
	int32  m_hermesContext = 2048;
	int32  m_hermesTokens  = 256;
	int32  m_hermesPort    = 8080;
	int32  m_hermesBatch   = 128;
	float  m_hermesTemp    = 0.7f;
	
	FString m_serverExePath;
	FString m_modelPath;
	FString m_configPath;
	
public:
	bool IsServerRunning() const { return m_bServerRunning; }
	bool IsServerReady()   const { return m_bServerReady; }

	// Config-Getter für Worker
	int32 GetPort()        const { return m_hermesPort; }
	int32 GetMaxTokens()   const { return m_hermesTokens; }
	float GetTemperature() const { return m_hermesTemp; }
	int32 GetBatch()       const { return m_hermesBatch; }
	int32 GetContext()     const { return m_hermesContext; }
};
