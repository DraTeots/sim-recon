#ifndef _DKinFitUtils_GlueX_
#define _DKinFitUtils_GlueX_

#include <deque>
#include <vector>
#include <map>
#include <set>

#include "TVector3.h"
#include "TMatrixFSym.h"
#include "TLorentzVector.h"

#include "particleType.h"

#include "DANA/DApplication.h"
#include "HDGEOMETRY/DMagneticFieldMap.h"
#include "HDGEOMETRY/DMagneticFieldMapNoField.h"
#include "PID/DBeamPhoton.h"
#include "PID/DNeutralShower.h"
#include "PID/DKinematicData.h"
#include "PID/DNeutralParticleHypothesis.h"

#include "PID/DChargedTrackHypothesis.h"
#include "PID/DNeutralParticleHypothesis.h"
#include "PID/DNeutralShower.h"
#include "PID/DKinematicData.h"
#include "PID/DBeamPhoton.h"

#include "KINFITTER/DKinFitter.h"
#include "KINFITTER/DKinFitUtils.h"

#include "ANALYSIS/DAnalysisUtilities.h"

using namespace std;

class DKinFitUtils_GlueX : public DKinFitUtils
{
	public:

		/***************************************************************** INITIALIZE ***************************************************************/

		//CONSTRUCTORS //call either one
		DKinFitUtils_GlueX(JEventLoop* locEventLoop);
		//useful for manually using a different field:
		DKinFitUtils_GlueX(const DMagneticFieldMap* locMagneticFieldMap, const DAnalysisUtilities* locAnalysisUtilities);

		void Reset_NewEvent(uint64_t locEventNumber);
		void Reset_NewEvent(void);
		void Set_MaxPoolSizes(size_t locNumReactions, size_t locExpectedNumCombos);

		/************************************************************** CREATE PARTICLES ************************************************************/

		//If multiple constraints, it is EXTREMELY CRITICAL that only one DKinFitParticle be created per particle, 
			//so that the particles are correctly linked across constraints!!
		//If a particle has already been created from this source object, will instead just return the originally-created input kinfit particle
			//This particle is guaranteed to be unchanged after it is created. Instead of updating it, the kinematic fitter clones a new (output) copy

		DKinFitParticle* Make_BeamParticle(const DBeamPhoton* locBeamPhoton);
		DKinFitParticle* Make_BeamParticle(const DBeamPhoton* locBeamPhoton, const DEventRFBunch* locEventRFBunch); //sets rf time for photon
		DKinFitParticle* Make_DetectedParticle(const DKinematicData* locKinematicData);
		using DKinFitUtils::Make_DetectedParticle; //this is necessary because the above declaration hides the base class function, which is needed by DKinFitResults_factory

		DKinFitParticle* Make_DetectedShower(const DNeutralShower* locNeutralShower, Particle_t locPID); //DO NOT call this unless the neutral is also in a vertex fit!
		DKinFitParticle* Make_TargetParticle(Particle_t locPID);
		DKinFitParticle* Make_DecayingParticle(Particle_t locPID, const set<DKinFitParticle*>& locFromInitialState, const set<DKinFitParticle*>& locFromFinalState);
		DKinFitParticle* Make_MissingParticle(Particle_t locPID);

		size_t Get_KinFitParticlePoolSize_Shared(void) const;
		size_t Get_KinFitParticlePoolSize(void) const{return dKinFitParticlePool_Acquired.size();};

		/************************************************************** RETURN MAPPING **************************************************************/

		const JObject* Get_SourceJObject(DKinFitParticle* locInputKinFitParticle) const;

		/************************************************************ CREATE DKINFITCHAIN ***********************************************************/

		//optional: can help make constraints
		const DKinFitChain* Make_KinFitChain(const DParticleCombo* locParticleCombo, DKinFitType locKinFitType);

		/************************************************************* CREATE CONSTRAINTS ***********************************************************/

		set<DKinFitConstraint*> Create_Constraints(const DParticleCombo* locParticleCombo, const DKinFitChain* locKinFitChain, DKinFitType locKinFitType, deque<DKinFitConstraint_Vertex*>& locSortedVertexConstraints);

		/*********************************************************** MAKE SPACETIME GUESSES *********************************************************/

		void Set_SpacetimeGuesses(const deque<DKinFitConstraint_Vertex*>& locSortedVertexConstraints, bool locIsP4FitFlag);

		/************************************************************ CONSTRAINT PREDICTORS *********************************************************/

		//These functions are necessary to determine:
			//Whether each neutral is in a vertex constraint or not (may not be enough particles to constrain that particular vertex)
			//The pull terms needed when creating histograms
			//The constraint strings for the confidence level histogram

		set<pair<int, int> > Get_KinFitVertexParticles(const DReaction* locReaction) const;
		string Get_ConstraintInfo(const DReaction* locReaction, DKinFitType locKinFitType, size_t& locNumConstraints, size_t& locNumUnknowns) const;

		/*********************************************************** CALCULATION ROUTINES ***********************************************************/

