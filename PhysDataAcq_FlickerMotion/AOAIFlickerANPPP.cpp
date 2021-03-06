//Things to do:
// Need to add a way to re-read from a small x, y coordinate file since these values are repeated
//Add the input to the SCB to record a second channel and test the values recorded


// (!) NOTE (!) This was created on [11/16/2017] and was copied from AOAIContDIStamp.cpp
// it is intended to make a new trial that contains only flicker/AN/PPP irrespective of the Matlab worldmaps

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <conio.h>
#include <stdio.h>
#include <windows.h>
#include <d:\Scripts\PhysDataAcqScripts\Visual Studio Projects\PhysDataAcq_FlickerMotion\PhysDataAcq_FlickerMotion\NIDAQmx.h>

#include "DynamicRandGen.h"
#include "CorrelatedNoise.h"
#include "FileCtrl.h"
#include "MenuReturnValues.h"

#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

using namespace std;

void DynamicRandGen(MenuReturnValues mValues, int idx)
{
	bool32		done = 0;
	char		errBuff[2048] = { '\0' };
	int32		totalReadCh0 = 0;
	int32		totalReadCh1 = 0;
	int32		error = 0;
	uInt32		NHalfBufs = 0;
	uInt32		NZSample = 0;
	uInt32		TotNZSample = 0;
	int16		outputBuffer;

	// Initialize variables for the Analog Input
	double		RecMaxAmp = mValues.dRecMaxVoltVal;
	double		RecMinAmp = mValues.dRecMinVoltVal;
	double		RecSampRate = mValues.dRecSampRate;
	float64		AIRecTimeout = mValues.dAIRecTimeOut;
	int16		NumAIChannels = mValues.iNumAIChans;
	int32		NumAISampRead = 0;
	ofstream*	ptrAIFile = 0;
	uInt32		totalAISampRead = 0;
	uInt32		AIOneChanBufSiz = 2000;
	uInt32		AIBuffer_Siz = AIOneChanBufSiz*NumAIChannels;	//KEY FOR THIS CODE WORKING IS THAT THE DIGITAL BUFFER BE > 2046 SAMPLES
	uInt32		AIBufferSpaceFilled = 0;
	string		strAIFileName;

	// Initialize variables for the Counters, and Digital Input Timestamps
	int32		samplesPerChanRead = 0;
	int32		numSampsPerChan = mValues.slNumSampsPerChan; //This # is chosen using an estimated H1 firing rate of 500 spikes/s & => at least 4 seconds before acquiring 2000 samples
	uInt32		ReadBufferSize = mValues.ulReadBufferSize; //This # is chosen based on 2 estimates: 1)Smallish frequency of disk writes, 2)H1 firing rate ~500 Spikes/s
	double		CIRecTimeout = mValues.dCIRecTimeOut;
	ofstream*	ptrAIFileCh0 = 0;
	ofstream*	ptrAIFileCh1 = 0;
	string		strAIFileNameCh0;
	string		strAIFileNameCh1;

	// Initialize variables for the Analog Output (Z-signal for 608 oscilloscope)
	bool		AOAutoStart = mValues.bStimAutoStart;
	double		StimMaxAmp = mValues.dStimMaxVoltVal;
	double		StimMinAmp = mValues.dStimMinVoltVal;
	double		StimSampRate = mValues.dStimSampRate;
	float64		AOStimTimeout = mValues.dStimTimeOut;
	float64		ActualSampRate = 0;
	int16		NumAOChannels = mValues.iNumNIAOChans;
	int16		NRepeats = mValues.iNumRepeats;
	double		TotalHalfBufs = 0;
	int32 		NumAOSampWritten = 0;
	ifstream*	ptrAOFile = 0;
	uInt32		AOBufferSpaceFree = 0;
	uInt32		AOFileSize = 0;
	uInt32		NZSignalRepeat = 3;
	uInt32		AOOneChanBufSiz = 200000;
	uInt32		AOHalfBuf_Siz = AOOneChanBufSiz * NumAOChannels * NZSignalRepeat;
	uInt32		AOBuffer_Siz = AOHalfBuf_Siz * 2;	//KEY FOR THIS CODE WORKING IS THAT THE DIGITAL BUFFER BE > 2046 SAMPLES
	string		strAOFileName;

	int	SingleChannel = mValues.iNumUEIAOChans;
	char PhysicalAOChannels[15];
	if (SingleChannel == 1)
		sprintf_s(PhysicalAOChannels, "/Dev2/ao%i", NumAOChannels - 1); //sprintf() => sprintf_s() 10/18/2017 [AWT]
	else if (SingleChannel == 0)
		sprintf_s(PhysicalAOChannels, "/Dev2/ao0:%i", NumAOChannels - 1); //sprintf() => sprintf_s() 10/18/2017 [AWT]
	else {
		cout << "Menu Option H Set Incorrectly" << endl;
		return;
	}

	// Initialize variables for the Analog Output (X-signal for 608 oscilloscope)
	ifstream*	ptrAO_X_File = 0;
	uInt32		AO_X_FileSize = 0;
	string		strAO_X_FileName;
	vector<int16>	X;

	// Initialize variables for the Analog Output (Y-signal for 608 oscilloscope)
	ifstream*	ptrAO_Y_File = 0;
	uInt32		AO_Y_FileSize = 0;
	string		strAO_Y_FileName;
	vector<int16>	Y;

	// Create and initialize arrays and vectors
	vector<uInt32>	readArrayCICh0;
	vector<uInt32>	readArrayCICh1;
	readArrayCICh0.assign(ReadBufferSize, 0);
	readArrayCICh1.assign(ReadBufferSize, 0);
	vector<int16>	dAOBuffer;
	vector<double>	dAIBuffer;
	dAOBuffer.assign(AOBuffer_Siz, 0);
	dAIBuffer.assign(AIBuffer_Siz, 0);

	// Initialize the handle to the NI tasks
	TaskHandle  CO1Handle = 0;
	TaskHandle  CO2Handle = 0;
	TaskHandle  AOHandle = 0;
	TaskHandle  AIHandle = 0;


	/*********************************************/
	// Initialize the Analog Response file
	/*********************************************/
	char fileIndex[65];
	_itoa_s(idx, fileIndex, 10); //itoa() => _itoa_s() 10/18/2017 [AWT]
	cout << mValues.strRecFileDirPath << "\\" << mValues.strRecFileBaseName << fileIndex << endl;
	strAIFileName = mValues.strRecFileDirPath + "\\" + mValues.strRecFileBaseName + fileIndex + "_AI.dat";
	ptrAIFile = fnOpenFileToWrite(strAIFileName);

	/*********************************************/
	// Initialize the Digital Response files
	/*********************************************/
	_itoa_s(idx, fileIndex, 10); //itoa() => _itoa_s() 10/18/2017 [AWT]
	cout << mValues.strRecFileDirPath << "\\" << mValues.strRecFileBaseName << fileIndex << "_Ch1.dat" << endl;
	strAIFileNameCh0 = mValues.strRecFileDirPath + "\\" + mValues.strRecFileBaseName + fileIndex + "_Ch1.dat";
	ptrAIFileCh0 = fnOpenFileToWrite(strAIFileNameCh0);

	cout << mValues.strRecFileDirPath << "\\" << mValues.strRecFileBaseName << fileIndex << "_Ch2.dat" << endl;
	strAIFileNameCh1 = mValues.strRecFileDirPath + "\\" + mValues.strRecFileBaseName + fileIndex + "_Ch2.dat";
	ptrAIFileCh1 = fnOpenFileToWrite(strAIFileNameCh1);


	/*********************************************/
	// Initialize the Stimulus file (X-signal)
	/*********************************************/
	strAO_X_FileName = mValues.strStimFileDirPath + "\\" + mValues.strStimFileName1 + ".dat";	//Name of Analog Stimulus file
	ptrAO_X_File = fnOpenFileToRead_int16(strAO_X_FileName, &AO_X_FileSize); //Call function to open Analog Output file
	X.assign(AO_X_FileSize, 0);
	for (uInt32 idx2 = 0; idx2<AO_X_FileSize; idx2++) {
		ptrAO_X_File->read(reinterpret_cast<char *>(&outputBuffer), sizeof(int16));
		X[idx2] = outputBuffer;
	}
	ptrAO_X_File->close();

	/*********************************************/
	// Initialize the Stimulus file (Y-signal)
	/*********************************************/
	strAO_Y_FileName = mValues.strStimFileDirPath + "\\" + mValues.strStimFileName2 + ".dat";	//Name of Analog Stimulus file
	ptrAO_Y_File = fnOpenFileToRead_int16(strAO_Y_FileName, &AO_Y_FileSize); //Call function to open Analog Output file
	Y.assign(AO_Y_FileSize, 0);
	for (uInt32 idx2 = 0; idx2<AO_Y_FileSize; idx2++) {
		ptrAO_Y_File->read(reinterpret_cast<char *>(&outputBuffer), sizeof(int16));
		Y[idx2] = outputBuffer;
	}
	ptrAO_Y_File->close();

	/*********************************************/
	// Initialize the Stimulus file (Z-signal)
	/*********************************************/
	strAOFileName = mValues.strStimFileDirPath + "\\" + mValues.strStimFileName0 + ".dat";	//Name of Analog Stimulus file
	ptrAOFile = fnOpenFileToRead_int16(strAOFileName, &AOFileSize);	//Call function to open Analog Output file

	/*********************************************/
	// Load Analog Output Buffer
	/*********************************************/
	NZSample = ConstructAOBuffer_int16(&dAOBuffer[0], AOOneChanBufSiz * 2, AOFileSize, AO_X_FileSize, ptrAOFile, &X[0], &Y[0], NZSignalRepeat, TotNZSample);
	TotNZSample += NZSample;
	cout << "Number of Z samples written: " << TotNZSample << endl << endl;
	NHalfBufs += 2;

	/*********************************************/
	// DAQmx Reset Device
	/*********************************************/
	DAQmxErrChk(DAQmxResetDevice("Dev2"));

	/*********************************************/
	// DAQmx Configure Code for Counter 1 - Timestamp channel 1
	/*********************************************/
	DAQmxErrChk(DAQmxCreateTask("CO1", &CO1Handle));
	DAQmxErrChk(DAQmxCreateCICountEdgesChan(CO1Handle, "Dev2/ctr0", "", DAQmx_Val_Rising, 0, DAQmx_Val_CountUp));
	DAQmxErrChk(DAQmxCfgSampClkTiming(CO1Handle, "/Dev2/PFI9", 100000.0, DAQmx_Val_Rising, DAQmx_Val_ContSamps, 2000));
	DAQmxErrChk(DAQmxSetCICountEdgesTerm(CO1Handle, "", "/Dev2/100kHzTimebase"));

	/*********************************************/
	// DAQmx Configure Code for Counter 2 - Timestamp channel 2
	/*********************************************/
	DAQmxErrChk(DAQmxCreateTask("CO2", &CO2Handle));
	DAQmxErrChk(DAQmxCreateCICountEdgesChan(CO2Handle, "Dev2/ctr1", "", DAQmx_Val_Rising, 0, DAQmx_Val_CountUp));
	DAQmxErrChk(DAQmxCfgSampClkTiming(CO2Handle, "/Dev2/PFI4", 100000.0, DAQmx_Val_Rising, DAQmx_Val_ContSamps, 2000));
	DAQmxErrChk(DAQmxSetCICountEdgesTerm(CO2Handle, "", "/Dev2/100kHzTimebase"));

	/*********************************************/
	// DAQmx Configure Code for AI
	/*********************************************/
	DAQmxErrChk(DAQmxCreateTask("AI", &AIHandle));
	DAQmxErrChk(DAQmxCreateAIVoltageChan(AIHandle, "/Dev2/ai0", "", DAQmx_Val_RSE, RecMinAmp, RecMaxAmp, DAQmx_Val_Volts, NULL));
	DAQmxErrChk(DAQmxCfgSampClkTiming(AIHandle, "", RecSampRate, DAQmx_Val_Rising, DAQmx_Val_ContSamps, AIOneChanBufSiz));
	DAQmxErrChk(DAQmxGetSampClkRate(AIHandle, &ActualSampRate));	//Read the actual sample clock rate (eventually coerced depending on the hardware used).
	cout << "The actual AI clock rate is: " << ActualSampRate << endl << endl;

	/*********************************************/
	// DAQmx Configure Code for Analog Output
	/*********************************************/
	DAQmxErrChk(DAQmxCreateTask("AO", &AOHandle));
	DAQmxErrChk(DAQmxCreateAOVoltageChan(AOHandle, PhysicalAOChannels, "", StimMinAmp, StimMaxAmp, DAQmx_Val_Volts, NULL));
	DAQmxErrChk(DAQmxCfgSampClkTiming(AOHandle, "/Dev2/ai/SampleClock", StimSampRate, DAQmx_Val_Rising, DAQmx_Val_ContSamps, AOOneChanBufSiz*NZSignalRepeat));
	DAQmxErrChk(DAQmxCfgOutputBuffer(AOHandle, AOBuffer_Siz / NumAOChannels)); // LOOK AT THIS!@!! Made this change and need to check. Switched AOHalfBuf_Siz for AOOneChanBufSiz
	DAQmxErrChk(DAQmxSetWriteRegenMode(AOHandle, DAQmx_Val_DoNotAllowRegen));
	DAQmxErrChk(DAQmxGetSampClkRate(AOHandle, &ActualSampRate));	//Read the actual sample clock rate (eventually coerced depending on the hardware used).
	cout << "The actual AO clock rate is: " << ActualSampRate << endl << endl;

	/*********************************************/
	// DAQmx Configure Start Trigger
	/*********************************************/
	DAQmxErrChk(DAQmxCfgDigEdgeStartTrig(AIHandle, "/Dev2/PFI7", DAQmx_Val_Rising)); // PFI6 -> PFI7 [11/13/2017]

	DAQmxErrChk(DAQmxCfgDigEdgeStartTrig(AOHandle, "/Dev2/PFI7", DAQmx_Val_Rising)); // PFI6 -> PFI7 [11/13/2017]

	DAQmxSetArmStartTrigType(CO1Handle, DAQmx_Val_DigEdge);
	DAQmxSetDigEdgeArmStartTrigSrc(CO1Handle, "/Dev2/PFI7"); // PFI6 -> PFI7 [11/13/2017]
	DAQmxSetDigEdgeArmStartTrigEdge(CO1Handle, DAQmx_Val_Rising);

	DAQmxSetArmStartTrigType(CO2Handle, DAQmx_Val_DigEdge);
	DAQmxSetDigEdgeArmStartTrigSrc(CO2Handle, "/Dev2/PFI7"); // PFI6 -> PFI7 [11/13/2017]
	DAQmxSetDigEdgeArmStartTrigEdge(CO2Handle, DAQmx_Val_Rising);


	/*********************************************/
	// DAQmx Initial Analog Output Write Code
	/*********************************************/
	DAQmxErrChk(DAQmxWriteBinaryI16(AOHandle, AOOneChanBufSiz*NZSignalRepeat * 2, AOAutoStart, AOStimTimeout, DAQmx_Val_GroupByScanNumber, &dAOBuffer[0], &NumAOSampWritten, NULL));



	/*********************************************/
	// Everything is prepared, start operation
	/*********************************************/
	cout << "Continuously Producing Raster for Tektronix 608." << endl;
	cout << "Continuously Timestamping Data." << endl;
	cout << "Press any key to TERMINATE trial." << endl << endl;

	TotalHalfBufs = (AOFileSize / AOOneChanBufSiz) * NRepeats;
	TotalHalfBufs = ceil(TotalHalfBufs) + 4;
	cout << "Number of half buffers to play: " << TotalHalfBufs << " <---------" << endl << endl << endl;

	/*********************************************/
	// DAQmx Start Code
	/*********************************************/
	DAQmxErrChk(DAQmxStartTask(AIHandle));
	DAQmxErrChk(DAQmxStartTask(AOHandle));
	DAQmxErrChk(DAQmxStartTask(CO2Handle));
	DAQmxErrChk(DAQmxStartTask(CO1Handle));

	while (!done && !_kbhit() && NHalfBufs < TotalHalfBufs) //kbhit() => _kbhit() 10/18/2017 [AWT]
	{
		//********************************************
		// DAQmx record the digital events
		//********************************************
		DAQmxErrChk(DAQmxReadCounterU32(CO1Handle, DAQmx_Val_Auto, CIRecTimeout, &readArrayCICh0[0], ReadBufferSize, &samplesPerChanRead, NULL));
		if (samplesPerChanRead > 0)
		{
			totalReadCh0 += samplesPerChanRead;
			ptrAIFileCh0->write(reinterpret_cast<char*>(&readArrayCICh0[0]), sizeof(uInt32)*samplesPerChanRead);
			if (totalReadCh0%numSampsPerChan == 0) {
				cout << "Total # of Events Recorded on Ch 1: \t" << totalReadCh0 << endl;
			}
		}

		DAQmxErrChk(DAQmxReadCounterU32(CO2Handle, DAQmx_Val_Auto, CIRecTimeout, &readArrayCICh1[0], ReadBufferSize, &samplesPerChanRead, NULL));
		if (samplesPerChanRead > 0)
		{
			totalReadCh1 += samplesPerChanRead;
			ptrAIFileCh1->write(reinterpret_cast<char*>(&readArrayCICh1[0]), sizeof(uInt32)*samplesPerChanRead);
			if (totalReadCh1%numSampsPerChan == 0) {
				cout << "Total # of Events Recorded on Ch 2: \t" << totalReadCh1 << endl;
			}
		}

		//********************************************
		// DAQmx Check Analog Input space available
		//********************************************
		DAQmxErrChk(DAQmxReadAnalogF64(AIHandle, AIOneChanBufSiz, AIRecTimeout, DAQmx_Val_GroupByScanNumber, &dAIBuffer[0], AIBuffer_Siz, &NumAISampRead, NULL));
		if (NumAISampRead>0)
		{
			totalAISampRead += NumAISampRead;
			//cout<<" Total Samples Read Per Channel : \t" << totalAISampRead << endl;
			ptrAIFile->write(reinterpret_cast<char*>(&dAIBuffer[0]), sizeof(double)*NumAISampRead*NumAIChannels);
		}
		//DAQmxErrChk (DAQmxIsTaskDone (AIHandle, &done));

		//********************************************
		// DAQmx Check output buffer space available
		//********************************************
		DAQmxErrChk(DAQmxGetWriteSpaceAvail(AOHandle, &AOBufferSpaceFree));
		if (AOBufferSpaceFree > AOOneChanBufSiz) {
			++NHalfBufs;
			NZSample = ConstructAOBuffer_int16(&dAOBuffer[0], AOOneChanBufSiz, AOFileSize, AO_X_FileSize, ptrAOFile, &X[0], &Y[0], NZSignalRepeat, TotNZSample);
			DAQmxErrChk(DAQmxWriteBinaryI16(AOHandle, AOOneChanBufSiz*NZSignalRepeat, 0, 1, DAQmx_Val_GroupByScanNumber, &dAOBuffer[0], &NumAOSampWritten, NULL));

			TotNZSample += NZSample;
			cout << NHalfBufs << " ; AO Buffer Space: " << AOBufferSpaceFree << " ; AO Samples Written: " << NumAOSampWritten << endl << endl;
		}
	}
	getchar();
	cout << "Number of half buffer transfered: " << NHalfBufs << endl;
	cout << "Number of samples transfered : " << TotNZSample << endl;


Error:
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
	if (CO1Handle != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(CO1Handle);
		DAQmxClearTask(CO1Handle);
	}
	if (CO2Handle != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(CO2Handle);
		DAQmxClearTask(CO2Handle);
	}
	if (AOHandle != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(AOHandle);
		DAQmxClearTask(AOHandle);
	}
	if (AIHandle != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(AIHandle);
		DAQmxClearTask(AIHandle);
	}

	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);

	/*********************************************/
	// DAQmx Reset Device
	/*********************************************/
	ptrAIFile->close();
	ptrAIFileCh0->close();
	ptrAIFileCh1->close();
	ptrAOFile->close();

	DAQmxErrChk(DAQmxResetDevice("Dev2"));

	return;

}