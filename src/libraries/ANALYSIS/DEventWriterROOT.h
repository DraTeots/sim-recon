#ifndef _DEventWriterROOT_
#define _DEventWriterROOT_

#include <map>
#include <string>

#include "TClonesArray.h"
#include "TLorentzVector.h"
#include "TVector3.h"
#include "TTree.h"
#include "TFile.h"
#include "TROOT.h"

#include <JANA/JApplication.h>
#include <JANA/JObject.h>
#include <JANA/JEventLoop.h>

#include <BCAL/DBCALShower.h>
#include <FCAL/DFCALShower.h>
#include <TRACKING/DMCThrown.h>
#include <TRACKING/DTrackTimeBased.h>

#include <PID/DChargedTrack.h>
#include <PID/DBeamPhoton.h>
#include <PID/DChargedTrackHypothesis.h>
#include <PID/DNeutralParticleHypothesis.h>
#include <PID/DNeutralShower.h>
#include <PID/DEventRFBunch.h>

#include <ANALYSIS/DParticleCombo.h>
#include <ANALYSIS/DReaction.h>
#include <ANALYSIS/DAnalysisResults.h>

using namespace std;
using namespace jana;

class DEventWriterROOT : public JObject
{
	public:
		JOBJECT_PUBLIC(DEventWriterROOT);

		DEventWriterROOT(JEventLoop* locEventLoop);
		~DEventWriterROOT(void);

		void Fill_Trees(JEventLoop* locEventLoop) const; //fills all from DAnalysisResults
		void Fill_Tree(JEventLoop* locEventLoop, const DReaction* locReaction, deque<const DParticleCombo*>& locParticleCombos) const;

	private:
		DEventWriterROOT(void){}; //don't allow default constructor

		//UTILITY FUNCTIONS
		void Get_Reactions(jana::JEventLoop* locEventLoop, vector<const DReaction*>& locReactions) const;
		string Convert_ToBranchName(string locInputName) const;
		ULong64_t Calc_ParticleMultiplexID(Particle_t locPID, bool& locIsFinalStateFlag) const;

		//TREE CREATION:
		void Create_Tree(const DReaction* locReaction, bool locIsMCDataFlag);
		void Create_Branches_FinalStateParticle(TTree* locTree, string locParticleBranchName, string locArraySizeString, bool locIsChargedFlag, bool locKinFitFlag);
		void Create_Branches_Beam(TTree* locTree, string locArraySizeString, bool locKinFitFlag);
		void Create_Branches_UnusedParticle(TTree* locTree, string locParticleBranchName, string locArraySizeString);
		void Create_Branches_ThrownParticle(TTree* locTree, string locParticleBranchName, string locArraySizeString);
		template <typename DType> string Create_Branch_Fundamental(TTree* locTree, string locParticleBranchName, string locVariableName, string locTypeString);
		template <typename DType> string Create_Branch_FundamentalArray(TTree* locTree, string locParticleBranchName, string locVariableName, string locArraySizeString, unsigned int locMinimumSize, string locTypeString);
		string Create_Branch_ClonesArray(TTree* locTree, string locParticleBranchName, string locVariableName, string locClassName, unsigned int locSize);

		//TREE FILLING:
		void Fill_ThrownParticleData(TTree* locTree, unsigned int locArrayIndex, unsigned int locMinArraySize, const DMCThrown* locMCThrown) const;
		void Fill_UnusedParticleData(TTree* locTree, unsigned int locArrayIndex, unsigned int locMinArraySize, const DKinematicData* locKinematicData, const map<const DNeutralShower*, unsigned long>& locShowerToIDMap) const;
		void Fill_BeamParticleData(TTree* locTree, unsigned int locArrayIndex, unsigned int locMinArraySize, const DKinematicData* locKinematicData, const DKinematicData* locKinematicData_Measured, const map<const DBeamPhoton*, unsigned long>& locBeamToIDMap) const;
		void Fill_ParticleData(TTree* locTree, unsigned int locArrayIndex, unsigned int locMinArraySize, string locParticleBranchName, const DKinematicData* locKinematicData, const DKinematicData* locKinematicData_Measured, const map<const DNeutralShower*, unsigned long>& locShowerToIDMap) const;
		template <typename DType> DType* Get_BranchAddress(TTree* locTree, string locBranchName, unsigned int locMinimumSize, unsigned int locCurrentSize) const;
		template <typename DType> void Fill_FundamentalData(TTree* locTree, string locVariableName, DType locValue) const;
		template <typename DType> void Fill_FundamentalData(TTree* locTree, string locParticleBranchName, string locVariableName, DType locValue, unsigned int locArrayIndex, unsigned int locMinArraySize, unsigned int locCurrentArraySize) const;
		template <typename DType> void Fill_ClonesData(TTree* locTree, string locParticleBranchName, string locVariableName, DType* locObject, unsigned int locArrayIndex, const map<string, TClonesArray*>& locClonesArrayMap) const;
};

