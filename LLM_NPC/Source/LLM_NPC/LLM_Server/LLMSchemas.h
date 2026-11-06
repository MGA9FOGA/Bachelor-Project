#pragma once

#include "CoreMinimal.h"

/**
 * Zentrale JSON-Schemas für die grammar-constrained LLM-Ausgabe. Wird als
 * FLLMRequest::JsonSchema mitgeschickt; llama.cpp zwingt die Ausgabe dann in genau
 * diese Form → das Parsing kann nie mehr an ungültigem JSON scheitern.
 *
 * (Als kompakte JSON-Strings gehalten; die Worker parsen sie in ein Objekt.)
 */
namespace LLMSchemas
{
	// Spieler-Dialog: Reply/Intent/Mood + optionale Quest.
	// Die Quest ist ein oneOf mit "kind"-Diskriminator (const): so erzwingt die Grammatik
	// je Art die richtigen Pflichtfelder (fetch: item+count / delivery: target) statt sie
	// nur "erlauben", sonst lässt das kleine Modell item/target gern weg. "description"
	// (längstes Freitextfeld) steht bewusst ZULETZT, damit die strukturierten Felder auch
	// bei knappem Token-Budget schon vollständig sind.
	inline FString Dialogue()
	{
		return TEXT("{\"type\":\"object\",\"properties\":{"
			"\"reply\":{\"type\":\"string\"},"
			"\"intent\":{\"type\":\"string\",\"enum\":[\"neutral\",\"greeting\",\"trade\",\"hostile\",\"quest\"]},"
			"\"mood\":{\"type\":\"string\",\"enum\":[\"neutral\",\"friendly\",\"suspicious\",\"angry\"]},"
			"\"quest\":{\"oneOf\":["
				"{\"type\":\"object\",\"properties\":{"
					"\"kind\":{\"const\":\"fetch\"},"
					"\"item\":{\"type\":\"string\"},\"count\":{\"type\":\"integer\"},"
					"\"title\":{\"type\":\"string\"},\"reward\":{\"type\":\"integer\"},"
					"\"description\":{\"type\":\"string\"}},"
					"\"required\":[\"kind\",\"item\",\"count\",\"title\",\"reward\",\"description\"]},"
				"{\"type\":\"object\",\"properties\":{"
					"\"kind\":{\"const\":\"delivery\"},"
					"\"target\":{\"type\":\"string\"},"
					"\"title\":{\"type\":\"string\"},\"reward\":{\"type\":\"integer\"},"
					"\"description\":{\"type\":\"string\"}},"
					"\"required\":[\"kind\",\"target\",\"title\",\"reward\",\"description\"]}"
			"]}"
			"},\"required\":[\"reply\",\"intent\",\"mood\"]}");
	}

	// Tagesplan: Liste aus {hour, action}.
	inline FString Schedule()
	{
		return TEXT("{\"type\":\"object\",\"properties\":{"
			"\"schedule\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{"
				"\"hour\":{\"type\":\"integer\"},"
				"\"action\":{\"type\":\"string\",\"enum\":[\"work\",\"eat\",\"drink\",\"rest\",\"wander\",\"idle\",\"sleep\",\"shop\"]}},"
				"\"required\":[\"hour\",\"action\"]}}"
			"},\"required\":[\"schedule\"]}");
	}

	// NPC↔NPC-Gespräch: Dialog + Ausgang + Grund + verdichtete Erinnerung.
	inline FString Conversation()
	{
		return TEXT("{\"type\":\"object\",\"properties\":{"
			"\"dialogue\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{"
				"\"speaker\":{\"type\":\"string\"},\"text\":{\"type\":\"string\"}},"
				"\"required\":[\"speaker\",\"text\"]}},"
			"\"outcome\":{\"type\":\"string\",\"enum\":[\"positive\",\"neutral\",\"negative\"]},"
			"\"reason\":{\"type\":\"string\"},"
			"\"memory\":{\"type\":\"string\"}"
			"},\"required\":[\"dialogue\",\"outcome\"]}");
	}

	// Reflexion: Liste kurzer Einsichten.
	inline FString Reflection()
	{
		return TEXT("{\"type\":\"object\",\"properties\":{"
			"\"reflections\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}"
			"},\"required\":[\"reflections\"]}");
	}

	// Meinung über eine Person: ein Satz.
	inline FString Opinion()
	{
		return TEXT("{\"type\":\"object\",\"properties\":{"
			"\"opinion\":{\"type\":\"string\"}"
			"},\"required\":[\"opinion\"]}");
	}

	// Proaktive Begrüßung (NPC eröffnet das Gespräch): nur ein kurzer Satz.
	inline FString Greeting()
	{
		return TEXT("{\"type\":\"object\",\"properties\":{"
			"\"reply\":{\"type\":\"string\"}"
			"},\"required\":[\"reply\"]}");
	}
}
