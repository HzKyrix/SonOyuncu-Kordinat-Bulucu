#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <TlHelp32.h>

// ============================================================
//  BELLEK OKUMA ARAÇLARI
// ============================================================

DWORD GetPID(const wchar_t* procName) {
    DWORD pid = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (lstrcmpiW(pe.szExeFile, procName) == 0) { pid = pe.th32ProcessID; break; }
            } while (Process32NextW(hSnap, &pe));
        }
    }
    CloseHandle(hSnap);
    return pid;
}

uintptr_t GetModuleBase(DWORD pid, const wchar_t* modName) {
    uintptr_t addr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me; me.dwSize = sizeof(me);
        if (Module32FirstW(hSnap, &me)) {
            do {
                if (lstrcmpiW(me.szModule, modName) == 0) { addr = (uintptr_t)me.modBaseAddr; break; }
            } while (Module32NextW(hSnap, &me));
        }
    }
    CloseHandle(hSnap);
    return addr;
}

uintptr_t ReadPointerChain(HANDLE hProc, uintptr_t base, std::vector<unsigned int> offsets) {
    uintptr_t addr = base;
    for (unsigned int i = 0; i < offsets.size(); i++) {
        if (!ReadProcessMemory(hProc, (LPCVOID)addr, &addr, sizeof(addr), NULL)) return 0;
        addr += offsets[i];
    }
    return addr;
}

// ============================================================
//  AYARLAR VE KOORDİNAT SİSTEMİ
// ============================================================

HANDLE hProcess = NULL;
uintptr_t openALBase = 0;

struct CoordResult { float x, y, z; bool valid; };

// Senin bulduğun stabil pointer yolu
const uintptr_t PTR_OFFSET = 0x001D9050;
const std::vector<unsigned int> PTR_OFFSETS = { 0x0, 0x60 };

CoordResult readCoordinates() {
    CoordResult res = { 0,0,0,false };
    if (!hProcess || !openALBase) return res;

    uintptr_t xAddr = ReadPointerChain(hProcess, openALBase + PTR_OFFSET, PTR_OFFSETS);

    if (xAddr != 0) {
        ReadProcessMemory(hProcess, (LPCVOID)xAddr, &res.x, sizeof(float), NULL);
        ReadProcessMemory(hProcess, (LPCVOID)(xAddr + 4), &res.y, sizeof(float), NULL);
        ReadProcessMemory(hProcess, (LPCVOID)(xAddr + 8), &res.z, sizeof(float), NULL);

        // --- YÜKSEKLİK DÜZELTMESİ ---
        // Eğer 1 blok yüksek gösteriyorsa kafa hizasını okuyordur.
        // Karakter boyu yaklaşık 1.62 birimdir. 1 blok fark için bunu çıkarıyoruz.
        res.y -= 1.62f;

        if (res.y > -200 && res.y < 500) res.valid = true;
    }
    return res;
}

// ============================================================
//  MAIN
// ============================================================

int main() {
    SetConsoleTitleA("Kyrix Client - Kordinat Okuyucu");

    std::wcout << L"SonOyuncu bekleniyor..." << std::endl;

    DWORD pid = GetPID(L"sonoyuncuclient.exe");
    while (pid == 0) {
        pid = GetPID(L"sonoyuncuclient.exe");
        Sleep(500);
    }

    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    openALBase = GetModuleBase(pid, L"OpenAL.dll");

    if (!openALBase) {
        std::cout << "Hata: OpenAL.dll bulunamadi!" << std::endl;
        system("pause"); return 1;
    }

    std::cout << "---------------------------------------------------" << std::endl;
    std::cout << "Kyrix SonOyuncu Kordinat Bulucu" << std::endl;
    std::cout << "---------------------------------------------------" << std::endl;

    while (true) {
        CoordResult c = readCoordinates();

        if (c.valid) {
            // %.2f virgülden sonra 2 basamak gösterir
            printf("\rX: %.2f | Y: %.2f | Z: %.2f        ", c.x, c.y, c.z);
        }
        else {
            printf("\rKordinat yolu bekleniyor...          ");
        }

        if (GetAsyncKeyState(VK_F8)) break; // F8 ile çıkış
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (hProcess) CloseHandle(hProcess);
    return 0;
}