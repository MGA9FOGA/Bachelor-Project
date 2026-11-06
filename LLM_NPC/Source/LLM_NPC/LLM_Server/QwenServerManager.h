#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "QwenServerManager.generated.h"

UCLASS()
class LLM_NPC_API UQwenServerManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& _collection) override;
	virtual void Deinitialize() override;
	
	UFUNCTION(BlueprintCallable, Category = "LLM")
	void EnableServer();
    
	UFUNCTION(BlueprintCallable, Category = "LLM")
	void DisableServer();
	
private:
	void StartQwenServer();
	void StopQwenServer();
	void PollServerReady(int32 _attemptsLeft);  
	void LoadConfigFromFile();
	
	FString GetConfigValue(const FString& _section, const FString& _key, const FString& _defaultValue) const;
	int32 GetConfigValue(const FString& _section, const FString& _key, int32 _defaultValue) const;
	float GetConfigValue(const FString& _section, const FString& _key, float _defaultValue) const;
	
	static UQwenServerManager* m_singletonInstance;
	
	FProcHandle m_serverProcessHandle;
	FTimerHandle m_startupPollTimer;  
	
	bool m_bServerRunning = false;
	bool m_bInitialized = false;
	bool m_bShuttingDown = false;
	bool m_bEnabled = false;
	
	int32 m_qwenNGL     = 20;
	int32 m_qwenContext = 512;
	int32 m_qwenTokens  = 64;
	int32 m_qwenPort    = 8081;
	float m_qwenTemp    = 0.2f;
	int32 m_qwenBatch   = 64;
	FString m_modelPath;
	FString m_serverExePath;
	FString m_configPath;
	
	
public:
	bool IsServerRunning() const { return m_bServerRunning; }
	
	int32 GetPort()        const { return m_qwenPort; }
	int32 GetMaxTokens()   const { return m_qwenTokens; }
	float GetTemperature() const { return m_qwenTemp; }
	int32 GetBatch()       const { return m_qwenBatch; }
	int32 GetContext()     const { return m_qwenContext; }
};
