//windows entry point
#include "defines.h"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>

#include <stdio.h>
#include <math.h>

struct win32_offscreen_buffer{
	BITMAPINFO info;
	void* memory;
	int width;
	int height;
	int pitch; //possible row offset
	int bytesPerPixel;
};

//TODO temporary globals
global_variable b32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

struct win32_window_dimensions{
	int width;
	int height;
};

//XInputGetState, this initializes to a stub pointer
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub){return ERROR_DEVICE_NOT_CONNECTED;}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pState)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub){return ERROR_DEVICE_NOT_CONNECTED;}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

//DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) DWORD WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput(){
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll"); //TODO diagnostic
	if(!XInputLibrary) HMODULE XInputLibrary = LoadLibraryA("xinput9_1_0.dll"); //TODO diagnostic
	if(!XInputLibrary) HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll"); //TODO diagnostic
	if(XInputLibrary){
		XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
		//TODO diagnostic
	}else{ /*TODO diagnostic*/ }
}

internal void
Win32InitDSound(HWND Window, i32 SamplesPerSecond, i32 bufferSize){
	//load the library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	HRESULT result;
	
	if(DSoundLibrary){
		//get a DirectSound object
		direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		
		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))){
			//setup sound format
			WAVEFORMATEX waveFormat{};
			waveFormat.wFormatTag      = WAVE_FORMAT_PCM;
			waveFormat.nChannels       = 2;
			waveFormat.nSamplesPerSec  = SamplesPerSecond;
			waveFormat.wBitsPerSample  = 16;
			waveFormat.nBlockAlign     = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
			waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
			waveFormat.cbSize          = 0;
			
			//create a primary buffer and setup format
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))){
				DSBUFFERDESC bufferDesc{};
				bufferDesc.dwSize  = sizeof(bufferDesc);
				bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
				
				LPDIRECTSOUNDBUFFER primaryBuffer;
				result = DirectSound->CreateSoundBuffer(&bufferDesc, &primaryBuffer, 0);
				if(SUCCEEDED(result)){
					result = primaryBuffer->SetFormat(&waveFormat);
					if(SUCCEEDED(result)){
						OutputDebugStringA("Primary buffer format was set\n");
					}else{ /*TODO diagnostic*/ }
				}else{ /*TODO diagnostic*/ }
			}else{ /*TODO diagnostic*/ }
			
			//create a secondary buffer
			DSBUFFERDESC bufferDesc{};
			bufferDesc.dwSize        = sizeof(bufferDesc);
			bufferDesc.dwFlags       = 0;
			bufferDesc.dwBufferBytes = bufferSize;
			bufferDesc.lpwfxFormat   = &waveFormat;
			
			result = DirectSound->CreateSoundBuffer(&bufferDesc, &GlobalSecondaryBuffer, 0);
			if(SUCCEEDED(result)){
				OutputDebugStringA("Secondary buffer created successfully\n");
			}else{ /*TODO diagnostic*/ }
			
			//start it playing
			
		}else{ /*TODO diagnostic*/ }
	}
}

internal win32_window_dimensions
Win32GetWindowDimensions(HWND Window){
	win32_window_dimensions result;
	
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	result.height = ClientRect.bottom - ClientRect.top;
	result.width = ClientRect.right - ClientRect.left;
	
	return result;
}

