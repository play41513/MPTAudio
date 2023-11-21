//---------------------------------------------------------------------------

#ifndef WaveAnalysisH
#define WaveAnalysisH
#include <vcl.h>
#include <math.h>
//---------------------------------------------------------------------------
struct WAVE_ANLY_PARM
   {
	   DWORD freqUnit;
	   TImage* Spectrum_View;
	   float L_channel_vertex_x_coordinates;
	   float L_channel_vertex_y_coordinates;
	   float R_channel_vertex_x_coordinates;
	   float R_channel_vertex_y_coordinates;

	   float freqX_dot[22];
	   float zerolevel;
	   bool  L_channel_fail;
	   bool  R_channel_fail;
   };

class WaveAnalysis
{
private:
	double          *WaveR;//�곡
	double          *WaveI;//�곡
	void fft1(double *Real_Quantity, double *Imaginary_Quantity, int data_lengh, const double *zSin, const double *zCos);
	void binrv(double *bc, int data_lengh, const int *lb);
	void brtab(int *lbr, int data_lengh);
	void cstab(double *zSin, double *zCos, int data_lengh, int ity);
	void FFT(double *Real_Quantity, double *Imaginary_Quantity, int data_lengh, int ity);
public:
	struct WAVE_ANLY_PARM PWAVE_ANLY_PARM;
	void WaveAnalysis::FillBuffer(short *idata,TImage* Spectrum_View,DWORD dwVertex);
	void WaveAnalysis::ClearFreqWave(TImage* Spectrum_View);
	float GetWaveFreq(bool bLeft,float freq);
};
#endif