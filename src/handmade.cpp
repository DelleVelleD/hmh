#include "handmade.h"

static_internal void
GameOutputSound(GameSoundOutputBuffer* sound_buffer, int tone_hz){
	local_persist f32 tSine;
	s16 toneVolume = 3000;
	int wavePeriod = sound_buffer->samples_per_second / tone_hz;
	
	s16* sampleOut = sound_buffer->samples;
	for(int sampleIndex = 0; sampleIndex < sound_buffer->sample_count; ++sampleIndex){
		f32 sineValue = sinf(tSine);
		s16 sampleValue = (s16)(sineValue *   toneVolume);
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;
		
		tSine += M_2PI / (f32)wavePeriod;
	}
}

static_internal void
RenderGradient(GameOffscreenBuffer* buffer, int xOffset, int yOffset){
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

static_internal void 
GameUpdateAndRender(GameMemory* memory, GameInput* input, GameOffscreenBuffer* render_buffer, GameSoundOutputBuffer* sound_buffer){
	GameState* game_state = (GameState*)memory->permanent_storage;
	Assert(sizeof(game_state) <= memory->permanent_storage_size);
	
	if(!memory->initialized) {
		game_state->tone_hz  = 256;
		memory->initialized = true;
	}
	
	GameControllerInput* input0 = &input->controllers[0];
	if(input0->analog){
		game_state->x_offset += (int)(4.f*input0->stick_left.end_x);
		game_state->tone_hz = 256 + (int)(128.f*input0->stick_left.end_y);
	}else{
		
	}
	
	if(input0->button_down.ended_down){
		game_state->y_offset += 1;
	}
	
	GameOutputSound(sound_buffer, game_state->tone_hz);
	RenderGradient(render_buffer, game_state->x_offset, game_state->y_offset);
}