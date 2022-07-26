#include <Windows.h>
#include <detours.h>
#include <map>
#include "fp_call.h"

#define AttachDetour(pointer, detour) (DetourUpdateThread(GetCurrentThread()), DetourAttach(&(PVOID&)pointer, detour))
#define DetachDetour(pointer, detour) (DetourUpdateThread(GetCurrentThread()), DetourDetach(&(PVOID&)pointer, detour))

UINT_PTR gameBase = (UINT_PTR)GetModuleHandle("game.dll");
WNDPROC wndProc = NULL;
HMODULE thismodule = NULL;
std::map<UINT, BOOL> autoCastAbilityHotkeys;
std::map<UINT, BOOL> autoCastItemHotkeys;
std::map<UINT, UINT> itemHotkeys;
DWORD hotkey = NULL;
DWORD hotkeyTemp = NULL;
bool isDown = false;

UINT_PTR pCGameWar3 = gameBase + 0xab65f4; // 0xc4 = [49]

auto SetCursorMode = (BOOL(__thiscall*)(LPVOID currentmode, int abilID, int a3, int a4, int itemHandle, int a6))(gameBase + 0x37aaa0);
auto GetAbilityBaseUIById = (UINT(__thiscall*)(UINT abilId))(gameBase + 0x32c8e0);
auto GetWidgetUIDefById = (UINT(__thiscall*)(UINT id))(gameBase + 0x32c880);
auto KeyPressed = (UINT(__thiscall*)(UINT mode, DWORD keyEvent))(gameBase + 0x382150);
auto GetGameWindow = (HWND(__thiscall*)(LPVOID a1))(gameBase + 0x6bad70);
auto GetGameUI = (UINT(__fastcall*)(UINT a1, UINT a2))(gameBase + 0x300710);

auto SetJassState = (void(__thiscall*)(BOOL jassState))(gameBase + 0x2ab0e0);

HRESULT CALLBACK WndProcCustom(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL __fastcall SetCursorModeCustom(LPVOID currentmode, LPVOID, int abilID, int a3, int a4, UINT itemHandle, int a6);
//BOOL __fastcall KeyPressedCustom(UINT mode, LPVOID, DWORD keyEvent);
void __fastcall SetJassStateCustom(BOOL jassState);
bool CSimpleButtonPress(UINT pButton, uint32_t key);
bool CSimpleButtonPressEx(UINT pButton, UINT key);
UINT GetItemButton(int id);

bool IsInGame();

bool ValidVersion();

extern "C" void __stdcall SetAbilityQuickcast(UINT abilityId, BOOL state) {
	autoCastAbilityHotkeys[abilityId] = state;
}

extern "C" void __stdcall SetItemQuickcast(UINT itemId, BOOL state) {
	autoCastItemHotkeys[itemId] = state;
}

extern "C" void __stdcall SetItemHotkey(UINT index, UINT hotkey) {
	itemHotkeys[index] = hotkey;
}

//-----------------------------------------------

BOOL APIENTRY DllMain(HMODULE module, UINT reason, LPVOID reserved) {
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(module);

		if (!gameBase || !ValidVersion()) {
			return FALSE;
		}

		thismodule = module;

		DetourTransactionBegin();

		wndProc = (WNDPROC)SetWindowLong(GetGameWindow(NULL), GWL_WNDPROC, (LONG)WndProcCustom);
		//AttachDetour(KeyPressed, KeyPressedCustom);
		AttachDetour(SetCursorMode, SetCursorModeCustom);
		AttachDetour(SetJassState, SetJassStateCustom);

		DetourTransactionCommit();

		break;
	case DLL_PROCESS_DETACH:
		DetourTransactionBegin();

		SetWindowLong(GetGameWindow(NULL), GWL_WNDPROC, (LONG)wndProc);
		//DetachDetour(KeyPressed, KeyPressedCustom);
		DetachDetour(SetCursorMode, SetCursorModeCustom);
		DetachDetour(SetJassState, SetJassStateCustom);

		DetourTransactionCommit();

		break;
	}

	return TRUE;
}

//---------------------------------------------------

void Click() {
	mouse_event(MOUSEEVENTF_LEFTDOWN, NULL, NULL, 0, 0);
	mouse_event(MOUSEEVENTF_LEFTUP, NULL, NULL, 0, 0);
}

HRESULT CALLBACK WndProcCustom(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	hotkeyTemp = wParam;
	if (IsInGame()) {
		if (msg == WM_KEYDOWN || msg == WM_KEYUP) {
			if (IsInGame()) {
				isDown = (msg - 0x100) == 0;

				if (isDown) {
					if (hotkey == hotkeyTemp) {
						UINT gameUI = GetGameUI(0, 0);
						if (gameUI) {
							UINT targetMode = ((UINT*)gameUI)[132];
							if (targetMode) {
								if (((UINT*)targetMode)[1] == 3) {
									Click();
								}
							}
						}
					}

					for (UINT i = 0; i < 6; i++) {
						if (itemHotkeys[i] == wParam) {
							CSimpleButtonPressEx(GetItemButton(i), VK_LBUTTON);
						}
					}

					if (wParam == VK_SPACE) {
						return DefWindowProc(wnd, msg, wParam, lParam);
					}
				}
			}
		}
	}

	return CallWindowProc(wndProc, wnd, msg, wParam, lParam);
}

