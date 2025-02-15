/*
    This file is part of D2DX.

    Copyright (C) 2021  Bolrog

    D2DX is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    D2DX is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with D2DX.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "pch.h"
#include "Detours.h"
#include "Utils.h"
#include "D2DXContextFactory.h"
#include "IWin32InterceptionHandler.h"
#include "IGameHelper.h"

using namespace d2dx;

#pragma comment(lib, "../../thirdparty/detours/detours.lib")

bool hasDetoured = false;
bool hasLateDetoured = false;

static IWin32InterceptionHandler* GetWin32InterceptionHandler()
{
    ID2DXContext* d2dxContext = D2DXContextFactory::GetInstance();

    if (!d2dxContext)
    {
        return nullptr;
    }

    return dynamic_cast<IWin32InterceptionHandler*>(d2dxContext);
}

static ID2GfxInterceptionHandler* GetD2GfxInterceptionHandler()
{
    ID2DXContext* d2dxContext = D2DXContextFactory::GetInstance();

    if (!d2dxContext)
    {
        return nullptr;
    }

    return dynamic_cast<ID2GfxInterceptionHandler*>(d2dxContext);
}

COLORREF(WINAPI* GetPixel_real)(
    _In_ HDC hdc,
    _In_ int x,
    _In_ int y) = GetPixel;

COLORREF WINAPI GetPixel_Hooked(_In_ HDC hdc, _In_ int x, _In_ int y)
{
    /* Gets rid of the long delay on startup and when switching between menus in < 1.14,
       as the game is doing a ton of pixel readbacks... for some reason. */
    return 0;
}

int (WINAPI* ShowCursor_Real)(
    _In_ BOOL bShow) = ShowCursor;

int WINAPI ShowCursor_Hooked(
    _In_ BOOL bShow)
{
    /* Override how the game hides/shows the cursor. We will take care of that. */
    return bShow ? 1 : -1;
}

BOOL(WINAPI* SetWindowPos_Real)(
    _In_ HWND hWnd,
    _In_opt_ HWND hWndInsertAfter,
    _In_ int X,
    _In_ int Y,
    _In_ int cx,
    _In_ int cy,
    _In_ UINT uFlags) = SetWindowPos;

BOOL
WINAPI
SetWindowPos_Hooked(
    _In_ HWND hWnd,
    _In_opt_ HWND hWndInsertAfter,
    _In_ int X,
    _In_ int Y,
    _In_ int cx,
    _In_ int cy,
    _In_ UINT uFlags)
{
    /* Stop the game from moving/sizing the window. */
    return SetWindowPos_Real(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags | SWP_NOSIZE | SWP_NOMOVE);
}

BOOL
(WINAPI*
    SetCursorPos_Real)(
        _In_ int X,
        _In_ int Y) = SetCursorPos;

BOOL
WINAPI
SetCursorPos_Hooked(
    _In_ int X,
    _In_ int Y)
{
    auto win32InterceptionHandler = GetWin32InterceptionHandler();

    if (!win32InterceptionHandler)
    {
        return FALSE;
    }

    auto adjustedPos = win32InterceptionHandler->OnSetCursorPos({ X, Y });
    
    if (adjustedPos.x < 0)
    {
        return FALSE;
    }

    return SetCursorPos_Real(adjustedPos.x, adjustedPos.y);
}

LRESULT
(WINAPI*
    SendMessageA_Real)(
        _In_ HWND hWnd,
        _In_ UINT Msg,
        _Pre_maybenull_ _Post_valid_ WPARAM wParam,
        _Pre_maybenull_ _Post_valid_ LPARAM lParam) = SendMessageA;

LRESULT
WINAPI
SendMessageA_Hooked(
    _In_ HWND hWnd,
    _In_ UINT Msg,
    _Pre_maybenull_ _Post_valid_ WPARAM wParam,
    _Pre_maybenull_ _Post_valid_ LPARAM lParam)
{
    if (Msg == WM_MOUSEMOVE)
    {
        auto win32InterceptionHandler = GetWin32InterceptionHandler();

        if (!win32InterceptionHandler)
        {
            return 0;
        }

        auto x = GET_X_LPARAM(lParam);
        auto y = GET_Y_LPARAM(lParam);

        auto adjustedPos = win32InterceptionHandler->OnMouseMoveMessage({ x, y });

        if (adjustedPos.x < 0)
        {
            return 0;
        }

        lParam = ((uint16_t)adjustedPos.y << 16) | ((uint16_t)adjustedPos.x & 0xFFFF);
    }

    return SendMessageA_Real(hWnd, Msg, wParam, lParam);
}

