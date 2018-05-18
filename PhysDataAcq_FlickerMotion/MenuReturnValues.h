#ifndef MENURETURNVALUES_H
#define MENURETURNVALUES_H


#include <string>
#include <ostream>
#include <d:\Scripts\PhysDataAcqScripts\Visual Studio Projects\PhysDataAcq_FlickerMotion\PhysDataAcq_FlickerMotion\NIDAQmx.h>

using namespace std;

class MenuReturnValues
{
public:
	MenuReturnValues();
	~MenuReturnValues(){};

	int		iRecordType;

	string	strRecFileDirPath;
	string	strRecFileBaseName;
	double	dRecSampRate;
	double	dRecMinVoltVal;
	double	dRecMaxVoltVal;
	int		iNumAIChans;
	int		iNumCIChans;
	double	dAIRecTimeOut;
	double	dCIRecTimeOut;
	int32	slDOArraySize;
	int32	slNumSampsPerChan;
	uInt32	ulReadBufferSize;

	string	strStimFileDirPath;
	string	strStimFileName0;
	string	strStimFileName1;
	string	strStimFileName2;
	double	dStimSampRate;
	double	dStimMinVoltVal;
	double	dStimMaxVoltVal;
	int		iNumRepeats;
	int		iNRasterPoints;
	int		iNumNIAOChans;
	int		iNumUEIAOChans;
	double	dStimTimeOut;
	bool	bStimAutoStart;
	int		iStimBufferSize;
	
	// Associated with flicker stimulus
	int		iPersist; 
	string	merge;

	//Associated with PPP stimulus
	int		AN; //added [5/18/2018]
	int		PPPTrial; //added [11/14/2017]

};

#endif