internal void
RenderGradient(win32_offscreen_buffer* buffer, int xOffset, int yOffset){
	u8* row = (u8*)buffer->memory;
	for(int y = 0; y < buffer->height; ++y){
		u32* pixel = (u32*)row;
		for(int x = 0; x < buffer->width; ++x){
			//b,g,r,pad     because windows
			u8 b = (x + xOffset);
			u8 g = (y + yOffset);
			//u8 r = ;
			//u8 p = ;
			
			*pixel++ = (/*((r << 16) |*/ (g << 8) | b); //shift red and green, increment the pointer
		}
		row += buffer->pitch;
	}
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer* buffer, int width, int height){
	//free memory if it exists
	if(buffer->memory){
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}
	
	buffer->width = width;
	buffer->height = height;
	buffer->bytesPerPixel = 4;
	
	//setup bitmap info
	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = buffer->width;
	buffer->info.bmiHeader.biHeight = -buffer->height; //negative to make it top-down
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;
	
	//allocate memory
	int BitmapMemorySize = buffer->bytesPerPixel * width * height;
	buffer->memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	buffer->pitch = buffer->width * buffer->bytesPerPixel;
	
	//draw the pixels
	//RenderGradient(0, 0); //TODO remove this possibly
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer* buffer, HDC DeviceContext,
						   int windowWidth, int windowHeight){
	StretchDIBits(DeviceContext, 
				  0, 0, windowWidth, windowHeight,
				  0, 0, buffer->width, buffer->height,
				  buffer->memory,
				  &buffer->info,
				  DIB_RGB_COLORS, SRCCOPY);
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
						UINT Message,
						WPARAM wParam,
						LPARAM lParam){
	LRESULT Result = 0;
	
	switch(Message){
		case WM_DESTROY:{
			//TODO handle with a message to the user?
			GlobalRunning = false;
		} break;
		
		case WM_CLOSE:{
			//TODO handle this as an error - recreate window?
			GlobalRunning = false;
		} break;
		
		case WM_ACTIVATEAPP:{
			OutputDebugStringA("WM_ACTIVEAPP\n");
		} break;
		
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:{
			u32 VKCode = wParam;
			b32 wasDown = ((lParam & (1 << 30)) != 0);
			b32 isDown = ((lParam & (1 << 31)) == 0);
			if(wasDown != isDown){
				if(VKCode == 'W'){
					
				}else if(VKCode == 'A'){
					
				}else if(VKCode == 'S'){
					
				}else if(VKCode == 'D'){
					
				}else if(VKCode == 'Q'){
					
				}else if(VKCode == 'E'){
					
				}else if(VKCode == VK_UP){
					
				}else if(VKCode == VK_LEFT){
					
				}else if(VKCode == VK_DOWN){
					
				}else if(VKCode == VK_RIGHT){
					
				}else if(VKCode == VK_ESCAPE){
					
				}else if(VKCode == VK_SPACE){
					
				}
			}
			b32 AltKeyWasDown = lParam & (1 << 29); //checks if the 29th bit is set
			if(VKCode == VK_F4 && AltKeyWasDown){
				GlobalRunning = false;
			}
		}break;
		
		case WM_PAINT:{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			
			win32_window_dimensions dimensions = Win32GetWindowDimensions(Window);
			Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, 
									   dimensions.width, dimensions.height);
			EndPaint(Window, &Paint);
		} break;
		
		default:{
			//OutputDebugStringA("default\n");
			Result = DefWindowProcA(Window, Message, wParam, lParam);
		} break;
	}
	
	return(Result);
}

struct win32_sound_output{
	int samplesPerSecond;
	int toneHz;
	i16 toneVolume;
	u32 runningSampleIndex;
	int wavePeriod;
	int bytesPerSample;
	int secondaryBufferSize;
	f32 tSine;
	int latencySampleCount;
};

