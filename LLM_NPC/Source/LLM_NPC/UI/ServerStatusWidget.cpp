#include "ServerStatusWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "LLM_NPC/LLM_Server/HermesServerManager.h"
#include "LLM_NPC/LLM_Server/QwenServerManager.h"

void UServerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyColors();
	PollHealth();

	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimer(
			m_pollTimer, this, &UServerStatusWidget::PollHealth,
			UpdateInterval, /*loop*/ true);
	}
}

void UServerStatusWidget::NativeDestruct()
{
	if (UWorld* world = GetWorld())
		world->GetTimerManager().ClearTimer(m_pollTimer);

	Super::NativeDestruct();
}

void UServerStatusWidget::PollHealth()
{
	int32 hermesPort = 8080;
	int32 qwenPort   = 8081;

	if (UGameInstance* gi = GetGameInstance())
	{
		if (UHermesServerManager* h = gi->GetSubsystem<UHermesServerManager>())
			hermesPort = h->GetPort();
		if (UQwenServerManager* q = gi->GetSubsystem<UQwenServerManager>())
			qwenPort = q->GetPort();
	}

	CheckServerHealth(hermesPort, /*bHermes*/ true);
	CheckServerHealth(qwenPort,   /*bHermes*/ false);
}

void UServerStatusWidget::CheckServerHealth(int32 _port, bool _bHermes)
{
	const FString url = FString::Printf(TEXT("http://127.0.0.1:%d/health"), _port);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> req = FHttpModule::Get().CreateRequest();
	req->SetURL(url);
	req->SetVerb(TEXT("GET"));
	req->SetTimeout(2.0f);

	TWeakObjectPtr<UServerStatusWidget> weakThis(this);

	req->OnProcessRequestComplete().BindLambda(
		[weakThis, _bHermes](FHttpRequestPtr, FHttpResponsePtr _response, bool _bOk)
		{
			if (!weakThis.IsValid()) return;
			UServerStatusWidget* self = weakThis.Get();

			// llama-server: 200 = bereit, 503 = laedt noch, kein Response = aus
			const bool bOnline =
				_bOk && _response.IsValid() && _response->GetResponseCode() == 200;

			if (_bHermes) self->m_bHermesOnline = bOnline;
			else          self->m_bQwenOnline   = bOnline;

			self->ApplyColors();
		});

	req->ProcessRequest();
}

void UServerStatusWidget::ApplyColors()
{
	if (ImgHermes) ImgHermes->SetColorAndOpacity(m_bHermesOnline ? OnlineColor : OfflineColor);
	if (ImgQwen)   ImgQwen->SetColorAndOpacity(m_bQwenOnline   ? OnlineColor : OfflineColor);

	if (TxtHermes)
		TxtHermes->SetText(FText::FromString(
			m_bHermesOnline ? TEXT("Hermes: Online") : TEXT("Hermes: Offline")));
	if (TxtQwen)
		TxtQwen->SetText(FText::FromString(
			m_bQwenOnline ? TEXT("Qwen: Online") : TEXT("Qwen: Offline")));
}