_Success_(return)
int
WINAPI
DrawTextA_Hooked(
    _In_ HDC hdc,
    _When_((format & DT_MODIFYSTRING), _At_((LPSTR)lpchText, _Inout_grows_updates_bypassable_or_z_(cchText, 4)))
    _When_((!(format & DT_MODIFYSTRING)), _In_bypassable_reads_or_z_(cchText))
    LPCSTR lpchText,
    _In_ int cchText,
    _Inout_ LPRECT lprc,
    _In_ UINT format)
{
    /* This removes the "weird characters" being printed by the game in the top left corner.
       There is still a delay but the GetPixel hook takes care of that... */
    return 0;
}


_Success_(return)
int(
    WINAPI *
    DrawTextA_Real)(
        _In_ HDC hdc,
        _When_((format & DT_MODIFYSTRING), _At_((LPSTR)lpchText, _Inout_grows_updates_bypassable_or_z_(cchText, 4)))
        _When_((!(format & DT_MODIFYSTRING)), _In_bypassable_reads_or_z_(cchText))
        LPCSTR lpchText,
        _In_ int cchText,
        _Inout_ LPRECT lprc,
        _In_ UINT format) = DrawTextA;


struct CellContext		//size 0x48
{
    DWORD nCellNo;					//0x00
    DWORD _0a;						//0x04
    DWORD dwUnit;					//0x08
    DWORD dwClass;					//0x0C
    DWORD dwMode;					//0x10
    DWORD _3;						//0x14
    DWORD _4;						//0x18
    BYTE _5;						//0x1C
    BYTE _5a;						//0x1D
    WORD _6;						//0x1E
    DWORD _7;						//0x20
    DWORD _8;						//0x24
    DWORD _9;						//0x28
    char* szName;					//0x2C
    DWORD _11;						//0x30
    void* pCellFile;			//0x34 also pCellInit
    DWORD _12;						//0x38
    void* pGfxCells;				//0x3C
    DWORD direction;				//0x40
    DWORD _14;						//0x44
};
typedef void (__stdcall* D2Gfx_DrawImageFunc)(CellContext* pData, int nXpos, int nYpos, DWORD dwGamma, int nDrawMode, BYTE* pPalette);
typedef void(__stdcall* D2Gfx_DrawShadowFunc)(CellContext* pData, int nXpos, int nYpos);
typedef void(__fastcall* D2Win_DrawTextFunc)(const wchar_t* wStr, int xPos, int yPos, DWORD dwColor, DWORD dwUnk);

D2Gfx_DrawImageFunc D2Gfx_DrawImage_Real = nullptr;
D2Gfx_DrawShadowFunc D2Gfx_DrawShadow_Real = nullptr;
D2Win_DrawTextFunc D2Win_DrawText_Real = nullptr;

Offset playerScreenPos = { 0, 0 };

bool drawingText = false;

#pragma optimize("", off)
__declspec(noinline) void __stdcall  D2Gfx_DrawImage_Hooked(CellContext* pData, int nXpos, int nYpos, DWORD dwGamma, int nDrawMode, BYTE* pPalette)
{
    /*
   D2DX_LOG("draw image %i, %i: '%p' nDrawMode %i, class %u, unit %u, dwMode %u, %p, %p, %p, %p, %p, %p", nXpos, nYpos, pData->szName, nDrawMode, pData->dwClass, pData->dwUnit,
        pData->dwMode,
        pData->_11,
        pData->_12,
        pData->_14, 
        pData->_7,
        pData->_8,
        pData->_9);*/

    auto d2GfxInterceptionHandler = GetD2GfxInterceptionHandler();
    
    if (d2GfxInterceptionHandler && !drawingText)
    {
        TextureCategory textureCategory = TextureCategory::Unknown;

        if (pData->dwClass > 0 && pData->dwUnit == 0)
        {
            // The player unit itself.
            textureCategory = TextureCategory::Player;
            playerScreenPos = { nXpos, nYpos };
        }
        else if (playerScreenPos.x > 0 &&
            nXpos == playerScreenPos.x &&
            nYpos == playerScreenPos.y)
        {
            // Overlays will be drawn at the player position, so mark them as part of the player.
            textureCategory = TextureCategory::Player;
        }
        else if (
            pData->dwClass == 0 &&
            pData->dwUnit == 0 &&
            pData->dwMode == 0)
        {
            textureCategory = TextureCategory::UserInterface;
        }

        if (textureCategory != TextureCategory::Unknown)
        {
            d2GfxInterceptionHandler->SetTextureCategory(textureCategory);
        }
    }

    D2Gfx_DrawImage_Real(pData, nXpos, nYpos, dwGamma, nDrawMode, pPalette);

    if (d2GfxInterceptionHandler && !drawingText)
    {
        d2GfxInterceptionHandler->SetTextureCategory(TextureCategory::Unknown);
    }
}

