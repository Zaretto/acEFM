// DirectInput8.h: interface for the CDirectInputJoystick class.
// Created by: Brad Blake	bradleyb@graduatecentre.com
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DIRECTINPUT8_H__C7F3B7B1_FF8E_4F74_8A80_10A7E827F1B8__INCLUDED_)
#define AFX_DIRECTINPUT8_H__C7F3B7B1_FF8E_4F74_8A80_10A7E827F1B8__INCLUDED_

#include <dinput.h>


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define KEYDOWN(name, key) (name[key] & 0x80)
#define KEYUP(name, key) (!(name[key] & 0x80))


#define D3DFVF_CV2D_T1D (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX0)


////////////////////////////////////////////////////////////

class CDirectInputJoystick  
{
public:

	//DI Device Objects
	LPDIRECTINPUTDEVICE8 m_pDIJoy;

	//Device State Buffers
	DIJOYSTATE2 m_js;
	char buffer[256];
	DWORD m_dwElements;

	//DI Object
	LPDIRECTINPUT8 m_pDI;

	//DI Device Capabilities
	DIDEVCAPS m_DIDevCaps;

	//Initialize Direct Input
	HRESULT InitDI(HWND hWnd, HINSTANCE hInstance);

	//Overridable Input Processing Methods
	virtual HRESULT ProcessJoy();

	//Release and Delete DI Objects
	void DIShutdown();


	//Construction/Destruction
	CDirectInputJoystick();
	virtual ~CDirectInputJoystick();

protected:

private:
	//Joystick callbacks
	friend BOOL __stdcall EnumAxesCallback( const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext );
	friend BOOL __stdcall EnumJoysticksCallback( const DIDEVICEINSTANCE* pdidInstance, VOID* pContext );	
};

//Global CDirectInputJoystick Instance
extern CDirectInputJoystick* g_pDI8;

#endif // !defined(AFX_DIRECTINPUT8_H__C7F3B7B1_FF8E_4F74_8A80_10A7E827F1B8__INCLUDED_)
