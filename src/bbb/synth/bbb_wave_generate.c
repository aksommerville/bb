#include "bbb/bbb.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* Sine.
 */
 
void bbb_wave_generate_sine(int16_t *v,int c) {
  double p=0.0,dp=(M_PI*2.0f)/c;
  int16_t lo=0,hi=0;
  for (;c-->0;v++,p+=dp) {
    *v=sin(p)*32760.0;
  }
}

/* Square with rounded corners.
 */
 
void bbb_wave_generate_losquare(int16_t *v,int c,int16_t level,double sigma) {
  if (c<1) return;
  
  // We're going to cheat, and only generate half of the wave.
  // If its length is odd, put an extra plateau sample at the end and lop it off.
  if (c&1) v[--c]=level;
  int halfc=c>>1;
  int16_t *back=v+halfc;
  
  // Measure the flat part and the sigmoid.
  int sigmac=halfc*sigma;
  if (sigmac<0) sigmac=0;
  else if (sigmac>halfc) sigmac=halfc;
  int flatc=halfc-sigmac;
  
  // Generate the plateau and valley.
  for (;flatc-->0;v++,back++) {
    *v=level;
    *back=-level;
  }
  
  // Generate the sigmoid: Half of a cosine wave, ie from max to min.
  double p=0.0,dp=M_PI/sigmac;
  for (;sigmac-->0;v++,back++,p+=dp) {
    *v=cos(p)*level;
    *back=-*v;
  }
}

/* Saw with rounded corners.
 * Picture a cosine wave: A big valley.
 * Now slide the bottom of the valley rightward and keep it continuous.
 */
 
void bbb_wave_generate_losaw(int16_t *v,int c,double base) {
  if (c<1) return;
  if (base<1.0) base=1.0;
  const int16_t level=32000;
  double denom=base-1.0;
  double np=0.0,npd=1.0/c;
  for (;c-->0;v++,np+=npd) {
    double p=(pow(base,np)-1.0)/denom; // 0..1
    *v=cos(p*M_PI*2.0)*level;
  }
}

/* Harmonics.
 */
 
void bbb_wave_generate_harmonics_u8(int16_t *v,const int16_t *ref,int c,const uint8_t *srccoefv,int coefc,int normalize) {
  if (!v||!ref||(c<1)||(coefc<1)) return;
  
  if (coefc>16) coefc=16;
  while (!srccoefv[coefc-1]) coefc--;
  if (!coefc) return;
  double coefv[16]={0};
  int i=coefc;
  if (normalize) {
    while (i-->0) coefv[i]=srccoefv[i]/(255.0*(i+1));
  } else {
    while (i-->0) coefv[i]=srccoefv[i]/255.0;
  }
  
  uint32_t pv[16]={0}; // literal index, not the 32-bit normal position we usually use
  for (i=c;i-->0;v++) {
    int j=coefc; while (j-->0) {
      if (!srccoefv[j]) continue;
      (*v)+=ref[pv[j]]*coefv[j];
      pv[j]+=j+1;
      if (pv[j]>=c) pv[j]-=c;
    }
  }
}

/* Single period FM.
 */
 
void bbb_wave_generate_fm(int16_t *v,const int16_t *ref,int c,uint8_t rate,double range) {
  if (c<rate) return;
  double cp=0.0;
  int i=c,mp=0;
  for (;i-->0;v++) {
    double dcp=1.0+(ref[mp]*range)/32760.0;
    mp+=rate;
    if (mp>=c) mp-=c;
    cp+=dcp;
    int cpi=(int)cp;
    if (cpi<0) cpi+=c;
    if (cpi>=c) {
      cpi%=c;
      cp-=c;
    }
    *v=ref[cpi];
  }
}

/* Trivial shapes.
 * Normally you don't want a wave for these, just generate them real-time.
 * But sometimes you need to generalize, so a wave is handy.
 */
 
static void bbb_signal_set_s(int16_t *v,int c,int16_t src) {
  if (c<1) return;
  v[0]=src;
  int havec=1;
  int halfc=c>>1;
  while (havec<=halfc) {
    havec<<=1;
    memcpy(v+havec,v,havec);
  }
  memcpy(v+havec,v,(c-havec)<<1);
}
 
void bbb_wave_generate_square(int16_t *v,int c) {
  if (c<1) return;
  int frontc=c>>1;
  int backc=c-frontc;
  bbb_signal_set_s(v,frontc,32767);
  bbb_signal_set_s(v+frontc,backc,-32767);
}

void bbb_wave_generate_saw(int16_t *v,int c,int16_t a,int16_t z) {
  if (a==z) {
    bbb_signal_set_s(v,c,a);
    return;
  }
  int p=0,range=z-a;
  for (;p<c;p++,v++) {
    *v=a+(range*p)/c;
  }
}

void bbb_wave_generate_triangle(int16_t *v,int c) {
  if (c<1) return;
  int frontc=c>>1;
  int backc=frontc-c;
  bbb_wave_generate_saw(v,frontc,-32767,32767);
  bbb_wave_generate_saw(v+frontc,backc,32767,-32767);
}
