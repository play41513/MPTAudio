z//---------------------------------------------------------------------------


#pragma hdrstop

#include "WaveAnalysis.h"
#include "ConstantString.h"
#include "main.h"

#define ERRORS 0.01086958
//---------------------------------------------------------------------------

#pragma package(smart_init)
void WaveAnalysis::FillBuffer(short *idata,TImage* Spectrum_View,DWORD dwVertex)
{   //http://www.doc88.com/p-90094135557.html   p23
	int i;
	double  dbTmp,x,y,count = buffer_size;
	short *data=new short[buffer_size*2];
	WaveR=new double[buffer_size];
	WaveI=new double[buffer_size];
	memcpy(data, idata, 2 * count * sizeof(short));
	bool bGetMainFreq = false;
	PWAVE_ANLY_PARM.L_channel_fail = false;
	PWAVE_ANLY_PARM.R_channel_fail = false;
	PWAVE_ANLY_PARM.Spectrum_View = Spectrum_View;
	AnsiString SS;
	DWORD dwHighestPoint = 200- dwVertex-10;

	//static_cast 強制轉型
	double  ts=static_cast<double>(Spectrum_View->ClientWidth) / static_cast<double>(count) * 2;
	//左聲道
	memset(WaveI, 0, count * sizeof(double));
	for(i=0; i < count; i++)
	{
		WaveR[i]=data[i * 2];
	}

	FFT(WaveR, WaveI, count, 1);
	Spectrum_View->Canvas->Pen->Color=clAqua;
	Spectrum_View->Canvas->Pen->Width = 2;
	Spectrum_View->Canvas->MoveTo(0, PWAVE_ANLY_PARM.zerolevel - sqrt(WaveR[0] * WaveR[0] + WaveI[0] * WaveI[0]) / count * 2);
	PWAVE_ANLY_PARM.L_channel_vertex_y_coordinates = PWAVE_ANLY_PARM.zerolevel;
	for(i=1; i < count / 2; i++)
	{
		x = i * ts;
		dbTmp = double(sqrt(WaveR[i] * WaveR[i] + WaveI[i] * WaveI[i]) / count * 2);
		//dbTmp = (double)PWAVE_ANLY_PARM.zerolevel*dbTmp/659;
		y = PWAVE_ANLY_PARM.zerolevel - dbTmp;
		if(dbTmp>PWAVE_ANLY_PARM.zerolevel)
			y =0;
		Spectrum_View->Canvas->LineTo(x, y);
		if(!bGetMainFreq && y< dwHighestPoint && y <= PWAVE_ANLY_PARM.L_channel_vertex_y_coordinates)
		{
			SS.printf("%.2f",SS);
			PWAVE_ANLY_PARM.L_channel_vertex_y_coordinates = SS.ToDouble();
			PWAVE_ANLY_PARM.L_channel_vertex_x_coordinates = x;
		}
		else if(!bGetMainFreq&& PWAVE_ANLY_PARM.L_channel_vertex_y_coordinates+50 < y)
		{    //判斷頻譜第一個高點已過去
			if(y>195)
				bGetMainFreq = true;
		}
		else if(bGetMainFreq && y< dwHighestPoint)//判斷第二個頂點
			PWAVE_ANLY_PARM.L_channel_fail = true;
	}
	//右聲道
	bGetMainFreq = false;
	memset(WaveI, 0, count * sizeof(double));
	for(i=0; i < count; i++)
    {
		WaveR[i]=data[i * 2 + 1];
    }

	FFT(WaveR, WaveI, count, 1);
	Spectrum_View->Canvas->Pen->Color=clRed;
	Spectrum_View->Canvas->Pen->Width = 1;
	Spectrum_View->Canvas->MoveTo(0, PWAVE_ANLY_PARM.zerolevel - sqrt(WaveR[0] * WaveR[0] + WaveI[0] * WaveI[0]) / count * 2);
	PWAVE_ANLY_PARM.R_channel_vertex_y_coordinates = PWAVE_ANLY_PARM.zerolevel;
	for(i=1; i < count / 2; i++)
	{
		dbTmp = double(sqrt(WaveR[i] * WaveR[i] + WaveI[i] * WaveI[i]) / count * 2);
		//dbTmp = (double)PWAVE_ANLY_PARM.zerolevel*dbTmp/899;
		y = PWAVE_ANLY_PARM.zerolevel - dbTmp;
		x = i*ts;
		if(dbTmp>PWAVE_ANLY_PARM.zerolevel)
			y =0;
		Spectrum_View->Canvas->LineTo(x, y);
		if(y< dwHighestPoint && y <= PWAVE_ANLY_PARM.R_channel_vertex_y_coordinates)
		{
			SS.printf("%.2f",SS);
			PWAVE_ANLY_PARM.R_channel_vertex_y_coordinates = SS.ToDouble();
			PWAVE_ANLY_PARM.R_channel_vertex_x_coordinates = x;
		}
		else if(!bGetMainFreq&& PWAVE_ANLY_PARM.R_channel_vertex_y_coordinates+50 < y)
		{    //判斷頻譜第一個頂點已過
			if(y>198)
				bGetMainFreq = true;
		}
		else if(bGetMainFreq && y< dwHighestPoint)//判斷第二個頂點
			PWAVE_ANLY_PARM.R_channel_fail = true;
	}
	Spectrum_View->Canvas->Unlock();
	delete[] data;
	delete[] WaveR;
	delete[] WaveI;
}