template <typename DType> string DEventWriterROOT::Create_Branch_Fundamental(TTree* locTree, string locParticleBranchName, string locVariableName, string locTypeString)
{
	string locBranchName = (locParticleBranchName != "") ? locParticleBranchName + string("__") + locVariableName : locVariableName;
	string locTypeName = locBranchName + string("/") + locTypeString;
	locTree->Branch(locBranchName.c_str(), new DType(), locTypeName.c_str());
	return locBranchName;
}

template <typename DType> string DEventWriterROOT::Create_Branch_FundamentalArray(TTree* locTree, string locParticleBranchName, string locVariableName, string locArraySizeString, unsigned int locMinimumSize, string locTypeString)
{
	string locBranchName = (locParticleBranchName != "") ? locParticleBranchName + string("__") + locVariableName : locVariableName;
	string locArrayName = locBranchName + string("[") + locArraySizeString + string("]/") + locTypeString;
	locTree->Branch(locBranchName.c_str(), new DType[locMinimumSize], locArrayName.c_str());
	return locBranchName;
}

template <typename DType> DType* DEventWriterROOT::Get_BranchAddress(TTree* locTree, string locBranchName, unsigned int locMinimumSize, unsigned int locCurrentSize) const
{
	//only works for fundamental types!!!
	if(locMinimumSize > locCurrentSize) //creates a new array if it is too small
	{
		delete (DType*)locTree->GetBranch(locBranchName.c_str())->GetAddress();
		locTree->SetBranchAddress(locBranchName.c_str(), new DType[locMinimumSize]);
	}
	return (DType*)locTree->GetBranch(locBranchName.c_str())->GetAddress();
}

template <typename DType> void DEventWriterROOT::Fill_FundamentalData(TTree* locTree, string locVariableName, DType locValue) const
{
	//only call if the variable is NOT an array!!
	Fill_FundamentalData<DType>(locTree, "", locVariableName, locValue, 0, 1, 1);
}

template <typename DType> void DEventWriterROOT::Fill_FundamentalData(TTree* locTree, string locParticleBranchName, string locVariableName, DType locValue, unsigned int locArrayIndex, unsigned int locMinArraySize, unsigned int locCurrentArraySize) const
{
	string locBranchName = (locParticleBranchName != "") ? locParticleBranchName + string("__") + locVariableName : locVariableName;
	DType* locBranchPointer = Get_BranchAddress<DType>(locTree, locBranchName, locMinArraySize, locCurrentArraySize);
	if(locMinArraySize == 1)
		*locBranchPointer = locValue;
	else
		locBranchPointer[locArrayIndex] = locValue;
}

template <typename DType> void DEventWriterROOT::Fill_ClonesData(TTree* locTree, string locParticleBranchName, string locVariableName, DType* locObject, unsigned int locArrayIndex, const map<string, TClonesArray*>& locClonesArrayMap) const
{
	//only call for objects inheriting from TObject*!!!
	string locBranchName = (locParticleBranchName != "") ? locParticleBranchName + string("__") + locVariableName : locVariableName;
	TClonesArray* locClonesArray = locClonesArrayMap.find(locBranchName)->second;
	DType* locConstructedObject = (DType*)locClonesArray->ConstructedAt(locArrayIndex);
	*locConstructedObject = *locObject;
}

#endif //_DEventWriterROOT_


