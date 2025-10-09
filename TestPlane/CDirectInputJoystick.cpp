// DirectInput8.cpp: implementation of the CDirectInputJoystick class.
// Created by: Brad Blake	bradleyb@graduatecentre.com
//////////////////////////////////////////////////////////////////////

// Include PCH here if required
#include "stdafx.h"
#include "CDirectInputJoystick.h"
#include "../flyt-EFM-dcsJSBSim/DCS_interface.h"

// Global Instance
CDirectInputJoystick* g_pDI8 = NULL;

// https://wiki.gel.ulaval.ca/index.php/Configuration_Joystick
CDirectInputJoystick::CDirectInputJoystick()
{
	g_pDI8			= this;
	m_pDI			= NULL;
	m_pDIJoy		= NULL;
	m_dwElements	= 16;

}

CDirectInputJoystick::~CDirectInputJoystick()
{
	DIShutdown();
}

/************************************************************************/
//Method:	InitDI
//Purpose:	Initialize Direct Input
/************************************************************************/
HRESULT CDirectInputJoystick::InitDI(HWND hWnd, HINSTANCE hInstance)
{	
	// Create The DI Object
	if(FAILED(DirectInput8Create(hInstance, DIRECTINPUT_VERSION, 
        IID_IDirectInput8, (void**)&m_pDI, NULL))) 
		return E_FAIL;
	
		if( FAILED(m_pDI->EnumDevices( DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY ) ) )
			return E_FAIL;

        if (!m_pDIJoy)
            return E_FAIL;

        if (FAILED(m_pDIJoy->SetDataFormat(&c_dfDIJoystick2)))
			return E_FAIL;

		//if( FAILED(m_pDIJoy->SetCooperativeLevel( hWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND ) ) )
		//	return E_FAIL;

		m_DIDevCaps.dwSize = sizeof(DIDEVCAPS);
		if ( FAILED(m_pDIJoy->GetCapabilities(&m_DIDevCaps) ) )
			return E_FAIL;

		if ( FAILED(m_pDIJoy->EnumObjects( EnumAxesCallback, (VOID*)hWnd, DIDFT_AXIS ) ) )
			return E_FAIL;

		//if(m_pDIJoy) m_pDIJoy->Acquire();

	return S_OK;
}


/************************************************************************/
//Method:	ProcessJoy
//Purpose:	Capture Joystick Input
/************************************************************************/
HRESULT CDirectInputJoystick::ProcessJoy()
{
    if (!m_pDIJoy)
        return E_FAIL;
	//Get joystick state and fill buffer
    if(FAILED(m_pDIJoy->Poll()))  
    {
        // DInput is telling us that the input stream has been
        // interrupted. We aren't tracking any state between polls, so
        // we don't have any special reset that needs to be done. We
        // just re-acquire and try again.
        HRESULT hr = m_pDIJoy->Acquire();
        if (hr == DIERR_INPUTLOST) return E_FAIL;
        //while (hr == DIERR_INPUTLOST) {
        //    hr = m_pDIJoy->Acquire();
        //}

        // If we encounter a fatal error, return failure.
        if ((hr == DIERR_INVALIDPARAM) || (hr == DIERR_NOTINITIALIZED)) {
            return E_FAIL;
        }

        // If another application has control of this device, return successfully.
        // We'll just have to wait our turn to use the joystick.
        if (hr == DIERR_OTHERAPPHASPRIO) {
            return S_OK;
        }
    }
    // Get the input's device state
    //if (FAILED(hr = joystick->GetDeviceState(sizeof(DIJOYSTATE), js))) {
    //    return hr; // The device should have been acquired during the Poll()
    //}
    if( FAILED(m_pDIJoy->GetDeviceState( sizeof(DIJOYSTATE2), &m_js ) ) )
        return E_FAIL;
    DIJOYSTATE2 *st = &m_js;
    //wprintf(L"Ax: (%4d,%4d,%4d) RAx: (%4d,%4d,%4d) Slider: (%4d,%4d)  nPov: (%4d,%4d,%4d,%4d)\n",
    //    st->lX, st->lY, st->lZ, st->lRx, st->lRy, st->lRz,
    //    st->rglSlider[0], st->rglSlider[1],
    //    st->rgdwPOV[0], st->rgdwPOV[1], st->rgdwPOV[2], st->rgdwPOV[3]
    //);
    ed_fm_set_command(2003, st->lRz / 1000.0);
    ed_fm_set_command(2004, st->lZ / 1000.0);
    ed_fm_set_command(2002, st->lX / 1000.0);
    ed_fm_set_command(2001, st->lY / 1000.0);


	return S_OK;
}

/************************************************************************/
//Method:	DIShutdown()
//Purpose:	Cleanup Objects
/************************************************************************/
void CDirectInputJoystick::DIShutdown()
{

	if (m_pDI)
	{
		if (m_pDIJoy)
		{
			m_pDIJoy->Unacquire();
			m_pDIJoy->Release();
			m_pDIJoy = NULL;
		}
		m_pDI->Release();
		m_pDI = NULL;
	}
	
}



/************************************************************************/
//Method:	EnumAxesCallback
//Purpose:	Joystick Axes Enumeration Callback
/************************************************************************/
BOOL __stdcall EnumAxesCallback(const DIDEVICEOBJECTINSTANCE *pdidoi, VOID *pContext)
{
	HWND hDlg = (HWND)pContext;

    wprintf(L"axes found %s (%x) ", pdidoi->tszName, pdidoi->dwDimension);
    RPC_STATUS status;
    if (IsEqualGUID(pdidoi->guidType, GUID_XAxis))
        printf("XAxis,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_YAxis))
        printf("YAxis,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_ZAxis))
        printf("ZAxis,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_RxAxis))
        printf("RxAxis,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_RyAxis))
        printf("RyAxis,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_RzAxis))
        printf("RzAxis,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_Slider))
        printf("Slider,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_Button))
        printf("Button,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_Key))
        printf("Key,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_POV))
        printf("POV,");
    else if (IsEqualGUID(pdidoi->guidType, GUID_Unknown))
        printf("Unknown,");
    printf("\n");
    DIPROPRANGE diprg; 
    diprg.diph.dwSize       = sizeof(DIPROPRANGE); 
    diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
    diprg.diph.dwHow        = DIPH_BYID; 
    diprg.diph.dwObj        = pdidoi->dwType; // Specify the enumerated axis
    diprg.lMin              = -1000; 
    diprg.lMax              = +1000; 
    
	// Set the range for the axis
	if( FAILED( g_pDI8->m_pDIJoy->SetProperty( DIPROP_RANGE, &diprg.diph ) ) )
		return DIENUM_STOP;

    return DIENUM_CONTINUE;
}

/************************************************************************/
//Method:	EnumJoysticksCallback
//Purpose:	Joystick Enumeration Callback
/************************************************************************/
BOOL __stdcall EnumJoysticksCallback(const DIDEVICEINSTANCE *pdidInstance, VOID *pContext)
{
	HRESULT hr;
    wprintf(L"found %s (%s)\n", pdidInstance->tszInstanceName, pdidInstance->tszProductName);
    hr = g_pDI8->m_pDI->CreateDevice( pdidInstance->guidInstance, &g_pDI8->m_pDIJoy, NULL );

    if( FAILED(hr) ) 
        return DIENUM_CONTINUE;

    return DIENUM_STOP;
}