		bool Propagate_TrackInfoToCommonVertex(DKinematicData* locKinematicData, DKinFitParticle* locKinFitParticle, const TMatrixDSym* locVXi);

		inline TVector3 Make_TVector3(DVector3 locDVector3) const;
		inline TLorentzVector Make_TLorentzVector(DLorentzVector locDLorentzVector) const;

		/******************************************************* OVERRIDE BASE CLASS FUNCTIONS ******************************************************/

		bool Get_IncludeBeamlineInVertexFitFlag(void) const;
		bool Get_IsDetachedVertex(int locPDG_PID) const;

		TVector3 Get_BField(const TVector3& locPosition) const; //must return in units of Tesla!!
		bool Get_IsBFieldNearBeamline(void) const;

	private:

		//PRIVATE DEFAULT CONSTRUCTOR
		DKinFitUtils_GlueX(void){} //Cannot use default constructor. Must construct with DMagneticFieldMap as argument

		/************************************************************** CREATE PARTICLES ************************************************************/

		//This method is only designed to be used when forming the vertex guesses.
		//It creates a new particle from an existing particle, cloning the covariance matrix
		//To recycle this memory between fits, the caller is responsible for doing so (between events is done automatically)
		DKinFitParticle* Make_DetectedParticle(DKinFitParticle* locDecayingKinFitParticle);

		/************************************************************ CREATE DKINFITCHAIN ***********************************************************/

		void Make_KinFitChainStep(const DParticleCombo* locParticleCombo, DKinFitType locKinFitType, size_t locStepIndex, DKinFitChain* locKinFitChain, DKinFitChainStep* locKinFitChainStep, map<size_t, size_t>& locStepCreationMap);

		/*********************************************************** MAKE SPACETIME GUESSES *********************************************************/

		void Construct_DetectedDecayingParticle_NoFit(DKinFitConstraint_Vertex* locOrigVertexConstraint, map<DKinFitParticle*, DKinFitParticle*>& locDetectedDecayingParticleMap, TLorentzVector locSpacetimeVertexGuess);

		DKinFitConstraint_Vertex* Build_NewConstraint(DKinFitConstraint_Vertex* locOrigVertexConstraint, const map<DKinFitParticle*, DKinFitParticle*>& locDetectedDecayingParticleMap, bool& locAttemptFitFlag, bool locSkipBadDecayingFlag);

		double Calc_TimeGuess(const DKinFitConstraint_Spacetime* locConstraint, DVector3 locVertexGuess);

		/************************************************************ CONSTRAINT PREDICTORS *********************************************************/

		string Build_VertexConstraintString(const DReaction* locReaction, const set<pair<int, int> >& locAllVertexParticles, set<pair<int, int> >& locFullConstrainParticles, set<pair<int, int> >& locOnlyConstrainTimeParticles, set<pair<int, int> >& locNoConstrainParticles, bool locSpacetimeFitFlag) const;

		deque<set<pair<int, int> > > Setup_VertexPredictions(const DReaction* locReaction) const;
		void Setup_VertexPrediction(const DReaction* locReaction, size_t locStepIndex, set<pair<int, int> >& locVertexParticles, const map<pair<int, int>, int>& locDecayMap_ParticleToDecayStep, set<size_t>& locIncludedStepIndices) const;

		deque<set<pair<int, int> > > Predict_VertexConstraints(const DReaction* locReaction, deque<set<pair<int, int> > > locAllVertices, bool locSpacetimeFitFlag, size_t& locNumConstraints, string& locAllConstraintString) const;
		void Group_VertexParticles(const DReaction* locReaction, bool locSpacetimeFitFlag, const set<pair<int, int> >& locVertexParticles, set<pair<int, int> >& locFullConstrainParticles, set<pair<int, int> >& locDecayingParticles, set<pair<int, int> >& locOnlyConstrainTimeParticles, set<pair<int, int> >& locNoConstrainParticles) const;

		/*************************************************************** NESTED CLASS ***************************************************************/

		class DDecayingParticleInfo
		{
			public:
				DDecayingParticleInfo(Particle_t locPID, const set<DKinFitParticle*>& locFromInitialState, const set<DKinFitParticle*>& locFromFinalState) : 
				dPID(locPID), dFromInitialState(locFromInitialState), dFromFinalState(locFromFinalState) {}

				bool operator<(const DDecayingParticleInfo& locDecayingParticleInfo) const;

				Particle_t dPID;
				set<DKinFitParticle*> dFromInitialState;
				set<DKinFitParticle*> dFromFinalState;
		};

		/************************************************************ MAGNETIC FIELD MAP ************************************************************/

		const DMagneticFieldMap* dMagneticFieldMap;
		const DAnalysisUtilities* dAnalysisUtilities;

		/************************************************************* PARTICLE MAPPING *************************************************************/

		//PARTICLE MAPPING
			//Particles are created like so: source -> input kinfit particle -> output kinfit particle
			//Cannot map input -> output: many outputs for a given input (same particle used in multiple kinfits)
			//can map: source -> input, input -> source, output -> input (base class)