//---------------------------------------------------------------------------
void WaveAnalysis::fft1(double *Real_Quantity, double *Imaginary_Quantity, int data_lengh, const double *zSin, const double *zCos)
{
	int is=data_lengh >> 1;
	int mpx=0;
    int ic=1;
	int ia;

    while(is)
    {
		is>>=1;
		mpx++;
    }

	is=data_lengh;

	for(ia=1; ia <= mpx; ia++)
    {
		int ib;
        int ka=0;
		is>>=1;
        for(ib=1; ib <= ic; ib++)
		{
            int in=1;
			int k;
            for(k=1; k <= is; k++)
            {
				int     j1=ka + k;
				int     j2=j1 + is;
				double  xRe=Real_Quantity[j1];
				double  xIm=Imaginary_Quantity[j1];
				double  yRe=Real_Quantity[j2];
				double  yIm=Imaginary_Quantity[j2];
				Real_Quantity[j1]=xRe + yRe;
				Imaginary_Quantity[j1]=xIm + yIm;
				xRe-=yRe;
				xIm-=yIm;
				Real_Quantity[j2]=xRe * zCos[in] - xIm * zSin[in];
				Imaginary_Quantity[j2]=xRe * zSin[in] + xIm * zCos[in];
                in+=ic;
			}

            ka+=is << 1;
        }

		ic<<=1;
	}
}

/* */
void WaveAnalysis::binrv(double *bc, int data_lengh, const int *lb)
{
	int is=data_lengh - 1;
    int i;
    for(i=2; i <= is; i++)
    {
        int ig=lb[i];
        if(ig <= i) continue;

        double  xx=bc[i];
        bc[i]=bc[ig];
        bc[ig]=xx;
    }
}

/* */
void WaveAnalysis::brtab(int *lbr, int data_lengh)
{
	int is=data_lengh >> 1;
	int mpx=0;
	//得到資料數為2的幾次方
	while(is)
    {
        is>>=1;
		mpx++;
    }

    int ln;
	for(ln=1; ln <= data_lengh; ln++)
    {
		int j1=ln - 1;
        int ibord=0;
		for(int k=1; k <= mpx; k++)
        {
            int j2=j1 >> 1;
			ibord=ibord * 2 + (j1 - 2 * j2);
			j1=j2;
		}
		lbr[ln]=ibord + 1;
	}
}

/* */
void WaveAnalysis::cstab(double *zSin, double *zCos, int data_lengh, int ity)
{//計算加權係數（旋轉因子w的i次冪表)   Sin[n]=sin (0~2 pi)
	double  yy;
	yy= -PI * 2.0 / data_lengh;
	if(ity < 0) yy= -yy;

	double  angle=0.0;
	for(int i=1; i <= data_lengh; i++)
	{
		zSin[i]=sin(angle);
		zCos[i]=cos(angle);
		angle+=yy;
	}
}