BOOL __fastcall SetCursorModeCustom(LPVOID currentmode, LPVOID, int abilID, int a3, int a4, UINT itemHandle, int a6) {
	BOOL retval = SetCursorMode(currentmode, abilID, a3, a4, itemHandle, a6);
	if ((abilID && autoCastAbilityHotkeys[abilID]) || (itemHandle && autoCastItemHotkeys[((UINT*)itemHandle)[12]])) {
		UINT gameUI = GetGameUI(0, 0);
		if (gameUI) {
			UINT targetMode = ((UINT*)gameUI)[132];
			if (targetMode) {
				if (((UINT*)targetMode)[1] == 3) {
					hotkey = hotkeyTemp;
					Click();
				}
			}
		}
	}
	else {
		hotkey = NULL;
	}
	// item + 0x30 = ID
	// cursor + 0x1b4 = CurrentMode
	// CurrentMode + 0x30 = victim
	// CurrentMode + 0xc = abilID
	// CurrentMode + 0x14 = ModeFlag (ModeFlag & 0x1000000) - AOE, (ModeFlag & 0x100000) - abil
	//printf("%.*s - %08X : Cursor: %08X\n", 4, (LPCSTR)&abilID, itemHandle, GetCursorGameUIByMode(currentmode));
	//cursor = GetCursorGameUIByMode(currentmode), targetAbilID = abilID, targetItemID = ((UINT*)itemHandle)[12];
	//printf("%.*s - %08X : Cursor: %08X\n", 4, (LPCSTR)&abilID, itemHandle, GetCursorGameUIByMode(currentmode));

	return retval;
}

//BOOL __fastcall KeyPressedCustom(UINT mode, LPVOID, DWORD keyEvent) {
//	hotkeyTemp = ((DWORD*)keyEvent)[4];
//	if (IsInGame() && hotkey == hotkeyTemp) {
//		UINT gameUI = GetGameUI(0, 0);
//		if (gameUI) {
//			UINT targetMode = ((UINT*)gameUI)[132];
//			if (targetMode) {
//				if (((UINT*)targetMode)[1] == 3) {
//					Click();
//				}
//			}
//		}
//	}
//
//	Beep(500, 200);
//
//	return KeyPressed(mode, keyEvent);
//}

void __fastcall SetJassStateCustom(BOOL jassState) {
	if (jassState == TRUE && thismodule) {
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)FreeLibrary, thismodule, NULL, NULL);
	}

	return SetJassState(jassState);
}

bool CSimpleButtonPress(UINT pButton, uint32_t key)
{
	return pButton ? this_call<UINT>(*(UINT*)(((UINT*)pButton)[0] + 0x6C), pButton, key) : false;
}

bool CSimpleButtonPressEx(UINT pButton, UINT key)
{
	bool result = false;
	if (pButton)
	{
		uint32_t oldflag = ((UINT*)pButton)[78];
		if (!(((UINT*)pButton)[78] & 0x2)) { ((UINT*)pButton)[78] |= 0x2; }
		result = CSimpleButtonPress(pButton, key);
		((UINT*)pButton)[78] = oldflag;
	}

	return result;
}

UINT GetItemButton(int id)
{
	UINT* gameui = (UINT*)GetGameUI(FALSE, FALSE);

	if (gameui != NULL)
	{
		int pGamePlayerPanelItems = gameui[241];

		if (pGamePlayerPanelItems != NULL)
		{
			int topLeftCommandButton = *(int*)(pGamePlayerPanelItems + 0x148);

			if (topLeftCommandButton != NULL)
			{
				topLeftCommandButton = *(int*)(topLeftCommandButton + 0x130);

				if (topLeftCommandButton != NULL)
				{
					topLeftCommandButton = *(int*)(topLeftCommandButton + 0x4);

					if (topLeftCommandButton != NULL)
					{
						return topLeftCommandButton + 0x1C0 * id;
					}
				}
			}
		}
	}

	return NULL;
}

//---------------------------------------------------

bool IsInGame() {
	return *(UINT*)pCGameWar3 && (*(UINT**)pCGameWar3)[49];
}

bool ValidVersion() {
	DWORD handle;
	DWORD size = GetFileVersionInfoSize("game.dll", &handle);

	LPSTR buffer = new char[size];
	GetFileVersionInfo("game.dll", handle, size, buffer);

	VS_FIXEDFILEINFO* verInfo;
	size = sizeof(VS_FIXEDFILEINFO);
	VerQueryValue(buffer, "\\", (LPVOID*)&verInfo, (UINT*)&size);
	delete[] buffer;

	return (((verInfo->dwFileVersionMS >> 16) & 0xffff) == 1 && (verInfo->dwFileVersionMS & 0xffff) == 26 && ((verInfo->dwFileVersionLS >> 16) & 0xffff) == 0 && ((verInfo->dwFileVersionLS >> 0) & 0xffff) == 6401);
}