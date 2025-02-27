﻿#include "pch.h"
#include "util.h"
#include "il2cpp-init.h"

using Genshin::ByteArray, Genshin::ClientKcpEvent, Genshin::KcpPacket, Genshin::KcpEventType;
using std::to_string;

HWND unityWnd = nullptr;
HANDLE hPipe  = nullptr;

// Allow Protocol: GetPlayerTokenRsp, PlayerLoginRsp, AchievementAllDataNotify, PingRsp
std::set<UINT16> PacketWhitelist = { 2407, 29665, 27422, 8231 };

bool OnPacket(KcpPacket* pkt) {
	if (pkt->data == nullptr) return true;
	auto len = pkt->length;
	auto data = (ByteArray*)new BYTE[len + 32];
	data->max_length = len;
	memcpy(data->vector, pkt->data, len);
	Genshin::XorEncrypt(&data, len, nullptr);
	if (ReadMapped<UINT16>(data->vector, 0) != 0x4567) {
		delete[] data;
		return true;
	}
	if (!PacketWhitelist.contains(ReadMapped<UINT16>(data->vector, 2))) {
		//ifdef _DEBUG
		printf("Blocked cmdid: %d\n", ReadMapped<UINT16>(data->vector, 2));
		//endif
		delete[] data;
		return false;
	}
	printf("Passed cmdid: %d\n", ReadMapped<UINT16>(data->vector, 2));
	if (ReadMapped<UINT16>(data->vector, 2) == 27422) {
		const auto headLength = ReadMapped<UINT16>(data->vector, 4);
		const auto dataLength = ReadMapped<UINT32>(data->vector, 6);
		const auto cStr = base64_encode(data->vector + 10 + headLength, dataLength) + "\n";
		WriteFile(hPipe, cStr.c_str(), cStr.length(), nullptr, nullptr);
		CloseHandle(hPipe);
		auto manager = Genshin::GetSingletonInstance(Genshin::GetSingletonManager(), il2cpp_string_new("GameManager"));
		Genshin::ForceQuit(manager);
	}
	delete[] data;
	return true;
}

std::string checksum;

namespace Hook {

	void SetVersion(void* obj, Il2CppString* value, void* method) {
		const auto version = ToString(value);
		value = string_new(version + " YaeAchievement");
		CALL_ORIGIN(SetVersion, obj, value, method);
	}

	bool KcpRecv(void* client, ClientKcpEvent* evt, void* method) {
		const auto result = CALL_ORIGIN(KcpRecv, client, evt, method);
		if (result == 0 || evt->fields.type != KcpEventType::EventRecvMsg) {
			return result;
		}
		return OnPacket(evt->fields.packet) ? result : false;
	}

	ByteArray* UnityEngine_RecordUserData(INT type) {
		return new ByteArray {};
	}
	// 不再使用checksum(?
}

void Run(HMODULE* phModule) {
	//AllocConsole();
	//freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	while ((unityWnd = FindMainWindowByPID(GetCurrentProcessId())) == nullptr) {
		Sleep(1000);
	}
	Sleep(5000);
	DisableVMProtect();
	InitIL2CPP();
	HookManager::install(Genshin::KcpRecv, Hook::KcpRecv);
	HookManager::install(Genshin::SetVersion, Hook::SetVersion);
	HookManager::install(Genshin::UnityEngine_RecordUserData, Hook::UnityEngine_RecordUserData);
	hPipe = CreateFile(R"(\\.\pipe\YaeAchievementPipe)", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hPipe == INVALID_HANDLE_VALUE) {
		Win32ErrorDialog(1001);
		ExitProcess(0);
	}
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReasonForCall, LPVOID lpReserved) {
	switch (ulReasonForCall) {
	case DLL_PROCESS_ATTACH:
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Run, new HMODULE(hModule), 0, NULL);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