/* */
void WaveAnalysis::FFT(double *Real_Quantity, double *Imaginary_Quantity, int data_lengh, int ity)
{
	double  *zCos, *zSin;
    int     *il;
	if(data_lengh <= 0) return;

	double  invsqrtn=1.0 / sqrt(data_lengh);
	zCos= (double *)malloc(sizeof(double) * data_lengh) - 1;
	zSin= (double *)malloc(sizeof(double) * data_lengh) - 1;
	cstab(zSin, zCos, data_lengh, ity);
	il= (int *)malloc(sizeof(int) * data_lengh) - 1;
	brtab(il, data_lengh);
	fft1(Real_Quantity - 1, Imaginary_Quantity - 1, data_lengh, zSin, zCos);
	binrv(Real_Quantity - 1, data_lengh, il);
	binrv(Imaginary_Quantity - 1, data_lengh, il);
	for(int i=0; i < data_lengh; i++)
    {
		Real_Quantity[i]*=invsqrtn;
		Imaginary_Quantity[i]*=invsqrtn;
    }

    free(il + 1);
	free(zCos + 1);
	free(zSin + 1);
}
void WaveAnalysis::ClearFreqWave(TImage* Spectrum_View)
{
	PWAVE_ANLY_PARM.zerolevel=Spectrum_View->ClientHeight - 20;
	float  dx;

	Spectrum_View->Canvas->Lock();
	Spectrum_View->Canvas->Brush->Color=clBlack;
	Spectrum_View->Canvas->Pen->Color=clWhite;
	Spectrum_View->Canvas->Rectangle(0, 0, Spectrum_View->ClientWidth, Spectrum_View->ClientHeight);
	Spectrum_View->Canvas->Pen->Color=clYellow;
	Spectrum_View->Canvas->MoveTo(0, PWAVE_ANLY_PARM.zerolevel);
	Spectrum_View->Canvas->LineTo(Spectrum_View->ClientWidth, PWAVE_ANLY_PARM.zerolevel);

	Spectrum_View->Canvas->Font->Color = clWhite;
	//
	PWAVE_ANLY_PARM.L_channel_fail = false;
	PWAVE_ANLY_PARM.R_channel_fail = false;
	AnsiString astrNumWave = Spectrum_View->Tag;
	TPanel *plCoordinateR = (TPanel *)frmMain->FindComponent("plCoordinateR"+astrNumWave);
	TPanel *plFreqR = (TPanel *)frmMain->FindComponent("plFreqR"+astrNumWave);
	TPanel *plCoordinateL = (TPanel *)frmMain->FindComponent("plCoordinateL"+astrNumWave);
	TPanel *plFreqL = (TPanel *)frmMain->FindComponent("plFreqL"+astrNumWave);
	plCoordinateR->Caption = "";
	plCoordinateL->Caption = "";
	plFreqR->Caption = "";
	plFreqL->Caption = "";
	//填入座標
	PWAVE_ANLY_PARM.freqX_dot[0] = 0;
	PWAVE_ANLY_PARM.freqX_dot[1] = 23.25;
	PWAVE_ANLY_PARM.freqX_dot[2] = 46.5;
	PWAVE_ANLY_PARM.freqX_dot[3] = 69.75;
	PWAVE_ANLY_PARM.freqX_dot[4] = 93;
	PWAVE_ANLY_PARM.freqX_dot[5] = 116.25;
	PWAVE_ANLY_PARM.freqX_dot[6] = 139.25;
	PWAVE_ANLY_PARM.freqX_dot[7] = 162.5;
	PWAVE_ANLY_PARM.freqX_dot[8] = 185.75;
	PWAVE_ANLY_PARM.freqX_dot[9] = 209;
	PWAVE_ANLY_PARM.freqX_dot[10] = 232.25;
	PWAVE_ANLY_PARM.freqX_dot[11] = 255.5;
	PWAVE_ANLY_PARM.freqX_dot[12] = 278.75;
	PWAVE_ANLY_PARM.freqX_dot[13] = 302;
	PWAVE_ANLY_PARM.freqX_dot[14] = 325;
	PWAVE_ANLY_PARM.freqX_dot[15] = 348.25;
	PWAVE_ANLY_PARM.freqX_dot[16] = 371.5;
	PWAVE_ANLY_PARM.freqX_dot[17] = 394.75;
	PWAVE_ANLY_PARM.freqX_dot[18] = 418;
	PWAVE_ANLY_PARM.freqX_dot[19] = 441.25;
	PWAVE_ANLY_PARM.freqX_dot[20] = 464;
	PWAVE_ANLY_PARM.freqX_dot[21] = 487;
	//
	for(int i=1; i <= 20; i++)
	{
		if(i<10)
			Spectrum_View->Canvas->TextOutA(PWAVE_ANLY_PARM.freqX_dot[i], PWAVE_ANLY_PARM.zerolevel + 2, AnsiString(i));
		else
		{
			Spectrum_View->Canvas->TextOutA(PWAVE_ANLY_PARM.freqX_dot[i]-6, PWAVE_ANLY_PARM.zerolevel + 2, AnsiString(i));
			if(i==20)
            	Spectrum_View->Canvas->TextOutA(PWAVE_ANLY_PARM.freqX_dot[i]+20, PWAVE_ANLY_PARM.zerolevel + 2, "(kHz)");
		}
	}
}
float WaveAnalysis::GetWaveFreq(bool bLeft,float freq)
{
	float numDot1K = 0,NumDot;
	AnsiString astrNumWave = AnsiString(PWAVE_ANLY_PARM.Spectrum_View->Tag);
	float fWaveFreq;
	AnsiString SS;
	if(bLeft)
	{
		TPanel *plCoordinateL = (TPanel *)frmMain->FindComponent("plCoordinateL"+astrNumWave);
		TPanel *plFreqL = (TPanel *)frmMain->FindComponent("plFreqL"+astrNumWave);
		plCoordinateL->Caption = "("+AnsiString(PWAVE_ANLY_PARM.L_channel_vertex_x_coordinates)+" , "
								+AnsiString(PWAVE_ANLY_PARM.L_channel_vertex_y_coordinates)+")";
		if(PWAVE_ANLY_PARM.L_channel_fail)
		{
			plFreqL->Caption = "MIXING";
			return ERROR_WAVE_MIXING;
		}
		else if(PWAVE_ANLY_PARM.L_channel_vertex_y_coordinates
			== PWAVE_ANLY_PARM.zerolevel)
		{
			plFreqL->Caption = "MUTE";
			return ERROR_WAVE_MUTE;
		}
		else
		{
			for(int i = 0 ; i<=20 ; i++)
			{
				if(PWAVE_ANLY_PARM.L_channel_vertex_x_coordinates
					> PWAVE_ANLY_PARM.freqX_dot[i]
					&& PWAVE_ANLY_PARM.L_channel_vertex_x_coordinates
					<= PWAVE_ANLY_PARM.freqX_dot[i+1])
				{
					numDot1K = PWAVE_ANLY_PARM.freqX_dot[i+1]-PWAVE_ANLY_PARM.freqX_dot[i];
					NumDot   = PWAVE_ANLY_PARM.L_channel_vertex_x_coordinates - PWAVE_ANLY_PARM.freqX_dot[i];
					fWaveFreq = float(i) + float(NumDot/numDot1K);
					SS.printf("%.2f",fWaveFreq);
					plFreqL->Caption = SS+" (kHz)";

					if(fWaveFreq+(fWaveFreq * frmMain->PAudio_PARM.dblFreq) >= freq
						&& fWaveFreq-(fWaveFreq * frmMain->PAudio_PARM.dblFreq) <= freq )
						return WAVE_FREQ_PASS;
					else return fWaveFreq;
				}
			}
			return ERROR_WAVE_MUTE;
		}
	}
	else
	{
		TPanel *plCoordinateR = (TPanel *)frmMain->FindComponent("plCoordinateR"+astrNumWave);
		TPanel *plFreqR = (TPanel *)frmMain->FindComponent("plFreqR"+astrNumWave);
		plCoordinateR->Caption = "("+AnsiString(PWAVE_ANLY_PARM.R_channel_vertex_x_coordinates)+" , "
								+AnsiString(PWAVE_ANLY_PARM.R_channel_vertex_y_coordinates)+")";
		if(PWAVE_ANLY_PARM.R_channel_fail)
		{
			plFreqR->Caption = "MIXING";
			return ERROR_WAVE_MIXING;
		}
		else if(PWAVE_ANLY_PARM.R_channel_vertex_y_coordinates
			== PWAVE_ANLY_PARM.zerolevel)
		{
			plFreqR->Caption = "MUTE";
			return ERROR_WAVE_MUTE;
		}
		else
		{
			for(int i = 0 ; i<=20 ; i++)
			{
				if(PWAVE_ANLY_PARM.R_channel_vertex_x_coordinates
					> PWAVE_ANLY_PARM.freqX_dot[i]
					&& PWAVE_ANLY_PARM.R_channel_vertex_x_coordinates
					<= PWAVE_ANLY_PARM.freqX_dot[i+1])
				{
					numDot1K = PWAVE_ANLY_PARM.freqX_dot[i+1]-PWAVE_ANLY_PARM.freqX_dot[i];
					NumDot   = PWAVE_ANLY_PARM.R_channel_vertex_x_coordinates - PWAVE_ANLY_PARM.freqX_dot[i];
					fWaveFreq = float(i) + float(NumDot/numDot1K);
					SS.printf("%.2f",fWaveFreq);
					plFreqR->Caption = SS+" (kHz)";
					if(fWaveFreq+(fWaveFreq*0.05) >= freq && fWaveFreq-(fWaveFreq*0.05) <= freq )
					 	return WAVE_FREQ_PASS;
					else return fWaveFreq;
				}
			}
			return ERROR_WAVE_MUTE;
		}
    }
}