void 
Win32FillSoundBuffer(win32_sound_output* soundOutput, DWORD byteToLock, DWORD bytesToWrite){
	void* region1; void* region2;
	DWORD region1Size, region2Size;
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(byteToLock, bytesToWrite, 
											 &region1, &region1Size, 
											 &region2, &region2Size, 0))){
		//TODO assert region1Size/region2Size is valid
		i16* sampleOut = (i16*)region1;
		DWORD region1SampleCount = region1Size/soundOutput->bytesPerSample;
		for(DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex){
			f32 sineValue = sinf(soundOutput->tSine);
			i16 sampleValue = i16(sineValue * soundOutput->toneVolume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			
			soundOutput->tSine += 2.f*M_PI / f32(soundOutput->wavePeriod);
			++soundOutput->runningSampleIndex;
		}
		
		sampleOut = (i16*)region2;
		DWORD region2SampleCount = region2Size/soundOutput->bytesPerSample;
		for(DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex){
			f32 sineValue = sinf(soundOutput->tSine);
			i16 sampleValue = i16(sineValue * soundOutput->toneVolume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			
			soundOutput->tSine += 2.f*M_PI / f32(soundOutput->wavePeriod);
			++soundOutput->runningSampleIndex;
		}
		
		GlobalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
	}
}

internal int CALLBACK
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode){
	LARGE_INTEGER perfCountFrequencyResult;
	QueryPerformanceFrequency(&perfCountFrequencyResult);
	i64 perfCountFrequency = perfCountFrequencyResult.QuadPart;
	
	Win32LoadXInput();
	
    WNDCLASSA WindowClass{};
	Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
	
	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	//WindowClass.hIcon;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";
	
	if(RegisterClassA(&WindowClass)){
		HWND Window = CreateWindowExA(0, WindowClass.lpszClassName,
									  "Handmade Hero",
									  WS_OVERLAPPEDWINDOW|WS_VISIBLE,
									  CW_USEDEFAULT, CW_USEDEFAULT,
									  CW_USEDEFAULT, CW_USEDEFAULT,
									  0, 0, Instance, 0);
		//get a message when there is one, keep going otherwise
		if(Window){
			HDC DeviceContext = GetDC(Window);
			
			//graphics
			int xOffset = 0;
			int yOffset = 0;
			
			//sound
			win32_sound_output soundOutput{};
			soundOutput.samplesPerSecond = 48000;
			soundOutput.toneHz = 256;
			soundOutput.toneVolume = 3000;
			soundOutput.wavePeriod = soundOutput.samplesPerSecond / soundOutput.toneHz;
			soundOutput.bytesPerSample = sizeof(i16)*2;
			soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond*soundOutput.bytesPerSample;
			soundOutput.latencySampleCount = soundOutput.samplesPerSecond;
			Win32InitDSound(Window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
			Win32FillSoundBuffer(&soundOutput, 0, soundOutput.latencySampleCount*soundOutput.bytesPerSample);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
			
			u64 lastCycleCount = __rdtsc();
			LARGE_INTEGER lastCounter;
			QueryPerformanceCounter(&lastCounter);
			
			GlobalRunning = true;
			while(GlobalRunning){
				MSG Message;
				
				//process messages
				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)){
					if(Message.message == WM_QUIT){
						GlobalRunning = false;
					}
					
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}
				
				//input
				for(DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex){
					XINPUT_STATE controllerState;
					if(XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS){
						//controller plugged in
						XINPUT_GAMEPAD* pad = &controllerState.Gamepad;
						
						b32 up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						b32 down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						b32 left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						b32 right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						b32 start = (pad->wButtons & XINPUT_GAMEPAD_START);
						b32 back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
						b32 lShoulder = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						b32 rShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						b32 aButton = (pad->wButtons & XINPUT_GAMEPAD_A);
						b32 bButton = (pad->wButtons & XINPUT_GAMEPAD_B);
						b32 xButton = (pad->wButtons & XINPUT_GAMEPAD_X);
						b32 yButton = (pad->wButtons & XINPUT_GAMEPAD_Y);
						
						i16 stickX = pad->sThumbLX; 
						i16 stickY = pad->sThumbLY; 
						
						//controller rumble
						//XINPUT_VIBRATION vibration;
						//vibration.wLeftMotorSpeed = 60000;
						//vibration.wRightMotorSpeed = 60000;
						//XInputSetState(controllerIndex, &vibration);
						
						xOffset += stickX / 8192;
						yOffset += stickY / 8192;
					}else{
						//controller not available
					}
				}
				
				//render
				RenderGradient(&GlobalBackbuffer, xOffset, yOffset);
				
				//sound output test
				DWORD playCursor;
				DWORD writeCursor;
				if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))){
					DWORD byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
					DWORD targetCursor = (playCursor + (soundOutput.latencySampleCount*soundOutput.bytesPerSample)) % soundOutput.secondaryBufferSize;
					DWORD bytesToWrite;
					if(byteToLock > playCursor){
						bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
						bytesToWrite += targetCursor;
					}else{
						bytesToWrite = targetCursor - byteToLock;
					}
					
					Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite);
				}
				
				//draw window
				win32_window_dimensions dimensions = Win32GetWindowDimensions(Window);
				Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, dimensions.width, dimensions.height);
				
				u64 endCycleCount = __rdtsc();
				LARGE_INTEGER endCounter;
				QueryPerformanceCounter(&endCounter);
				
				u64 cyclesElapsed = endCycleCount - lastCycleCount;
				i64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
				f32 mspf = (1000.0*(f32)counterElapsed) / (f32)perfCountFrequency;
				f32 fps = (f32)perfCountFrequency / (f32)counterElapsed;
				f32 mcpf = (f32)cyclesElapsed / (1000.0*1000.0);
				
				char buffer[256];
				sprintf(buffer, "%.2fmspf, %.2ffps, %.2fmcpf\n", mspf, fps, mcpf);
				OutputDebugStringA(buffer);
				
				lastCounter = endCounter;
				lastCycleCount = endCycleCount;
			}
		}else{ /*TODO logging */ }
	}else{ /*TODO logging */ }
	return 0;
}

