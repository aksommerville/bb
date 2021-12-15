/* bb_pitch.h
 */
 
#ifndef BB_PITCH_H
#define BB_PITCH_H

#include <stdint.h>

extern const float bb_hz_from_noteidv[128];

float bb_adjust_pitch_f(float pitch,float cents);
uint32_t bb_adjust_pitch_u32(uint32_t pitch,int16_t cents);
uint16_t bb_adjust_pitch_u16(uint16_t pitch,int16_t cents);

#endif