		//MAP: SOURCE -> KINFIT INPUT
			//Needed internally for cloning
		map<pair<const DBeamPhoton*, const DEventRFBunch*>, DKinFitParticle*> dParticleMap_SourceToInput_Beam;
		map<const DKinematicData*, DKinFitParticle*> dParticleMap_SourceToInput_DetectedParticle;
		map<pair<const DNeutralShower*, Particle_t>, DKinFitParticle*> dParticleMap_SourceToInput_Shower;
		map<Particle_t, DKinFitParticle*> dParticleMap_SourceToInput_Target;
		map<DDecayingParticleInfo, DKinFitParticle*> dParticleMap_SourceToInput_Decaying;
		map<Particle_t, DKinFitParticle*> dParticleMap_SourceToInput_Missing;

		//MAP: KINFIT INPUT -> SOURCE
			//no maps for missing or target: would just map to PID
			//needed for getting back to the source particle
		map<DKinFitParticle*, const JObject*> dParticleMap_InputToSource_JObject;
		map<DKinFitParticle*, DDecayingParticleInfo> dParticleMap_InputToSource_Decaying;

		/************************************************************ MEMORY RESOURCES **************************************************************/

		 //use DANA global resource pool instead
		TMatrixFSym* Get_SymMatrixResource(unsigned int locNumMatrixRows);
		void Recycle_DetectedDecayingParticles(map<DKinFitParticle*, DKinFitParticle*>& locDecayingToDetectedParticleMap);

		deque<DKinFitParticle*>& Get_AvailableParticleDeque(void) const; //returns reference to static (shared-amongst-threads) deque
		DKinFitParticle* Get_KinFitParticleResource(void);
		void Reset_ParticleMemory(void);
		void Acquire_Particles(size_t locNumRequestedParticles);

		//acquired from the shared pool for this event
		deque<DKinFitParticle*> dKinFitParticlePool_Acquired;

		size_t dTargetMaxNumAvailableParticles;
		size_t dNumFillBufferParticles;

		/************************************************************** MISCELLANEOUS ***************************************************************/

		DApplication* dApplication;
		bool dWillBeamHaveErrorsFlag;
		uint64_t dEventNumber;
		size_t dNumFillBufferMatrices; //when no matrix resources at hand, the number of matrices to request from DApplication
};

inline TVector3 DKinFitUtils_GlueX::Make_TVector3(DVector3 locDVector3) const
{
	return TVector3(locDVector3.X(), locDVector3.Y(), locDVector3.Z());
}

inline TLorentzVector DKinFitUtils_GlueX::Make_TLorentzVector(DLorentzVector locDLorentzVector) const
{
	return TLorentzVector(locDLorentzVector.X(), locDLorentzVector.Y(), locDLorentzVector.Z(), locDLorentzVector.T());
}

inline const JObject* DKinFitUtils_GlueX::Get_SourceJObject(DKinFitParticle* locInputKinFitParticle) const
{
	DKinFitParticleType locKinFitParticleType = locInputKinFitParticle->Get_KinFitParticleType();
	if((locKinFitParticleType == d_DecayingParticle) || (locKinFitParticleType == d_MissingParticle) || (locKinFitParticleType == d_TargetParticle))
		return NULL;

	map<DKinFitParticle*, const JObject*>::const_iterator locIterator = dParticleMap_InputToSource_JObject.find(locInputKinFitParticle);
	return ((locIterator != dParticleMap_InputToSource_JObject.end()) ? locIterator->second : NULL);
}

inline bool DKinFitUtils_GlueX::DDecayingParticleInfo::operator<(const DKinFitUtils_GlueX::DDecayingParticleInfo& locDecayingParticleInfo) const
{
	if(dPID < locDecayingParticleInfo.dPID)
		return true;
	else if(dPID > locDecayingParticleInfo.dPID)
		return false;

	if(dFromInitialState < locDecayingParticleInfo.dFromInitialState)
		return true;
	else if(dFromInitialState > locDecayingParticleInfo.dFromInitialState)
		return false;

	return (dFromFinalState < locDecayingParticleInfo.dFromFinalState);
}

inline TMatrixFSym* DKinFitUtils_GlueX::Get_SymMatrixResource(unsigned int locNumMatrixRows)
{
	//if kinfit pool (buffer) is empty, use DApplication global pool to retrieve a new batch of matrices
	if(Get_SymMatrixPoolAvailableSize() == 0)
	{
		deque<TMatrixFSym*> locMatrices = dApplication->Get_CovarianceMatrixResources(locNumMatrixRows, dNumFillBufferMatrices, dEventNumber);

		//Then store them to the buffer by "recycling" them
			//these only live in the "available" pool, and aren't set in the "all" pool
			//when the pools are reset for a new event, the buffer is cleared and the utils forget all about them
			//thus, the memory is managed by the DApplication, and only by the DApplication
		Recycle_Matrices(locMatrices); 
	}

	//now, retrieve one from the buffer pool
	return DKinFitUtils::Get_SymMatrixResource(locNumMatrixRows);
}

#endif // _DKinFitUtils_GlueX_