#pragma optimize("", off)
__declspec(noinline) void __stdcall  D2Gfx_DrawShadow_Hooked(CellContext* pData, int nXpos, int nYpos)
{
    //D2DX_LOG("draw shadow %i, %i: '%p' class %u, unit %u, dwMode %u, %p, %p, %p, %p, %p, %p", nXpos, nYpos, pData->szName, pData->dwClass, pData->dwUnit,
    //    pData->dwMode,
    //    pData->_9,
    //    pData->_12,
    //    pData->_14,
    //    pData->_3,
    //    pData->_4,
    //    pData->_7);

    auto d2GfxInterceptionHandler = GetD2GfxInterceptionHandler();

    if (d2GfxInterceptionHandler)
    {
        TextureCategory textureCategory = TextureCategory::Unknown;

        if (pData->dwClass > 0 && pData->dwUnit == 0)
        {
            textureCategory = TextureCategory::Player;
        }

        if (textureCategory != TextureCategory::Unknown)
        {
            d2GfxInterceptionHandler->SetTextureCategory(textureCategory);
        }
    }

    D2Gfx_DrawShadow_Real(pData, nXpos, nYpos);

    if (d2GfxInterceptionHandler)
    {
        d2GfxInterceptionHandler->SetTextureCategory(TextureCategory::Unknown);
    }
}

#pragma optimize("", off)
__declspec(noinline) void __fastcall D2Win_DrawText_Hooked(const wchar_t* wStr, int xPos, int yPos, DWORD dwColor, DWORD dwUnk)
{
    auto d2GfxInterceptionHandler = GetD2GfxInterceptionHandler();

    if (d2GfxInterceptionHandler)
    {
        d2GfxInterceptionHandler->SetTextureCategory(TextureCategory::UserInterface);
    }

    drawingText = true;

    D2Win_DrawText_Real(wStr, xPos, yPos, dwColor, dwUnk);

    drawingText = false;

    if (d2GfxInterceptionHandler)
    {
        d2GfxInterceptionHandler->SetTextureCategory(TextureCategory::Unknown);
    }
}

void d2dx::AttachDetours()
{
    if (hasDetoured)
    {
        return;
    }

    hasDetoured = true;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)DrawTextA_Real, DrawTextA_Hooked);
    DetourAttach(&(PVOID&)GetPixel_real, GetPixel_Hooked);
    DetourAttach(&(PVOID&)SendMessageA_Real, SendMessageA_Hooked);
    DetourAttach(&(PVOID&)ShowCursor_Real, ShowCursor_Hooked);
    DetourAttach(&(PVOID&)SetCursorPos_Real, SetCursorPos_Hooked);
    auto r = DetourAttach(&(PVOID&)SetWindowPos_Real, SetWindowPos_Hooked);

    LONG lError = DetourTransactionCommit();

    if (lError != NO_ERROR) {
        D2DX_FATAL_ERROR("Failed to detour Win32 functions.");
    }
}

_Use_decl_annotations_
void d2dx::AttachLateDetours(
    IGameHelper* gameHelper)
{
    if (hasLateDetoured)
    {
        return;
    }

    hasLateDetoured = true;

    D2Gfx_DrawImage_Real = (D2Gfx_DrawImageFunc)gameHelper->GetFunction(D2Function::D2Gfx_DrawImage);
    D2Gfx_DrawShadow_Real = (D2Gfx_DrawShadowFunc)gameHelper->GetFunction(D2Function::D2Gfx_DrawShadow);
    D2Win_DrawText_Real =  (D2Win_DrawTextFunc)gameHelper->GetFunction(D2Function::D2Win_DrawText);

    if (!D2Gfx_DrawImage_Real && !D2Gfx_DrawShadow_Real && !D2Win_DrawText_Real)
    {
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (D2Gfx_DrawImage_Real)
    {
        DetourAttach(&(PVOID&)D2Gfx_DrawImage_Real, D2Gfx_DrawImage_Hooked);
    }

    if (D2Gfx_DrawShadow_Real)
    {
        DetourAttach(&(PVOID&)D2Gfx_DrawShadow_Real, D2Gfx_DrawShadow_Hooked);
    }

    if (D2Win_DrawText_Real)
    {
        DetourAttach(&(PVOID&)D2Win_DrawText_Real, D2Win_DrawText_Hooked);
    }

    LONG lError = DetourTransactionCommit();

    if (lError != NO_ERROR) {
        D2DX_FATAL_ERROR("Failed to detour D2Gfx functions.");
    }
}

void d2dx::DetachDetours()
{
    if (!hasDetoured)
    {
        return;
    }

    hasDetoured = false;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)DrawTextA_Real, DrawTextA_Hooked);
    DetourDetach(&(PVOID&)GetPixel_real, GetPixel_Hooked);
    DetourDetach(&(PVOID&)SendMessageA_Real, SendMessageA_Hooked);
    DetourDetach(&(PVOID&)ShowCursor_Real, ShowCursor_Hooked);
    DetourDetach(&(PVOID&)SetCursorPos_Real, SetCursorPos_Hooked);
    DetourDetach(&(PVOID&)SetWindowPos_Real, SetWindowPos_Hooked);

    LONG lError = DetourTransactionCommit();

    if (lError != NO_ERROR) {
        /* An error here doesn't really matter. The process is going. */
    }
}
