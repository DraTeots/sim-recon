#include "ANALYSIS/DHistogramActions.h"

void DHistogramAction_PID::Initialize(JEventLoop* locEventLoop)
{
	string locHistName, locHistTitle;
	string locParticleName, locParticleName2, locParticleROOTName, locParticleROOTName2;
	Particle_t locPID, locPID2;

	vector<const DParticleID*> locParticleIDs;
	locEventLoop->Get(locParticleIDs);
	dParticleID = locParticleIDs[0];

	//setup pid deques: searched for and generated
	deque<Particle_t> locDesiredPIDs, locThrownPIDs;
	Get_Reaction()->Get_DetectedFinalPIDs(locDesiredPIDs);
	vector<const DMCThrown*> locMCThrowns;
	locEventLoop->Get(locMCThrowns);
	if(!locMCThrowns.empty()) //else real data, not MC!!
	{
		DMCThrownMatching_factory* locMCThrownMatchingFactory = static_cast<DMCThrownMatching_factory*>(locEventLoop->GetFactory("DMCThrownMatching"));
		locMCThrownMatchingFactory->Get_MCThrownComparisonPIDs(locThrownPIDs);
		locThrownPIDs.push_back(Unknown); //unmatched tracks
	}

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();
	for(size_t loc_i = 0; loc_i < locDesiredPIDs.size(); ++loc_i)
	{
		locPID = locDesiredPIDs[loc_i];
		locParticleName = ParticleType(locPID);
		locParticleROOTName = ParticleName_ROOT(locPID);
		CreateAndChangeTo_Directory(locParticleName, locParticleName);

		// Confidence Level
		locHistName = "PIDConfidenceLevel";
		locHistTitle = locParticleROOTName + string(" PID;PID Confidence Level");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_PIDFOM[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_PIDFOM[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumFOMBins, 0.0, 1.0);

		if(ParticleCharge(locPID) != 0) //no other sources of PID for neutrals
		{
			// TOF Confidence Level
			locHistName = "TOFConfidenceLevel";
			locHistTitle = locParticleROOTName + string(" PID;TOF Confidence Level");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistMap_TOFFOM[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistMap_TOFFOM[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumFOMBins, 0.0, 1.0);

			// DC dE/dx Confidence Level
			locHistName = "DCdEdxConfidenceLevel";
			locHistTitle = locParticleROOTName + string(" PID;DC #it{#frac{dE}{dx}} Confidence Level");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistMap_DCdEdxFOM[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistMap_DCdEdxFOM[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumFOMBins, 0.0, 1.0);
		}

		//one per thrown pid:
		for(size_t loc_j = 0; loc_j < locThrownPIDs.size(); ++loc_j)
		{
			locPID2 = locThrownPIDs[loc_j];
			if((ParticleCharge(locPID2) != ParticleCharge(locPID)) && (locPID2 != Unknown))
				continue;
			locParticleName2 = ParticleType(locPID2);
			locParticleROOTName2 = ParticleName_ROOT(locPID2);

			//Confidence Level for Thrown PID
			pair<Particle_t, Particle_t> locPIDPair(locPID, locPID2);
			locHistName = string("PIDConfidenceLevel_ForThrownPID_") + locParticleName2;
			locHistTitle = locParticleROOTName + string(" PID, Thrown PID = ") + locParticleROOTName2 + string(";PID Confidence Level");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistMap_PIDFOMForTruePID[locPIDPair] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistMap_PIDFOMForTruePID[locPIDPair] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumFOMBins, 0.0, 1.0);
		}

		//beta vs p
		locHistName = "BetaVsP";
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#beta");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_BetaVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_BetaVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumBetaBins, dMinBeta, dMaxBeta);

		//delta-beta vs p
		locHistName = "DeltaBetaVsP";
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#Delta#beta");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaBetaVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaBetaVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaBetaBins, dMinDeltaBeta, dMaxDeltaBeta);

		//TOF Confidence Level vs Delta-Beta
		locHistName = "TOFConfidenceLevelVsDeltaBeta";
		locHistTitle = locParticleROOTName + string(";#Delta#beta;TOF Confidence Level");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_TOFFOMVsDeltaBeta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_TOFFOMVsDeltaBeta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaBetaBins, dMinDeltaBeta, dMaxDeltaBeta, dNumFOMBins, 0.0, 1.0);

		// P Vs Theta, PID FOM < 1%
		locHistName = "PVsTheta_LowPIDFOM";
		locHistTitle = locParticleROOTName + string(", PID FOM < 1%;#theta#circ;p (GeV/c)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_PVsTheta_LowPIDFOM[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_PVsTheta_LowPIDFOM[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

		// P Vs Theta, PID FOM = NaN
		locHistName = "PVsTheta_NaNPIDFOM";
		locHistTitle = locParticleROOTName + string(", PID FOM = NaN;#theta#circ;p (GeV/c)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_PVsTheta_NaNPIDFOM[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_PVsTheta_NaNPIDFOM[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

		if(ParticleCharge(locPID) != 0) //no other sources of PID for neutrals
		{
			// P Vs Theta, TOF FOM < 1%
			locHistName = "PVsTheta_LowTOFFOM";
			locHistTitle = locParticleROOTName + string(", TOF FOM < 1%;#theta#circ;p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistMap_PVsTheta_LowTOFFOM[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistMap_PVsTheta_LowTOFFOM[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

			// P Vs Theta, TOF FOM = NaN
			locHistName = "PVsTheta_NaNTOFFOM";
			locHistTitle = locParticleROOTName + string(", TOF FOM = NaN;#theta#circ;p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistMap_PVsTheta_NaNTOFFOM[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistMap_PVsTheta_NaNTOFFOM[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

			// P Vs Theta, Beta < 0
			locHistName = "PVsTheta_NegativeBeta";
			locHistTitle = locParticleROOTName + string(", #beta < 0.0;#theta#circ;p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistMap_PVsTheta_NegativeBeta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistMap_PVsTheta_NegativeBeta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

			// P Vs Theta, DC dE/dx FOM < 1%
			locHistName = "PVsTheta_LowDCdEdxFOM";
			locHistTitle = locParticleROOTName + string(", DC #it{#frac{dE}{dx}} FOM < 1%;#theta#circ;p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistMap_PVsTheta_LowDCdEdxFOM[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistMap_PVsTheta_LowDCdEdxFOM[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

			// P Vs Theta, DC dE/dx FOM = NaN
			locHistName = "PVsTheta_NaNDCdEdxFOM";
			locHistTitle = locParticleROOTName + string(", DC #it{#frac{dE}{dx}} FOM = NaN;#theta#circ;p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistMap_PVsTheta_NaNDCdEdxFOM[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistMap_PVsTheta_NaNDCdEdxFOM[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);
		}

		gDirectory->cd("..");
	} //end of PID loop

	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
} //end of Initialize() function

bool DHistogramAction_PID::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	vector<const DMCThrownMatching*> locMCThrownMatchingVector;
	locEventLoop->Get(locMCThrownMatchingVector);
	const DMCThrownMatching* locMCThrownMatching = locMCThrownMatchingVector.empty() ? NULL : locMCThrownMatchingVector[0];

	vector<const DEventRFBunch*> locEventRFBunches;
	locEventLoop->Get(locEventRFBunches);
	const DEventRFBunch* locEventRFBunch = locEventRFBunches[0];

	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);

	deque<const DKinematicData*> locParticles;
	for(size_t loc_i = 0; loc_i < locParticleCombo->Get_NumParticleComboSteps(); ++loc_i)
	{
		const DParticleComboStep* locParticleComboStep = locParticleCombo->Get_ParticleComboStep(loc_i);

		//charged tracks
		locParticleComboStep->Get_DetectedFinalChargedParticles_Measured(locParticles);
		for(size_t loc_j = 0; loc_j < locParticles.size(); ++loc_j)
		{
			if(!Get_AnalysisUtilities()->Find_SimilarCombos(locParticles[loc_j], locPreviousParticleCombos)) //else duplicate
				Fill_ChargedHists(static_cast<const DChargedTrackHypothesis*>(locParticles[loc_j]), locMCThrownMatching, locEventRFBunch);
		}

		//neutral particles
		locParticleComboStep->Get_DetectedFinalNeutralParticles_Measured(locParticles);
		for(size_t loc_j = 0; loc_j < locParticles.size(); ++loc_j)
		{
			if(!Get_AnalysisUtilities()->Find_SimilarCombos(locParticles[loc_j], locPreviousParticleCombos)) //else duplicate
				Fill_NeutralHists(static_cast<const DNeutralParticleHypothesis*>(locParticles[loc_j]), locMCThrownMatching, locEventRFBunch);
		}
	}

	return true;
}

void DHistogramAction_PID::Fill_ChargedHists(const DChargedTrackHypothesis* locChargedTrackHypothesis, const DMCThrownMatching* locMCThrownMatching, const DEventRFBunch* locEventRFBunch)
{
	Particle_t locPID = locChargedTrackHypothesis->PID();
	double locDeltaBeta, locBeta_Timing;

	double locStartTime, locStartTimeVariance;
	bool locUsedRFTimeFlag;
	bool locFoundStartTimeFlag = dParticleID->Calc_TrackStartTime(locChargedTrackHypothesis, locEventRFBunch, locStartTime, locStartTimeVariance, locUsedRFTimeFlag);
	locBeta_Timing = locFoundStartTimeFlag ? locChargedTrackHypothesis->pathLength()/(SPEED_OF_LIGHT*(locChargedTrackHypothesis->t1() - locStartTime)) : numeric_limits<double>::quiet_NaN();
	locDeltaBeta = locChargedTrackHypothesis->lorentzMomentum().Beta() - locBeta_Timing;
	double locFOM_Timing = (locChargedTrackHypothesis->dNDF_Timing > 0) ? TMath::Prob(locChargedTrackHypothesis->dChiSq_Timing, locChargedTrackHypothesis->dNDF_Timing) : numeric_limits<double>::quiet_NaN();
	double locFOM_DCdEdx = (locChargedTrackHypothesis->dNDF_DCdEdx > 0) ? TMath::Prob(locChargedTrackHypothesis->dChiSq_DCdEdx, locChargedTrackHypothesis->dNDF_DCdEdx) : numeric_limits<double>::quiet_NaN();

	double locP = locChargedTrackHypothesis->momentum().Mag();
	double locTheta = locChargedTrackHypothesis->momentum().Theta()*180.0/TMath::Pi();
	const DMCThrown* locMCThrown = (locMCThrownMatching != NULL) ? locMCThrownMatching->Get_MatchingMCThrown(locChargedTrackHypothesis) : NULL;

	Get_Application()->RootWriteLock();
	dHistMap_PIDFOM[locPID]->Fill(locChargedTrackHypothesis->dFOM);
	dHistMap_TOFFOM[locPID]->Fill(locFOM_Timing);
	dHistMap_DCdEdxFOM[locPID]->Fill(locFOM_DCdEdx);
	dHistMap_BetaVsP[locPID]->Fill(locP, locBeta_Timing);
	dHistMap_DeltaBetaVsP[locPID]->Fill(locP, locDeltaBeta);
	dHistMap_TOFFOMVsDeltaBeta[locPID]->Fill(locDeltaBeta, locFOM_Timing);
	pair<Particle_t, Particle_t> locPIDPair(locPID, Unknown); //default unless matched
	if(locMCThrown != NULL) //else bogus track (not matched to any thrown tracks)
		locPIDPair.second = (Particle_t)(locMCThrown->type); //matched
	if(dHistMap_PIDFOMForTruePID.find(locPIDPair) != dHistMap_PIDFOMForTruePID.end()) //else hist not created or PID is weird
		dHistMap_PIDFOMForTruePID[locPIDPair]->Fill(locChargedTrackHypothesis->dFOM);

	if((locChargedTrackHypothesis->dFOM < 0.01) && (locChargedTrackHypothesis->dNDF > 0))
		dHistMap_PVsTheta_LowPIDFOM[locPID]->Fill(locTheta, locP);
	else if(locChargedTrackHypothesis->dNDF == 0) //NaN
		dHistMap_PVsTheta_NaNPIDFOM[locPID]->Fill(locTheta, locP);

	if(locFOM_Timing < 0.01)
		dHistMap_PVsTheta_LowTOFFOM[locPID]->Fill(locTheta, locP);
	else if(locChargedTrackHypothesis->dNDF_Timing == 0) //NaN
		dHistMap_PVsTheta_NaNTOFFOM[locPID]->Fill(locTheta, locP);

	if(locFOM_DCdEdx < 0.01)
		dHistMap_PVsTheta_LowDCdEdxFOM[locPID]->Fill(locTheta, locP);
	else if(locChargedTrackHypothesis->dNDF_DCdEdx == 0) //NaN
		dHistMap_PVsTheta_NaNDCdEdxFOM[locPID]->Fill(locTheta, locP);

	if(locBeta_Timing < 0.0)
		dHistMap_PVsTheta_NegativeBeta[locPID]->Fill(locTheta, locP);

	Get_Application()->RootUnLock();
}

void DHistogramAction_PID::Fill_NeutralHists(const DNeutralParticleHypothesis* locNeutralParticleHypothesis, const DMCThrownMatching* locMCThrownMatching, const DEventRFBunch* locEventRFBunch)
{
	Particle_t locPID = locNeutralParticleHypothesis->PID();

	double locStartTime = locEventRFBunch->dMatchedToTracksFlag ? locEventRFBunch->dTime : numeric_limits<double>::quiet_NaN();
	double locBeta_Timing = locNeutralParticleHypothesis->pathLength()/(SPEED_OF_LIGHT*(locNeutralParticleHypothesis->t1() - locStartTime));
	double locDeltaBeta = locNeutralParticleHypothesis->lorentzMomentum().Beta() - locBeta_Timing;

	double locP = locNeutralParticleHypothesis->momentum().Mag();
	double locTheta = locNeutralParticleHypothesis->momentum().Theta()*180.0/TMath::Pi();
	const DMCThrown* locMCThrown = (locMCThrownMatching != NULL) ? locMCThrownMatching->Get_MatchingMCThrown(locNeutralParticleHypothesis) : NULL;

	Get_Application()->RootWriteLock();
	dHistMap_PIDFOM[locPID]->Fill(locNeutralParticleHypothesis->dFOM);
	dHistMap_BetaVsP[locPID]->Fill(locP, locBeta_Timing);
	dHistMap_DeltaBetaVsP[locPID]->Fill(locP, locDeltaBeta);
	dHistMap_TOFFOMVsDeltaBeta[locPID]->Fill(locDeltaBeta, locNeutralParticleHypothesis->dFOM);
	pair<Particle_t, Particle_t> locPIDPair(locPID, Unknown); //default unless matched
	if(locMCThrown != NULL) //else bogus track (not matched to any thrown tracks)
		locPIDPair.second = (Particle_t)(locMCThrown->type); //matched
	if(dHistMap_PIDFOMForTruePID.find(locPIDPair) != dHistMap_PIDFOMForTruePID.end()) //else hist not created or PID is weird
		dHistMap_PIDFOMForTruePID[locPIDPair]->Fill(locNeutralParticleHypothesis->dFOM);

	if((locNeutralParticleHypothesis->dFOM < 0.01) && (locNeutralParticleHypothesis->dNDF > 0))
		dHistMap_PVsTheta_LowPIDFOM[locPID]->Fill(locTheta, locP);
	else if(locNeutralParticleHypothesis->dNDF == 0) //NaN
		dHistMap_PVsTheta_NaNPIDFOM[locPID]->Fill(locTheta, locP);

	Get_Application()->RootUnLock();
}

void DHistogramAction_TrackVertexComparison::Initialize(JEventLoop* locEventLoop)
{
	deque<deque<Particle_t> > locDetectedChargedPIDs;
	Get_Reaction()->Get_DetectedFinalChargedPIDs(locDetectedChargedPIDs);

	deque<deque<Particle_t> > locDetectedChargedPIDs_HasDupes;
	Get_Reaction()->Get_DetectedFinalChargedPIDs(locDetectedChargedPIDs_HasDupes, true);

	string locHistName, locHistTitle, locStepName, locStepROOTName, locParticleName, locParticleROOTName;
	Particle_t locPID, locHigherMassPID, locLowerMassPID;
	string locHigherMassParticleName, locLowerMassParticleName, locHigherMassParticleROOTName, locLowerMassParticleROOTName;

	size_t locNumSteps = Get_Reaction()->Get_NumReactionSteps();
	dHistDeque_TrackZToCommon.resize(locNumSteps);
	dHistDeque_TrackTToCommon.resize(locNumSteps);
	dHistDeque_TrackDOCAToCommon.resize(locNumSteps);
	dHistDeque_MaxTrackDeltaZ.resize(locNumSteps);
	dHistDeque_MaxTrackDeltaT.resize(locNumSteps);
	dHistDeque_MaxTrackDOCA.resize(locNumSteps);
	dHistDeque_TrackDeltaTVsP.resize(locNumSteps);

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();
	for(size_t loc_i = 0; loc_i < locNumSteps; ++loc_i)
	{
		if(locDetectedChargedPIDs[loc_i].empty())
			continue;

		const DReactionStep* locReactionStep = Get_Reaction()->Get_ReactionStep(loc_i);
		locStepName = locReactionStep->Get_StepName();
		locStepROOTName = locReactionStep->Get_StepROOTName();
		CreateAndChangeTo_Directory(locStepName, locStepName);

		// Max Track DeltaZ
		locHistName = "MaxTrackDeltaZ";
		locHistTitle = locStepROOTName + string(";Largest Track #DeltaVertex-Z (cm)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistDeque_MaxTrackDeltaZ[loc_i] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistDeque_MaxTrackDeltaZ[loc_i] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaVertexZBins, 0.0, dMaxDeltaVertexZ);

		// Max Track DeltaT
		locHistName = "MaxTrackDeltaT";
		locHistTitle = locStepROOTName + string(";Largest Track #DeltaVertex-T (ns)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistDeque_MaxTrackDeltaT[loc_i] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistDeque_MaxTrackDeltaT[loc_i] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaVertexTBins, 0.0, dMaxDeltaVertexT);

		// Max Track DOCA
		locHistName = "MaxTrackDOCA";
		locHistTitle = locStepROOTName + string(";Largest Track DOCA (cm)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistDeque_MaxTrackDOCA[loc_i] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistDeque_MaxTrackDOCA[loc_i] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDOCABins, 0.0, dMaxDOCA);

		for(size_t loc_j = 0; loc_j < locDetectedChargedPIDs[loc_i].size(); ++loc_j)
		{
			locPID = locDetectedChargedPIDs[loc_i][loc_j];
			if(dHistDeque_TrackZToCommon[loc_i].find(locPID) != dHistDeque_TrackZToCommon[loc_i].end())
				continue; //already created for this pid
			locParticleName = ParticleType(locPID);
			locParticleROOTName = ParticleName_ROOT(locPID);

			// TrackZ To Common
			locHistName = string("TrackZToCommon_") + locParticleName;
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";#DeltaVertex-Z (Track, Common) (cm)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_TrackZToCommon[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_TrackZToCommon[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaVertexZBins, dMinDeltaVertexZ, dMaxDeltaVertexZ);

			// TrackT To Common
			locHistName = string("TrackTToCommon_") + locParticleName;
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";#DeltaVertex-T (Track, Common) (ns)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_TrackTToCommon[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_TrackTToCommon[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaVertexTBins, dMinDeltaVertexT, dMaxDeltaVertexT);

			// TrackDOCA To Common
			locHistName = string("TrackDOCAToCommon_") + locParticleName;
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";DOCA (Track, Common) (cm)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_TrackDOCAToCommon[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_TrackDOCAToCommon[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDOCABins, dMinDOCA, dMaxDOCA);

			// DeltaT Vs P against beam photon
			if((locReactionStep->Get_InitialParticleID() == Gamma) && (dHistMap_BeamTrackDeltaTVsP.find(locPID) == dHistMap_BeamTrackDeltaTVsP.end()))
			{
				locHistName = string("TrackDeltaTVsP_") + ParticleType(locPID) + string("_Beam") + ParticleType(Gamma);
				locHistTitle = locStepROOTName + string(";") + ParticleName_ROOT(locPID) + string(" Momentum (GeV/c);t_{") + ParticleName_ROOT(locPID) + string("} - t_{Beam ") + ParticleName_ROOT(Gamma) + string("} (ns)");
				if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
					dHistMap_BeamTrackDeltaTVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
				else
					dHistMap_BeamTrackDeltaTVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaVertexTBins, dMinDeltaVertexT, dMaxDeltaVertexT);
			}
		}

		//delta-t vs p
		for(int loc_j = 0; loc_j < int(locDetectedChargedPIDs_HasDupes[loc_i].size()) - 1; ++loc_j)
		{
			locPID = locDetectedChargedPIDs_HasDupes[loc_i][loc_j];
			for(size_t loc_k = loc_j + 1; loc_k < locDetectedChargedPIDs_HasDupes[loc_i].size(); ++loc_k)
			{
				if(ParticleMass(locDetectedChargedPIDs_HasDupes[loc_i][loc_k]) > ParticleMass(locPID))
				{
					locHigherMassPID = locDetectedChargedPIDs_HasDupes[loc_i][loc_k];
					locLowerMassPID = locPID;
				}
				else
				{
					locHigherMassPID = locPID;
					locLowerMassPID = locDetectedChargedPIDs_HasDupes[loc_i][loc_k];
				}
				pair<Particle_t, Particle_t> locParticlePair(locHigherMassPID, locLowerMassPID);
				if(dHistDeque_TrackDeltaTVsP[loc_i].find(locParticlePair) != dHistDeque_TrackDeltaTVsP[loc_i].end())
					continue; //already created for this pair

				locHigherMassParticleName = ParticleType(locHigherMassPID);
				locHigherMassParticleROOTName = ParticleName_ROOT(locHigherMassPID);
				locLowerMassParticleName = ParticleType(locLowerMassPID);
				locLowerMassParticleROOTName = ParticleName_ROOT(locLowerMassPID);

				// DeltaT Vs P
				locHistName = string("TrackDeltaTVsP_") + locHigherMassParticleName + string("_") + locLowerMassParticleName;
				locHistTitle = locStepROOTName + string(";") + locHigherMassParticleROOTName + string(" Momentum (GeV/c);t_{") + locHigherMassParticleROOTName + string("} - t_{") + locLowerMassParticleROOTName + string("} (ns)");
				if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
					dHistDeque_TrackDeltaTVsP[loc_i][locParticlePair] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
				else
					dHistDeque_TrackDeltaTVsP[loc_i][locParticlePair] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaVertexTBins, dMinDeltaVertexT, dMaxDeltaVertexT);
			}
		}
		gDirectory->cd("..");
	}

	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_TrackVertexComparison::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	deque<const DKinematicData*> locParticles;
	DVector3 locVertex;
	double locVertexTime;
	Particle_t locPID;
	double locDOCA, locDeltaZ, locDeltaT, locMaxDOCA, locMaxDeltaZ, locMaxDeltaT;
	DKinFitType locKinFitType = Get_Reaction()->Get_KinFitType();
	const DKinFitResults* locKinFitResults = locParticleCombo->Get_KinFitResults();

	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);

	for(size_t loc_i = 0; loc_i < locParticleCombo->Get_NumParticleComboSteps(); ++loc_i)
	{
		const DParticleComboStep* locParticleComboStep = locParticleCombo->Get_ParticleComboStep(loc_i);
		if(Get_AnalysisUtilities()->Find_SimilarCombos_FinalCharged(pair<const DParticleComboStep*, size_t>(locParticleComboStep, loc_i), locPreviousParticleCombos))
			continue; //duplicate, already histogrammed

		locParticleComboStep->Get_DetectedFinalChargedParticles_Measured(locParticles);
		if(locParticles.empty())
			continue;

		//Grab/Find common vertex & time
		const DKinematicData* locBeamParticle = (locParticleComboStep->Get_InitialParticleID() == Gamma) ? locParticleComboStep->Get_InitialParticle() : NULL;
		if((locKinFitResults != NULL) && ((locKinFitType == d_VertexFit) || (locKinFitType == d_SpacetimeFit) || (locKinFitType == d_P4AndVertexFit) || (locKinFitType == d_P4AndSpacetimeFit)))
		{
			//locKinFitResults NULL if failed kinfit
			//vertex kinfit
			deque<const DKinematicData*> locParticles_KinFit;
			locParticleComboStep->Get_DetectedFinalChargedParticles(locParticles_KinFit);
			locVertex = locParticles_KinFit[0]->position();
			if((locKinFitType == d_SpacetimeFit) || (locKinFitType == d_P4AndSpacetimeFit))
				locVertexTime = locParticles_KinFit[0]->time();
			else //do crude time determination
				locVertexTime = Get_AnalysisUtilities()->Calc_CrudeTime(locParticles_KinFit, locVertex);
		}
		else //do crude vertex & time determination
		{
			locVertex = Get_AnalysisUtilities()->Calc_CrudeVertex(locParticles);
			locVertexTime = Get_AnalysisUtilities()->Calc_CrudeTime(locParticles, locVertex);
		}

		for(size_t loc_j = 0; loc_j < locParticles.size(); ++loc_j)
		{
			locPID = locParticles[loc_j]->PID();

			//find max's
			locMaxDOCA = -1.0;
			locMaxDeltaZ = -1.0;
			locMaxDeltaT = -1.0;
			for(size_t loc_k = loc_j + 1; loc_k < locParticles.size(); ++loc_k)
			{
				locDeltaZ = fabs(locParticles[loc_j]->position().Z() - locParticles[loc_k]->position().Z());
				if(locDeltaZ > locMaxDeltaZ)
					locMaxDeltaZ = locDeltaZ;

				locDeltaT = fabs(locParticles[loc_j]->time() - locParticles[loc_k]->time());
				if(locDeltaT > locMaxDeltaT)
					locMaxDeltaT = locDeltaT;

				locDOCA = Get_AnalysisUtilities()->Calc_DOCA(locParticles[loc_j], locParticles[loc_k]);
				if(locDOCA > locMaxDOCA)
					locMaxDOCA = locDOCA;
			}

			//delta-t vs p
			deque<pair<const DKinematicData*, size_t> > locParticlePairs;
			size_t locHigherMassParticleIndex, locLowerMassParticleIndex;
			//minimize number of locks by keeping track of the results and saving them at the end
			deque<pair<Particle_t, Particle_t> > locPIDPairs;
			deque<double> locPs;
			deque<double> locDeltaTs;
			for(size_t loc_k = loc_j + 1; loc_k < locParticles.size(); ++loc_k)
			{
				locParticlePairs.clear();
				locParticlePairs.push_back(pair<const DKinematicData*, size_t>(locParticles[loc_j], loc_i));
				locParticlePairs.push_back(pair<const DKinematicData*, size_t>(locParticles[loc_k], loc_i));
				if(Get_AnalysisUtilities()->Find_SimilarCombos(locParticlePairs, locPreviousParticleCombos))
					continue; //already histed!

				if(locParticles[loc_k]->mass() > locParticles[loc_j]->mass())
				{
					locHigherMassParticleIndex = loc_k;
					locLowerMassParticleIndex = loc_j;
				}
				else
				{
					locHigherMassParticleIndex = loc_j;
					locLowerMassParticleIndex = loc_k;
				}

				locDeltaTs.push_back(locParticles[locHigherMassParticleIndex]->time() - locParticles[locLowerMassParticleIndex]->time());
				locPs.push_back(locParticles[locHigherMassParticleIndex]->momentum().Mag());
				locPIDPairs.push_back(pair<Particle_t, Particle_t>(locParticles[locHigherMassParticleIndex]->PID(), locParticles[locLowerMassParticleIndex]->PID()));
			}

			double locBeamDeltaT = (locBeamParticle != NULL) ? locParticles[loc_j]->time() - locBeamParticle->time() : numeric_limits<double>::quiet_NaN();
			locDOCA = Get_AnalysisUtilities()->Calc_DOCAToVertex(locParticles[loc_j], locVertex);

			//HISTOGRAM //do all at once to reduce #locks & amount of time within the lock
			Get_Application()->RootWriteLock();
			{
				//comparison to common vertex/time
				dHistDeque_TrackZToCommon[loc_i][locPID]->Fill(locParticles[loc_j]->position().Z() - locVertex.Z());
				dHistDeque_TrackTToCommon[loc_i][locPID]->Fill(locParticles[loc_j]->time() - locVertexTime);
				dHistDeque_TrackDOCAToCommon[loc_i][locPID]->Fill(locDOCA);
				//hist max's
				if(locMaxDeltaZ > 0.0) //else none found (e.g. only 1 detected charged track)
				{
					dHistDeque_MaxTrackDeltaZ[loc_i]->Fill(locMaxDeltaZ);
					dHistDeque_MaxTrackDeltaT[loc_i]->Fill(locMaxDeltaT);
					dHistDeque_MaxTrackDOCA[loc_i]->Fill(locMaxDOCA);
				}
				//delta-t's
				if(locBeamParticle != NULL)
					dHistMap_BeamTrackDeltaTVsP[locPID]->Fill(locParticles[loc_j]->momentum().Mag(), locBeamDeltaT);
				for(size_t loc_k = 0; loc_k < locPIDPairs.size(); ++loc_k)
				{
					if(dHistDeque_TrackDeltaTVsP[loc_i].find(locPIDPairs[loc_k]) == dHistDeque_TrackDeltaTVsP[loc_i].end())
					{
						//pair not found: equal masses and order switched somehow //e.g. mass set differently between REST and reconstruction
						pair<Particle_t, Particle_t> locTempPIDPair(locPIDPairs[loc_k]);
						locPIDPairs[loc_k].first = locTempPIDPair.second;
						locPIDPairs[loc_k].second = locTempPIDPair.first;
					}
					dHistDeque_TrackDeltaTVsP[loc_i][locPIDPairs[loc_k]]->Fill(locPs[loc_k], locDeltaTs[loc_k]);
				}
			}
			Get_Application()->RootUnLock();
		} //end of particle loop
	} //end of step loop
	return true;
}

void DHistogramAction_ParticleComboKinematics::Initialize(JEventLoop* locEventLoop)
{
	if(Get_UseKinFitResultsFlag() && (Get_Reaction()->Get_KinFitType() == d_NoFit))
	{
		cout << "WARNING: REQUESTED HISTOGRAM OF KINEMAITIC FIT RESULTS WHEN NO KINEMATIC FIT!!!" << endl;
		return; //no fit performed, but kinfit data requested!!
	}

	vector<const DParticleID*> locParticleIDs;
	locEventLoop->Get(locParticleIDs);
	dParticleID = locParticleIDs[0];

	string locHistName, locHistTitle, locStepName, locStepROOTName, locParticleName, locParticleROOTName;
	Particle_t locPID;

	size_t locNumSteps = Get_Reaction()->Get_NumReactionSteps();
	dHistDeque_PVsTheta.resize(locNumSteps);
	dHistDeque_PhiVsTheta.resize(locNumSteps);
	dHistDeque_BetaVsP.resize(locNumSteps);
	dHistDeque_DeltaBetaVsP.resize(locNumSteps);
	dHistDeque_P.resize(locNumSteps);
	dHistDeque_Theta.resize(locNumSteps);
	dHistDeque_Phi.resize(locNumSteps);
	dHistDeque_VertexZ.resize(locNumSteps);
	dHistDeque_VertexT.resize(locNumSteps);
	dHistDeque_VertexYVsX.resize(locNumSteps);

	deque<deque<Particle_t> > locDetectedFinalPIDs;
	Get_Reaction()->Get_DetectedFinalPIDs(locDetectedFinalPIDs);

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();

	//beam particle
	locPID = Get_Reaction()->Get_ReactionStep(0)->Get_InitialParticleID();
	bool locBeamParticleUsed = (locPID == Gamma);
	if(locBeamParticleUsed)
	{
		locParticleName = string("Beam_") + ParticleType(locPID);
		CreateAndChangeTo_Directory(locParticleName, locParticleName);

		// Momentum
		locHistName = "Momentum";
		locHistTitle = string("Beam ") + locParticleROOTName + string(";p (GeV/c)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dBeamParticleHist_P = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dBeamParticleHist_P = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPBins, dMinP, dMaxP);

		// Theta
		locHistName = "Theta";
		locHistTitle = string("Beam ") + locParticleROOTName + string(";#theta#circ");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dBeamParticleHist_Theta = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dBeamParticleHist_Theta = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumThetaBins, dMinTheta, dMaxTheta);

		// Phi
		locHistName = "Phi";
		locHistTitle = string("Beam ") + locParticleROOTName + string(";#phi#circ");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dBeamParticleHist_Phi = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dBeamParticleHist_Phi = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPhiBins, dMinPhi, dMaxPhi);

		// P Vs Theta
		locHistName = "PVsTheta";
		locHistTitle = string("Beam ") + locParticleROOTName + string(";#theta#circ;p (GeV/c)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dBeamParticleHist_PVsTheta = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dBeamParticleHist_PVsTheta = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

		// Phi Vs Theta
		locHistName = "PhiVsTheta";
		locHistTitle = string("Beam ") + locParticleROOTName + string(";#theta#circ;#phi#circ");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dBeamParticleHist_PhiVsTheta = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dBeamParticleHist_PhiVsTheta = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPhiBins, dMinPhi, dMaxPhi);

		// Vertex-Z
		locHistName = "VertexZ";
		locHistTitle = string("Beam ") + locParticleROOTName + string(";Vertex-Z (cm)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dBeamParticleHist_VertexZ = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dBeamParticleHist_VertexZ = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumVertexZBins, dMinVertexZ, dMaxVertexZ);

		// Vertex-Y Vs Vertex-X
		locHistName = "VertexYVsX";
		locHistTitle = string("Beam ") + locParticleROOTName + string(";Vertex-X (cm);Vertex-Y (cm)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dBeamParticleHist_VertexYVsX = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dBeamParticleHist_VertexYVsX = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNumVertexXYBins, dMinVertexXY, dMaxVertexXY, dNumVertexXYBins, dMinVertexXY, dMaxVertexXY);

		// Vertex-T
		locHistName = "VertexT";
		locHistTitle = string("Beam ") + locParticleROOTName + string(";Vertex-T (ns)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dBeamParticleHist_VertexT = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dBeamParticleHist_VertexT = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumTBins, dMinT, dMaxT);

		gDirectory->cd("..");
	}

	//CREATE THE HISTOGRAMS
	deque<Particle_t> locPIDs;
	for(size_t loc_i = 0; loc_i < locNumSteps; ++loc_i)
	{
		const DReactionStep* locReactionStep = Get_Reaction()->Get_ReactionStep(loc_i);
		locStepName = locReactionStep->Get_StepName();
		locStepROOTName = locReactionStep->Get_StepROOTName();

		Particle_t locInitialPID = locReactionStep->Get_InitialParticleID();

		//get PIDs
		if(!Get_UseKinFitResultsFlag()) //measured, ignore missing & decaying particles (ignore target anyway)
			locPIDs = locDetectedFinalPIDs[loc_i];
		else //kinematic fit: decaying & missing particles are reconstructed
		{
			locReactionStep->Get_FinalParticleIDs(locPIDs);
			if((!locBeamParticleUsed) || (loc_i != 0)) //add decaying parent particle //skip if on beam particle!
				locPIDs.insert(locPIDs.begin(), locInitialPID);
		}

		if(locPIDs.empty())
			continue;

		CreateAndChangeTo_Directory(locStepName, locStepName);
		for(size_t loc_j = 0; loc_j < locPIDs.size(); ++loc_j)
		{
			locPID = locPIDs[loc_j];
			locParticleName = ParticleType(locPID);
			locParticleROOTName = ParticleName_ROOT(locPID);
			if(dHistDeque_P[loc_i].find(locPID) != dHistDeque_P[loc_i].end())
				continue; //pid already done

			CreateAndChangeTo_Directory(locParticleName, locParticleName);

			// Momentum
			locHistName = "Momentum";
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_P[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_P[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPBins, dMinP, dMaxP);

			// Theta
			locHistName = "Theta";
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";#theta#circ");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_Theta[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_Theta[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumThetaBins, dMinTheta, dMaxTheta);

			// Phi
			locHistName = "Phi";
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";#phi#circ");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_Phi[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_Phi[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPhiBins, dMinPhi, dMaxPhi);

			// P Vs Theta
			locHistName = "PVsTheta";
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";#theta#circ;p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_PVsTheta[loc_i][locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_PVsTheta[loc_i][locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

			// Phi Vs Theta
			locHistName = "PhiVsTheta";
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";#theta#circ;#phi#circ");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_PhiVsTheta[loc_i][locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_PhiVsTheta[loc_i][locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPhiBins, dMinPhi, dMaxPhi);

			//beta vs p
			locHistName = "BetaVsP";
			locHistTitle = locParticleROOTName + string(";p (GeV/c);#beta");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_BetaVsP[loc_i][locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_BetaVsP[loc_i][locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumBetaBins, dMinBeta, dMaxBeta);

			//delta-beta vs p
			locHistName = "DeltaBetaVsP";
			locHistTitle = locParticleROOTName + string(";p (GeV/c);#Delta#beta");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_DeltaBetaVsP[loc_i][locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_DeltaBetaVsP[loc_i][locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaBetaBins, dMinDeltaBeta, dMaxDeltaBeta);

			// Vertex-Z
			locHistName = "VertexZ";
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";Vertex-Z (cm)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_VertexZ[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_VertexZ[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumVertexZBins, dMinVertexZ, dMaxVertexZ);

			// Vertex-Y Vs Vertex-X
			locHistName = "VertexYVsX";
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";Vertex-X (cm);Vertex-Y (cm)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_VertexYVsX[loc_i][locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_VertexYVsX[loc_i][locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNumVertexXYBins, dMinVertexXY, dMaxVertexXY, dNumVertexXYBins, dMinVertexXY, dMaxVertexXY);

			// Vertex-T
			locHistName = "VertexT";
			locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(";Vertex-T (ns)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_VertexT[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_VertexT[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumTBins, dMinT, dMaxT);

			gDirectory->cd("..");
		} //end of particle loop
		gDirectory->cd("..");
	} //end of step loop
	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_ParticleComboKinematics::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	if(Get_UseKinFitResultsFlag() && (Get_Reaction()->Get_KinFitType() == d_NoFit))
	{
		cout << "WARNING: REQUESTED HISTOGRAM OF KINEMAITIC FIT RESULTS WHEN NO KINEMATIC FIT!!! Skipping histogram." << endl;
		return true; //no fit performed, but kinfit data requested!!
	}

	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);

	const DKinematicData* locKinematicData;
	for(size_t loc_i = 0; loc_i < locParticleCombo->Get_NumParticleComboSteps(); ++loc_i)
	{
		const DParticleComboStep* locParticleComboStep = locParticleCombo->Get_ParticleComboStep(loc_i);

		//initial particle
		locKinematicData = locParticleComboStep->Get_InitialParticle();
		if(locKinematicData != NULL)
		{
			if(locParticleComboStep->Get_InitialParticleID() == Gamma)
			{
				if(Get_UseKinFitResultsFlag()) //kinfit, can be no duplicate
					Fill_BeamHists(locKinematicData);
				else if(!Get_AnalysisUtilities()->Find_SimilarCombos(pair<const DKinematicData*, size_t>(locKinematicData, loc_i), locPreviousParticleCombos)) //measured, check for dupe
					Fill_BeamHists(locKinematicData); //else duplicate
			}
			else if(Get_UseKinFitResultsFlag()) //decaying particle, but kinfit so can hist
				Fill_Hists(locEventLoop, locKinematicData, loc_i);
		}

		//final particles
		for(size_t loc_j = 0; loc_j < locParticleComboStep->Get_NumFinalParticles(); ++loc_j)
		{
			if(Get_UseKinFitResultsFlag())
				locKinematicData = locParticleComboStep->Get_FinalParticle(loc_j);
			else
			{
				locKinematicData = locParticleComboStep->Get_FinalParticle_Measured(loc_j);
				if(locKinematicData == NULL)
					continue; //e.g. a decaying or missing particle whose params aren't set yet
				if(Get_AnalysisUtilities()->Find_SimilarCombos(pair<const DKinematicData*, size_t>(locKinematicData, loc_i), locPreviousParticleCombos))
					continue; //duplicate!
			}
			if(locKinematicData == NULL)
				continue;
			Fill_Hists(locEventLoop, locKinematicData, loc_i);
		} //end of particle loop
	} //end of step loop
	return true;
}

void DHistogramAction_ParticleComboKinematics::Fill_Hists(JEventLoop* locEventLoop, const DKinematicData* locKinematicData, size_t locStepIndex)
{
	Particle_t locPID = locKinematicData->PID();
	DVector3 locMomentum = locKinematicData->momentum();
	double locPhi = locMomentum.Phi()*180.0/TMath::Pi();
	double locTheta = locMomentum.Theta()*180.0/TMath::Pi();
	double locP = locMomentum.Mag();

	vector<const DEventRFBunch*> locEventRFBunches;
	locEventLoop->Get(locEventRFBunches);
	const DEventRFBunch* locEventRFBunch = (!locEventRFBunches.empty()) ? locEventRFBunches[0] : NULL;

	double locBeta_Timing, locDeltaBeta;
	if(ParticleCharge(locPID) == 0)
	{
		const DNeutralParticleHypothesis* locNeutralParticleHypothesis = static_cast<const DNeutralParticleHypothesis*>(locKinematicData);
		double locStartTime = locEventRFBunch->dMatchedToTracksFlag ? locEventRFBunch->dTime : numeric_limits<double>::quiet_NaN();
		locBeta_Timing = locNeutralParticleHypothesis->pathLength()/(SPEED_OF_LIGHT*(locNeutralParticleHypothesis->t1() - locStartTime));
		locDeltaBeta = locNeutralParticleHypothesis->lorentzMomentum().Beta() - locBeta_Timing;
	}
	else
	{
		const DChargedTrackHypothesis* locChargedTrackHypothesis = static_cast<const DChargedTrackHypothesis*>(locKinematicData);
		double locStartTime, locStartTimeVariance;
		bool locUsedRFTimeFlag;
		bool locFoundStartTimeFlag = dParticleID->Calc_TrackStartTime(locChargedTrackHypothesis, locEventRFBunch, locStartTime, locStartTimeVariance, locUsedRFTimeFlag);
		locBeta_Timing = locFoundStartTimeFlag ? locChargedTrackHypothesis->pathLength()/(SPEED_OF_LIGHT*(locChargedTrackHypothesis->t1() - locStartTime)) : numeric_limits<double>::quiet_NaN();
		locDeltaBeta = locChargedTrackHypothesis->lorentzMomentum().Beta() - locBeta_Timing;
	}

	Get_Application()->RootWriteLock();
	dHistDeque_P[locStepIndex][locPID]->Fill(locP);
	dHistDeque_Phi[locStepIndex][locPID]->Fill(locPhi);
	dHistDeque_Theta[locStepIndex][locPID]->Fill(locTheta);
	dHistDeque_PVsTheta[locStepIndex][locPID]->Fill(locTheta, locP);
	dHistDeque_PhiVsTheta[locStepIndex][locPID]->Fill(locTheta, locPhi);
	dHistDeque_BetaVsP[locStepIndex][locPID]->Fill(locP, locBeta_Timing);
	dHistDeque_DeltaBetaVsP[locStepIndex][locPID]->Fill(locP, locDeltaBeta);
	dHistDeque_VertexZ[locStepIndex][locPID]->Fill(locKinematicData->position().Z());
	dHistDeque_VertexYVsX[locStepIndex][locPID]->Fill(locKinematicData->position().X(), locKinematicData->position().Y());
	dHistDeque_VertexT[locStepIndex][locPID]->Fill(locKinematicData->time());
	Get_Application()->RootUnLock();
}

void DHistogramAction_ParticleComboKinematics::Fill_BeamHists(const DKinematicData* locKinematicData)
{
	DVector3 locMomentum = locKinematicData->momentum();
	double locPhi = locMomentum.Phi()*180.0/TMath::Pi();
	double locTheta = locMomentum.Theta()*180.0/TMath::Pi();
	double locP = locMomentum.Mag();

	Get_Application()->RootWriteLock();
	dBeamParticleHist_P->Fill(locP);
	dBeamParticleHist_Phi->Fill(locPhi);
	dBeamParticleHist_Theta->Fill(locTheta);
	dBeamParticleHist_PVsTheta->Fill(locTheta, locP);
	dBeamParticleHist_PhiVsTheta->Fill(locTheta, locPhi);
	dBeamParticleHist_VertexZ->Fill(locKinematicData->position().Z());
	dBeamParticleHist_VertexYVsX->Fill(locKinematicData->position().X(), locKinematicData->position().Y());
	dBeamParticleHist_VertexT->Fill(locKinematicData->time());
	Get_Application()->RootUnLock();
}

void DHistogramAction_ThrownParticleKinematics::Initialize(JEventLoop* locEventLoop)
{
	vector<const DMCThrown*> locMCThrowns;
	locEventLoop->Get(locMCThrowns);
	if(locMCThrowns.empty())
		return; //e.g. non-simulated event

	string locHistName, locHistTitle, locParticleName, locParticleROOTName;
	Particle_t locPID;

	dMCThrownMatchingFactory = static_cast<DMCThrownMatching_factory*>(locEventLoop->GetFactory("DMCThrownMatching"));

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();

	// Beam Particle
	locPID = Gamma;
	locParticleName = string("Beam_") + ParticleType(locPID);
	locParticleROOTName = ParticleName_ROOT(locPID);
	CreateAndChangeTo_Directory(locParticleName, locParticleName);
	locHistName = "Momentum";
	locHistTitle = string("Thrown Beam ") + locParticleROOTName + string(";p (GeV/c)");
	if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
		dBeamParticle_P = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
	else
		dBeamParticle_P = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPBins, dMinP, dMaxP);
	gDirectory->cd("..");

	for(size_t loc_i = 0; loc_i < dFinalStatePIDs.size(); ++loc_i)
	{
		locPID = dFinalStatePIDs[loc_i];
		locParticleName = ParticleType(locPID);
		locParticleROOTName = ParticleName_ROOT(locPID);
		CreateAndChangeTo_Directory(locParticleName, locParticleName);

		// Momentum
		locHistName = "Momentum";
		locHistTitle = string("Thrown ") + locParticleROOTName + string(";p (GeV/c)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_P[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_P[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPBins, dMinP, dMaxP);

		// Theta
		locHistName = "Theta";
		locHistTitle = string("Thrown ") + locParticleROOTName + string(";#theta#circ");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_Theta[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_Theta[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumThetaBins, dMinTheta, dMaxTheta);

		// Phi
		locHistName = "Phi";
		locHistTitle = string("Thrown ") + locParticleROOTName + string(";#phi#circ");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_Phi[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_Phi[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPhiBins, dMinPhi, dMaxPhi);

		// P Vs Theta
		locHistName = "PVsTheta";
		locHistTitle = string("Thrown ") + locParticleROOTName + string(";#theta#circ;p (GeV/c)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_PVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_PVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

		// Phi Vs Theta
		locHistName = "PhiVsTheta";
		locHistTitle = string("Thrown ") + locParticleROOTName + string(";#theta#circ;#phi#circ");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_PhiVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_PhiVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPhiBins, dMinPhi, dMaxPhi);

		// Vertex-Z
		locHistName = "VertexZ";
		locHistTitle = string("Thrown ") + locParticleROOTName + string(";Vertex-Z (cm)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_VertexZ[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_VertexZ[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumVertexZBins, dMinVertexZ, dMaxVertexZ);

		// Vertex-Y Vs Vertex-X
		locHistName = "VertexYVsX";
		locHistTitle = string("Thrown ") + locParticleROOTName + string(";Vertex-X (cm);Vertex-Y (cm)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_VertexYVsX[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_VertexYVsX[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNumVertexXYBins, dMinVertexXY, dMaxVertexXY, dNumVertexXYBins, dMinVertexXY, dMaxVertexXY);

		// Vertex-T
		locHistName = "VertexT";
		locHistTitle = string("Thrown ") + locParticleROOTName + string(";Vertex-T (ns)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_VertexT[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_VertexT[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumTBins, dMinT, dMaxT);

		gDirectory->cd("..");
	}
	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_ThrownParticleKinematics::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	vector<const DMCThrown*> locMCThrowns;
	locEventLoop->Get(locMCThrowns);
	if(locMCThrowns.empty())
		return true; //e.g. non-simulated event

	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);
	if(!locPreviousParticleCombos.empty())
		return true; //else double-counting!

	Particle_t locPID;
	const DMCThrown* locMCThrown;

	vector<const DBeamPhoton*> locBeamPhotons;
	locEventLoop->Get(locBeamPhotons);
	for(size_t loc_i = 0; loc_i < locBeamPhotons.size(); ++loc_i)
		dBeamParticle_P->Fill(locBeamPhotons[loc_i]->energy());

	for(size_t loc_i = 0; loc_i < locMCThrowns.size(); ++loc_i)
	{
		locMCThrown = locMCThrowns[loc_i];
		if(!dMCThrownMatchingFactory->Check_IsValidMCComparisonPID(locMCThrowns, locMCThrown))
			continue; //e.g. a muon from pion decay
		locPID = (Particle_t)locMCThrown->type;
		if(dHistMap_P.find(locPID) == dHistMap_P.end())
			continue; //not interested in histogramming

		DVector3 locMomentum = locMCThrown->momentum();
		double locPhi = locMomentum.Phi()*180.0/TMath::Pi();
		double locTheta = locMomentum.Theta()*180.0/TMath::Pi();
		double locP = locMomentum.Mag();
		Get_Application()->RootWriteLock();
		dHistMap_P[locPID]->Fill(locP);
		dHistMap_Phi[locPID]->Fill(locPhi);
		dHistMap_Theta[locPID]->Fill(locTheta);
		dHistMap_PVsTheta[locPID]->Fill(locTheta, locP);
		dHistMap_PhiVsTheta[locPID]->Fill(locTheta, locPhi);
		dHistMap_VertexZ[locPID]->Fill(locMCThrown->position().Z());
		dHistMap_VertexYVsX[locPID]->Fill(locMCThrown->position().X(), locMCThrown->position().Y());
		dHistMap_VertexT[locPID]->Fill(locMCThrown->time());
		Get_Application()->RootUnLock();
	}
	return true;
}

void DHistogramAction_DetectedParticleKinematics::Initialize(JEventLoop* locEventLoop)
{
	string locHistName, locHistTitle, locParticleName, locParticleROOTName;
	Particle_t locPID;

	vector<const DParticleID*> locParticleIDs;
	locEventLoop->Get(locParticleIDs);
	dParticleID = locParticleIDs[0];

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();

	// Beam Particle
	locPID = Gamma;
	locParticleName = string("Beam_") + ParticleType(locPID);
	locParticleROOTName = ParticleName_ROOT(locPID);
	CreateAndChangeTo_Directory(locParticleName, locParticleName);
	locHistName = "Momentum";
	locHistTitle = string("Beam ") + locParticleROOTName + string(";p (GeV/c)");
	if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
		dBeamParticle_P = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
	else
		dBeamParticle_P = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPBins, dMinP, dMaxP);
	gDirectory->cd("..");

	for(size_t loc_i = 0; loc_i < dFinalStatePIDs.size(); ++loc_i)
	{
		locPID = dFinalStatePIDs[loc_i];
		locParticleName = ParticleType(locPID);
		locParticleROOTName = ParticleName_ROOT(locPID);
		CreateAndChangeTo_Directory(locParticleName, locParticleName);

		// Momentum
		locHistName = "Momentum";
		locHistTitle = locParticleROOTName + string(";p (GeV/c)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_P[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_P[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPBins, dMinP, dMaxP);

		// Theta
		locHistName = "Theta";
		locHistTitle = locParticleROOTName + string(";#theta#circ");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_Theta[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_Theta[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumThetaBins, dMinTheta, dMaxTheta);

		// Phi
		locHistName = "Phi";
		locHistTitle = locParticleROOTName + string(";#phi#circ");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_Phi[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_Phi[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPhiBins, dMinPhi, dMaxPhi);

		// P Vs Theta
		locHistName = "PVsTheta";
		locHistTitle = locParticleROOTName + string(";#theta#circ;p (GeV/c)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_PVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_PVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

		// Phi Vs Theta
		locHistName = "PhiVsTheta";
		locHistTitle = locParticleROOTName + string(";#theta#circ;#phi#circ");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_PhiVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_PhiVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPhiBins, dMinPhi, dMaxPhi);

		//beta vs p
		locHistName = "BetaVsP";
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#beta");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_BetaVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_BetaVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumBetaBins, dMinBeta, dMaxBeta);

		//delta-beta vs p
		locHistName = "DeltaBetaVsP";
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#Delta#beta");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaBetaVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaBetaVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaBetaBins, dMinDeltaBeta, dMaxDeltaBeta);

		// Vertex-Z
		locHistName = "VertexZ";
		locHistTitle = locParticleROOTName + string(";Vertex-Z (cm)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_VertexZ[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_VertexZ[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumVertexZBins, dMinVertexZ, dMaxVertexZ);

		if(ParticleCharge(locPID) != 0)
		{
			// Tracking FOM Vs Vertex-Z
			locHistName = "TrackingFOMVsVertexZ";
			locHistTitle = locParticleROOTName + string(";Vertex-Z (cm); Time-Based Track Reconstruction FOM");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistMap_TrackingFOMVsVertexZ[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistMap_TrackingFOMVsVertexZ[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DVertexZBins, dMinVertexZ, dMaxVertexZ, dNumTrackFOMBins, 0.0, 1.0);
		}

		// Vertex-Z Vs Theta
		locHistName = "VertexZVsTheta";
		locHistTitle = locParticleROOTName + string(";#theta#circ;Vertex-Z (cm)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_VertexZVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_VertexZVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DVertexZBins, dMinVertexZ, dMaxVertexZ);

		// Vertex-Y Vs Vertex-X
		locHistName = "VertexYVsX";
		locHistTitle = locParticleROOTName + string(";Vertex-X (cm);Vertex-Y (cm)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_VertexYVsX[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_VertexYVsX[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNumVertexXYBins, dMinVertexXY, dMaxVertexXY, dNumVertexXYBins, dMinVertexXY, dMaxVertexXY);

		// Vertex-T
		locHistName = "VertexT";
		locHistTitle = locParticleROOTName + string(";Vertex-T (ns)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_VertexT[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_VertexT[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumTBins, dMinT, dMaxT);

		gDirectory->cd("..");
	}
	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}


bool DHistogramAction_DetectedParticleKinematics::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);
	if(!locPreviousParticleCombos.empty())
		return true; //else double-counting!

	Particle_t locPID;

	vector<const DEventRFBunch*> locEventRFBunches;
	locEventLoop->Get(locEventRFBunches);
	const DEventRFBunch* locEventRFBunch = (!locEventRFBunches.empty()) ? locEventRFBunches[0] : NULL;
	double locBeta_Timing, locDeltaBeta;

	vector<const DMCThrownMatching*> locMCThrownMatchingVector;
	locEventLoop->Get(locMCThrownMatchingVector);
	const DMCThrownMatching* locMCThrownMatching = locMCThrownMatchingVector.empty() ? NULL : locMCThrownMatchingVector[0];

	vector<const DBeamPhoton*> locBeamPhotons;
	locEventLoop->Get(locBeamPhotons);
	for(size_t loc_i = 0; loc_i < locBeamPhotons.size(); ++loc_i)
		dBeamParticle_P->Fill(locBeamPhotons[loc_i]->energy());

	vector<const DChargedTrack*> locChargedTracks;
	locEventLoop->Get(locChargedTracks);
	const DChargedTrackHypothesis* locChargedTrackHypothesis = NULL;

	for(size_t loc_i = 0; loc_i < locChargedTracks.size(); ++loc_i)
	{
		if(locMCThrownMatching != NULL)
		{
			const DMCThrown* locMCThrown = locMCThrownMatching->Get_MatchingMCThrown(locChargedTracks[loc_i]);
			if(locMCThrown == NULL)
			{
				locChargedTrackHypothesis = locChargedTracks[loc_i]->Get_BestFOM();
				locPID = locChargedTrackHypothesis->PID();
			}
			else
			{
				locPID = (Particle_t)locMCThrown->type;
				locChargedTrackHypothesis = locChargedTracks[loc_i]->Get_Hypothesis(locPID);
				if(locChargedTrackHypothesis == NULL)
					locChargedTrackHypothesis = locChargedTracks[loc_i]->Get_BestFOM();
			}
		}
		else
		{
			locChargedTrackHypothesis = locChargedTracks[loc_i]->Get_BestFOM();
			locPID = locChargedTrackHypothesis->PID();
		}

		if(dHistMap_P.find(locPID) == dHistMap_P.end())
			continue; //e.g. a decaying particle, or not interested in histogramming

		DVector3 locMomentum = locChargedTrackHypothesis->momentum();
		double locPhi = locMomentum.Phi()*180.0/TMath::Pi();
		double locTheta = locMomentum.Theta()*180.0/TMath::Pi();
		double locP = locMomentum.Mag();

		double locStartTime, locStartTimeVariance;
		bool locUsedRFTimeFlag;
		bool locFoundStartTimeFlag = dParticleID->Calc_TrackStartTime(locChargedTrackHypothesis, locEventRFBunch, locStartTime, locStartTimeVariance, locUsedRFTimeFlag);
		locBeta_Timing = locFoundStartTimeFlag ? locChargedTrackHypothesis->pathLength()/(SPEED_OF_LIGHT*(locChargedTrackHypothesis->t1() - locStartTime)) : numeric_limits<double>::quiet_NaN();
		locDeltaBeta = locChargedTrackHypothesis->lorentzMomentum().Beta() - locBeta_Timing;
		double locTrackingFOM = TMath::Prob(locChargedTrackHypothesis->dChiSq_Track, locChargedTrackHypothesis->dNDF_Track);

		Get_Application()->RootWriteLock();
		dHistMap_P[locPID]->Fill(locP);
		dHistMap_Phi[locPID]->Fill(locPhi);
		dHistMap_Theta[locPID]->Fill(locTheta);
		dHistMap_PVsTheta[locPID]->Fill(locTheta, locP);
		dHistMap_PhiVsTheta[locPID]->Fill(locTheta, locPhi);
		dHistMap_BetaVsP[locPID]->Fill(locP, locBeta_Timing);
		dHistMap_DeltaBetaVsP[locPID]->Fill(locP, locDeltaBeta);
		dHistMap_VertexZ[locPID]->Fill(locChargedTrackHypothesis->position().Z());
		dHistMap_TrackingFOMVsVertexZ[locPID]->Fill(locChargedTrackHypothesis->position().Z(), locTrackingFOM);
		dHistMap_VertexZVsTheta[locPID]->Fill(locTheta, locChargedTrackHypothesis->position().Z());
		dHistMap_VertexYVsX[locPID]->Fill(locChargedTrackHypothesis->position().X(), locChargedTrackHypothesis->position().Y());
		dHistMap_VertexT[locPID]->Fill(locChargedTrackHypothesis->time());
		Get_Application()->RootUnLock();
	}

	vector<const DNeutralParticle*> locNeutralParticles;
	locEventLoop->Get(locNeutralParticles);
	const DNeutralParticleHypothesis* locNeutralParticleHypothesis;

	for(size_t loc_i = 0; loc_i < locNeutralParticles.size(); ++loc_i)
	{
		if(locMCThrownMatching != NULL)
		{
			const DMCThrown* locMCThrown = locMCThrownMatching->Get_MatchingMCThrown(locNeutralParticles[loc_i]);
			if(locMCThrown == NULL)
			{
				locNeutralParticleHypothesis = locNeutralParticles[loc_i]->Get_BestFOM();
				locPID = locNeutralParticleHypothesis->PID();
			}
			else
			{
				locPID = (Particle_t)locMCThrown->type;
				locNeutralParticleHypothesis = locNeutralParticles[loc_i]->Get_Hypothesis(locPID);
				if(locNeutralParticleHypothesis == NULL)
					locNeutralParticleHypothesis = locNeutralParticles[loc_i]->Get_BestFOM();
			}
		}
		else
		{
			locNeutralParticleHypothesis = locNeutralParticles[loc_i]->Get_BestFOM();
			locPID = locNeutralParticleHypothesis->PID();
		}

		if(dHistMap_P.find(locPID) == dHistMap_P.end())
			continue; //e.g. a decaying particle, or not interested in histogramming

		DVector3 locMomentum = locNeutralParticleHypothesis->momentum();
		double locPhi = locMomentum.Phi()*180.0/TMath::Pi();
		double locTheta = locMomentum.Theta()*180.0/TMath::Pi();
		double locP = locMomentum.Mag();

		double locStartTime = locEventRFBunch->dMatchedToTracksFlag ? locEventRFBunch->dTime : numeric_limits<double>::quiet_NaN();
		locBeta_Timing = locNeutralParticleHypothesis->pathLength()/(SPEED_OF_LIGHT*(locNeutralParticleHypothesis->t1() - locStartTime));
		locDeltaBeta = locNeutralParticleHypothesis->lorentzMomentum().Beta() - locBeta_Timing;

		Get_Application()->RootWriteLock();
		dHistMap_P[locPID]->Fill(locP);
		dHistMap_Phi[locPID]->Fill(locPhi);
		dHistMap_Theta[locPID]->Fill(locTheta);
		dHistMap_PVsTheta[locPID]->Fill(locTheta, locP);
		dHistMap_PhiVsTheta[locPID]->Fill(locTheta, locPhi);
		dHistMap_BetaVsP[locPID]->Fill(locP, locBeta_Timing);
		dHistMap_DeltaBetaVsP[locPID]->Fill(locP, locDeltaBeta);
		dHistMap_VertexZ[locPID]->Fill(locNeutralParticleHypothesis->position().Z());
		dHistMap_VertexZVsTheta[locPID]->Fill(locTheta, locNeutralParticleHypothesis->position().Z());
		dHistMap_VertexYVsX[locPID]->Fill(locNeutralParticleHypothesis->position().X(), locNeutralParticleHypothesis->position().Y());
		dHistMap_VertexT[locPID]->Fill(locNeutralParticleHypothesis->time());
		Get_Application()->RootUnLock();
	}
	return true;
}

void DHistogramAction_GenReconTrackComparison::Initialize(JEventLoop* locEventLoop)
{
	vector<const DMCThrown*> locMCThrowns;
	locEventLoop->Get(locMCThrowns);
	if(locMCThrowns.empty())
		return; //e.g. non-simulated event

	string locHistName, locHistTitle, locParticleName, locParticleROOTName;
	Particle_t locPID;

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();

	locHistName = "DeltaT_RFBeamBunch";
	if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
		dRFBeamBunchDeltaT_Hist = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
	else
		dRFBeamBunchDeltaT_Hist = new TH1D(locHistName.c_str(), ";RF #Deltat (Reconstructed - Thrown)", dNumRFDeltaTBins, dMinRFDeltaT, dMaxRFDeltaT);

	for(size_t loc_i = 0; loc_i < dFinalStatePIDs.size(); ++loc_i)
	{
		locPID = dFinalStatePIDs[loc_i];
		locParticleName = ParticleType(locPID);
		locParticleROOTName = ParticleName_ROOT(locPID);
		CreateAndChangeTo_Directory(locParticleName, locParticleName);

		// DeltaP/P
		locHistName = string("DeltaPOverP");
		locHistTitle = locParticleROOTName + string(";#Deltap/p (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaPOverP[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaPOverP[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaPOverPBins, dMinDeltaPOverP, dMaxDeltaPOverP);

		// DeltaTheta
		locHistName = string("DeltaTheta");
		locHistTitle = locParticleROOTName + string(";#Delta#theta#circ (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaTheta[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaTheta[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaThetaBins, dMinDeltaTheta, dMaxDeltaTheta);

		// DeltaPhi
		locHistName = string("DeltaPhi");
		locHistTitle = locParticleROOTName + string(";#Delta#phi#circ (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaPhi[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaPhi[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaPhiBins, dMinDeltaPhi, dMaxDeltaPhi);

		// DeltaT
		locHistName = string("DeltaT");
		locHistTitle = locParticleROOTName + string(";#Deltat (ns) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaT[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaT[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaTBins, dMinDeltaT, dMaxDeltaT);

		// DeltaVertexZ
		locHistName = string("DeltaVertexZ");
		locHistTitle = locParticleROOTName + string(";#DeltaVertex-Z (cm) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaVertexZ[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaVertexZ[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaVertexZBins, dMinDeltaVertexZ, dMaxDeltaVertexZ);

		// DeltaP/P Vs P
		locHistName = string("DeltaPOverPVsP");
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#Deltap/p (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaPOverPVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaPOverPVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaPOverPBins, dMinDeltaPOverP, dMaxDeltaPOverP);

		// DeltaP/P Vs Theta
		locHistName = string("DeltaPOverPVsTheta");
		locHistTitle = locParticleROOTName + string(";#theta#circ;#Deltap/p (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaPOverPVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaPOverPVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNumDeltaPOverPBins, dMinDeltaPOverP, dMaxDeltaPOverP);

		// DeltaTheta Vs P
		locHistName = string("DeltaThetaVsP");
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#Delta#theta#circ (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaThetaVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaThetaVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaThetaBins, dMinDeltaTheta, dMaxDeltaTheta);

		// DeltaTheta Vs Theta
		locHistName = string("DeltaThetaVsTheta");
		locHistTitle = locParticleROOTName + string(";#theta#circ;#Delta#theta#circ (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaThetaVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaThetaVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNumDeltaThetaBins, dMinDeltaTheta, dMaxDeltaTheta);

		// DeltaPhi Vs P
		locHistName = string("DeltaPhiVsP");
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#Delta#phi#circ (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaPhiVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaPhiVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaPhiBins, dMinDeltaPhi, dMaxDeltaPhi);

		// DeltaPhi Vs Theta
		locHistName = string("DeltaPhiVsTheta");
		locHistTitle = locParticleROOTName + string(";#theta#circ;#Delta#phi#circ (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaPhiVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaPhiVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNumDeltaPhiBins, dMinDeltaPhi, dMaxDeltaPhi);

		// DeltaT Vs Theta
		locHistName = string("DeltaTVsTheta");
		locHistTitle = locParticleROOTName + string(";#theta#circ;#Deltat (ns) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaTVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaTVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNumDeltaTBins, dMinDeltaT, dMaxDeltaT);

		// DeltaT Vs P
		locHistName = string("DeltaTVsP");
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#Deltat (ns) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaTVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaTVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaTBins, dMinDeltaT, dMaxDeltaT);

		// DeltaVertexZ Vs Theta
		locHistName = string("DeltaVertexZVsTheta");
		locHistTitle = locParticleROOTName + string(";#theta#circ;#DeltaVertex-Z (cm) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaVertexZVsTheta[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaVertexZVsTheta[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNumDeltaVertexZBins, dMinDeltaVertexZ, dMaxDeltaVertexZ);

		// P Vs Theta
		locHistName = "PVsTheta_LargeDeltaT";
		locHistTitle = locParticleROOTName + string(";#theta#circ;p (GeV/c)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_PVsTheta_LargeDeltaT[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_PVsTheta_LargeDeltaT[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

		gDirectory->cd("..");
	}
	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_GenReconTrackComparison::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	vector<const DMCThrown*> locMCThrowns;
	locEventLoop->Get(locMCThrowns);
	if(locMCThrowns.empty())
		return true; //e.g. non-simulated event

	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);
	if(!locPreviousParticleCombos.empty())
		return true; //else double-counting!

	Particle_t locPID;
	double locDeltaPOverP, locDeltaTheta, locDeltaPhi, locDeltaVertexZ;
	double locThrownP, locThrownTheta, locDeltaT;

	const DMCThrown* locMCThrown;
	vector<const DMCThrownMatching*> locMCThrownMatchingVector;
	locEventLoop->Get(locMCThrownMatchingVector);
	if(locMCThrownMatchingVector.empty())
		return true;
	const DMCThrownMatching* locMCThrownMatching = locMCThrownMatchingVector[0];

	//RF time difference
	vector<const DEventRFBunch*> locEventRFBunches;
	locEventLoop->Get(locEventRFBunches);
	const DEventRFBunch* locEventRFBunch = locEventRFBunches[0];
	double locRFTime = locEventRFBunch->dMatchedToTracksFlag ? locEventRFBunch->dTime : numeric_limits<double>::quiet_NaN();
	double locRFDeltaT = locRFTime - 0.0;
	Get_Application()->RootWriteLock();
	dRFBeamBunchDeltaT_Hist->Fill(locRFDeltaT);
	Get_Application()->RootUnLock();

	//charged particles
	const DChargedTrackHypothesis* locChargedTrackHypothesis;
	map<const DMCThrown*, const DChargedTrack*> locThrownToChargedMap;
	locMCThrownMatching->Get_ThrownToChargedMap(locThrownToChargedMap);
	for(map<const DMCThrown*, const DChargedTrack*>::iterator locIterator = locThrownToChargedMap.begin(); locIterator != locThrownToChargedMap.end(); ++locIterator)
	{
		locMCThrown = locIterator->first;
		locChargedTrackHypothesis = locIterator->second->Get_BestFOM();
		locPID = (Particle_t)locMCThrown->type;
		if(dHistMap_DeltaPOverP.find(locPID) == dHistMap_DeltaPOverP.end())
			continue; //e.g. not interested in histogramming

		locThrownP = locMCThrown->momentum().Mag();
		locThrownTheta = locMCThrown->momentum().Theta()*180.0/TMath::Pi();
		locDeltaPOverP = (locChargedTrackHypothesis->momentum().Mag() - locThrownP)/locThrownP;
		locDeltaTheta = locChargedTrackHypothesis->momentum().Theta()*180.0/TMath::Pi() - locThrownTheta;
		locDeltaPhi = locChargedTrackHypothesis->momentum().Phi()*180.0/TMath::Pi() - locMCThrown->momentum().Phi()*180.0/TMath::Pi();
		locDeltaT = locChargedTrackHypothesis->time() - locMCThrown->time(); //time comparison isn't fair if track comes from a detached vertex!!!
		locDeltaVertexZ = locChargedTrackHypothesis->position().Z() - locMCThrown->position().Z();

		vector<const DTrackTimeBased*> locTrackTimeBasedVector;
		locChargedTrackHypothesis->Get(locTrackTimeBasedVector);
		const DTrackTimeBased* locTrackTimeBased = locTrackTimeBasedVector[0];
		Get_Application()->RootWriteLock();
		dHistMap_DeltaPOverP[locPID]->Fill(locDeltaPOverP);
		dHistMap_DeltaTheta[locPID]->Fill(locDeltaTheta);
		dHistMap_DeltaPhi[locPID]->Fill(locDeltaPhi);
		dHistMap_DeltaT[locPID]->Fill(locDeltaT);
		dHistMap_DeltaVertexZ[locPID]->Fill(locDeltaVertexZ);
		dHistMap_DeltaPOverPVsP[locPID]->Fill(locThrownP, locDeltaPOverP);
		dHistMap_DeltaPOverPVsTheta[locPID]->Fill(locThrownTheta, locDeltaPOverP);
		dHistMap_DeltaThetaVsP[locPID]->Fill(locThrownP, locDeltaTheta);
		dHistMap_DeltaThetaVsTheta[locPID]->Fill(locThrownTheta, locDeltaTheta);
		dHistMap_DeltaPhiVsP[locPID]->Fill(locThrownP, locDeltaPhi);
		dHistMap_DeltaPhiVsTheta[locPID]->Fill(locThrownTheta, locDeltaPhi);
		dHistMap_DeltaTVsTheta[locPID]->Fill(locThrownTheta, locDeltaT);
		dHistMap_DeltaTVsP[locPID]->Fill(locThrownP, locDeltaT);
		dHistMap_DeltaVertexZVsTheta[locPID]->Fill(locThrownTheta, locDeltaVertexZ);
		if((locTrackTimeBased->FOM > 0.01) && (locDeltaT >= 1.002))
			dHistMap_PVsTheta_LargeDeltaT[locPID]->Fill(locThrownTheta, locThrownP);
		Get_Application()->RootUnLock();
	}

	//neutral particles
	const DNeutralParticleHypothesis* locNeutralParticleHypothesis;
	map<const DMCThrown*, const DNeutralParticle*> locThrownToNeutralMap;
	locMCThrownMatching->Get_ThrownToNeutralMap(locThrownToNeutralMap);
	for(map<const DMCThrown*, const DNeutralParticle*>::iterator locIterator = locThrownToNeutralMap.begin(); locIterator != locThrownToNeutralMap.end(); ++locIterator)
	{
		locMCThrown = locIterator->first;
		locNeutralParticleHypothesis = locIterator->second->Get_BestFOM();
		locPID = (Particle_t)locMCThrown->type;
		if(dHistMap_DeltaPOverP.find(locPID) == dHistMap_DeltaPOverP.end())
			continue; //e.g. not interested in histogramming

		locThrownP = locMCThrown->momentum().Mag();
		locThrownTheta = locMCThrown->momentum().Theta()*180.0/TMath::Pi();
		locDeltaPOverP = (locNeutralParticleHypothesis->momentum().Mag() - locThrownP)/locThrownP;
		locDeltaTheta = locNeutralParticleHypothesis->momentum().Theta()*180.0/TMath::Pi() - locThrownTheta;
		locDeltaPhi = locNeutralParticleHypothesis->momentum().Phi()*180.0/TMath::Pi() - locMCThrown->momentum().Phi()*180.0/TMath::Pi();
		locDeltaT = locNeutralParticleHypothesis->time() - locMCThrown->time(); //time comparison isn't fair if track comes from a detached vertex!!!
		locDeltaVertexZ = locNeutralParticleHypothesis->position().Z() - locMCThrown->position().Z();

		Get_Application()->RootWriteLock();
		dHistMap_DeltaPOverP[locPID]->Fill(locDeltaPOverP);
		dHistMap_DeltaTheta[locPID]->Fill(locDeltaTheta);
		dHistMap_DeltaPhi[locPID]->Fill(locDeltaPhi);
		dHistMap_DeltaT[locPID]->Fill(locDeltaT);
		dHistMap_DeltaVertexZ[locPID]->Fill(locDeltaVertexZ);
		dHistMap_DeltaPOverPVsP[locPID]->Fill(locThrownP, locDeltaPOverP);
		dHistMap_DeltaPOverPVsTheta[locPID]->Fill(locThrownTheta, locDeltaPOverP);
		dHistMap_DeltaThetaVsP[locPID]->Fill(locThrownP, locDeltaTheta);
		dHistMap_DeltaThetaVsTheta[locPID]->Fill(locThrownTheta, locDeltaTheta);
		dHistMap_DeltaPhiVsP[locPID]->Fill(locThrownP, locDeltaPhi);
		dHistMap_DeltaPhiVsTheta[locPID]->Fill(locThrownTheta, locDeltaPhi);
		dHistMap_DeltaTVsTheta[locPID]->Fill(locThrownTheta, locDeltaT);
		dHistMap_DeltaTVsP[locPID]->Fill(locThrownP, locDeltaT);
		dHistMap_DeltaVertexZVsTheta[locPID]->Fill(locThrownTheta, locDeltaVertexZ);
		if(locDeltaT >= 1.002)
			dHistMap_PVsTheta_LargeDeltaT[locPID]->Fill(locThrownTheta, locThrownP);
		Get_Application()->RootUnLock();
	}
	return true;
}

void DHistogramAction_TOFHitStudy::Initialize(JEventLoop* locEventLoop)
{
	string locHistName, locHistTitle, locParticleName, locParticleROOTName;
	Particle_t locPID;

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();

	for(size_t loc_i = 0; loc_i < dFinalStatePIDs.size(); ++loc_i)
	{
		locPID = dFinalStatePIDs[loc_i];
		locParticleName = ParticleType(locPID);
		locParticleROOTName = ParticleName_ROOT(locPID);
		CreateAndChangeTo_Directory(locParticleName, locParticleName);

		// DeltaT
		locHistName = string("DeltaT_") + locParticleName;
		locHistTitle = locParticleROOTName + string(";#Deltat (ns) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaT[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaT[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaTBins, dMinDeltaT, dMaxDeltaT);

		// DeltaX
		locHistName = string("DeltaX_") + locParticleName;
		locHistTitle = locParticleROOTName + string(";#Deltax (cm) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaX[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaX[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaXBins, dMinDeltaX, dMaxDeltaX);

		// DeltaY
		locHistName = string("DeltaY_") + locParticleName;
		locHistTitle = locParticleROOTName + string(";#Deltay (cm) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaY[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaY[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumDeltaXBins, dMinDeltaX, dMaxDeltaX);

		// dE
		locHistName = string("dE_") + locParticleName;
		locHistTitle = locParticleROOTName + string(";dE (MeV)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_dE[locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_dE[locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumdEBins, dMindE, dMaxdE);

		// DeltaT Vs P
		locHistName = string("DeltaTVsP_") + locParticleName;
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#Deltat (ns) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaTVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaTVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaTBins, dMinDeltaT, dMaxDeltaT);

		// DeltaX Vs P
		locHistName = string("DeltaXVsP_") + locParticleName;
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#Deltax (cm) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaXVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaXVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaXBins, dMinDeltaX, dMaxDeltaX);

		// DeltaY Vs P
		locHistName = string("DeltaYVsP_") + locParticleName;
		locHistTitle = locParticleROOTName + string(";p (GeV/c);#Deltay (cm) (Reconstructed - Thrown)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_DeltaYVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_DeltaYVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumDeltaXBins, dMinDeltaX, dMaxDeltaX);

		// dE Vs P
		locHistName = string("dEVsP_") + locParticleName;
		locHistTitle = locParticleROOTName + string(";p (GeV/c);dE (GeV)");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistMap_dEVsP[locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHistMap_dEVsP[locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNum2DPBins, dMinP, dMaxP, dNumdEBins, dMindE, dMaxdE);

		gDirectory->cd("..");
	}
	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_TOFHitStudy::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);
	if(!locPreviousParticleCombos.empty())
		return true; //else double-counting!

	vector<const DMCThrownMatching*> locMCThrownMatchingVector;
	locEventLoop->Get(locMCThrownMatchingVector);
	if(locMCThrownMatchingVector.empty())
		return true;
	const DMCThrownMatching* locMCThrownMatching = locMCThrownMatchingVector[0];

	vector<const DMCThrown*> locMCThrownVector;
	locEventLoop->Get(locMCThrownVector);

	map<const DTOFTruth*, const DTOFPoint*> locTOFTruthToPointMap;
	locMCThrownMatching->Get_TOFTruthToPointMap(locTOFTruthToPointMap);

	map<const DTOFTruth*, const DTOFPoint*>::iterator locTOFIterator;
	for(locTOFIterator = locTOFTruthToPointMap.begin(); locTOFIterator != locTOFTruthToPointMap.end(); ++locTOFIterator)
	{
		const DTOFTruth* locTOFTruth = locTOFIterator->first;
		const DTOFPoint* locTOFPoint = locTOFIterator->second;
		const DMCThrown* locMCThrown = NULL;
		for(size_t loc_i = 0; loc_i < locMCThrownVector.size(); ++loc_i)
		{
			if(locMCThrownVector[loc_i]->myid != locTOFTruth->track)
				continue;
			locMCThrown = locMCThrownVector[loc_i];
			break;
		}

		Particle_t locPID = (locMCThrown == NULL) ? Unknown : locMCThrown->PID();
		if(dHistMap_DeltaT.find(locPID) == dHistMap_DeltaT.end())
			continue;

		DVector3 locMomentumAtTOF(locTOFTruth->px, locTOFTruth->py, locTOFTruth->pz);
		DVector3 locThrownMomentum = (locMCThrown == NULL) ? locMomentumAtTOF : locMCThrown->momentum();
		double locThrownPMag = locThrownMomentum.Mag();

		//DTOFPoint and DTOFTruth reported at different z's (I think center vs. detector face): propagate truth information to the reconstructed z
		double locDeltaZ = locTOFPoint->pos.Z() - locTOFTruth->z;
		double locDeltaPathLength = locDeltaZ/cos(locMomentumAtTOF.Theta());
		double locPropagatedTrueX = locTOFTruth->x + locDeltaPathLength*sin(locMomentumAtTOF.Theta())*cos(locMomentumAtTOF.Phi());
		double locPropagatedTrueY = locTOFTruth->y + locDeltaPathLength*sin(locMomentumAtTOF.Theta())*sin(locMomentumAtTOF.Phi());
		double locVelocity = SPEED_OF_LIGHT*locMomentumAtTOF.Mag()/locTOFTruth->E;
		double locPropagatedTrueT = locTOFTruth->t + locDeltaPathLength/locVelocity;

		double locDeltaT = locTOFPoint->t - locPropagatedTrueT;
		double locDeltaX = locTOFPoint->pos.X() - locPropagatedTrueX;
		double locDeltaY = locTOFPoint->pos.Y() - locPropagatedTrueY;

		double locdE_MeV = locTOFPoint->dE*1000.0;

		Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
		dHistMap_DeltaT[locPID]->Fill(locDeltaT);
		dHistMap_DeltaX[locPID]->Fill(locDeltaX);
		dHistMap_DeltaY[locPID]->Fill(locDeltaY);
		dHistMap_dE[locPID]->Fill(locdE_MeV);
		dHistMap_DeltaTVsP[locPID]->Fill(locThrownPMag, locDeltaT);
		dHistMap_DeltaXVsP[locPID]->Fill(locThrownPMag, locDeltaX);
		dHistMap_DeltaYVsP[locPID]->Fill(locThrownPMag, locDeltaY);
		dHistMap_dEVsP[locPID]->Fill(locThrownPMag, locdE_MeV);
		Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
	}

	return true;
}

void DHistogramAction_TrackMultiplicity::Initialize(JEventLoop* locEventLoop)
{
	dThrownTopology = new string;
	dDetectedTopology = new string;

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();

	string locLabelName;
	string locHistName("dHist_NumReconstructedTracks");
	if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
		dHist_NumReconstructedTracks = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
	else
	{
		dHist_NumReconstructedTracks = new TH2D("NumReconstructedTracks", ";Track Type;Num Tracks / Event", 5 + dFinalStatePIDs.size(), -0.5, 4.5 + dFinalStatePIDs.size(), dMaxNumTracks + 1, -0.5, (float)dMaxNumTracks + 0.5);
		dHist_NumReconstructedTracks->GetXaxis()->SetBinLabel(1, "# Total");
		dHist_NumReconstructedTracks->GetXaxis()->SetBinLabel(2, "# q = +");
		dHist_NumReconstructedTracks->GetXaxis()->SetBinLabel(3, "# q = -");
		dHist_NumReconstructedTracks->GetXaxis()->SetBinLabel(4, "# q = 0");
		dHist_NumReconstructedTracks->GetXaxis()->SetBinLabel(5, "# q != 0");
		for(size_t loc_i = 0; loc_i < dFinalStatePIDs.size(); ++loc_i)
		{
			locLabelName = string("# ") + string(ParticleName_ROOT(dFinalStatePIDs[loc_i]));
			dHist_NumReconstructedTracks->GetXaxis()->SetBinLabel(6 + loc_i, locLabelName.c_str());
		}
	}

	string locTreeName("dTree_TrackTopologies");
	if(gDirectory->Get(locTreeName.c_str()) != NULL) //already created by another thread
		dTree_TrackTopologies = static_cast<TTree*>(gDirectory->Get(locTreeName.c_str()));
	else
	{
		dTree_TrackTopologies = new TTree(locTreeName.c_str(), "TrackTopologies");
		dTree_TrackTopologies->Branch("Thrown_String", dThrownTopology);
		dTree_TrackTopologies->Branch("Detected_String", dDetectedTopology);
	}

	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_TrackMultiplicity::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);
	if(!locPreviousParticleCombos.empty())
		return true; //else double-counting!

	vector<const DChargedTrack*> locChargedTracks;
	locEventLoop->Get(locChargedTracks);

	vector<const DNeutralParticle*> locNeutralParticles;
	locEventLoop->Get(locNeutralParticles);

	const DMCThrown* locMCThrown;
	Particle_t locPID;
	vector<const DMCThrownMatching*> locMCThrownMatchingVector;
	locEventLoop->Get(locMCThrownMatchingVector);
	const DMCThrownMatching* locMCThrownMatching = locMCThrownMatchingVector.empty() ? NULL : locMCThrownMatchingVector[0];

	size_t locNumPositiveTracks = 0;
	size_t locNumNegativeTracks = 0;
	for(size_t loc_i = 0; loc_i < locChargedTracks.size(); ++loc_i)
	{
		if(locChargedTracks[loc_i]->Get_BestFOM()->charge() > 0.0)
			++locNumPositiveTracks;
		else
			++locNumNegativeTracks;
	}

	// get #tracks by pid type //USES MC PID IF EXISTS!!!
	map<Particle_t, size_t> locNumTracksByPID;

	// charged by pid
	for(size_t loc_i = 0; loc_i < locChargedTracks.size(); ++loc_i)
	{
		if(locMCThrownMatching != NULL)
		{
			locMCThrown = locMCThrownMatching->Get_MatchingMCThrown(locChargedTracks[loc_i]);
			if(locMCThrown == NULL)
				locPID = locChargedTracks[loc_i]->Get_BestFOM()->PID();
			else
				locPID = (Particle_t)locMCThrown->type;
		}
		else
			locPID = locChargedTracks[loc_i]->Get_BestFOM()->PID();

		if(locNumTracksByPID.find(locPID) != locNumTracksByPID.end())
			++locNumTracksByPID[locPID];
		else
			locNumTracksByPID[locPID] = 1;
	}

	// neutrals by pid
	for(size_t loc_i = 0; loc_i < locNeutralParticles.size(); ++loc_i)
	{
		if(locMCThrownMatching != NULL)
		{
			locMCThrown = locMCThrownMatching->Get_MatchingMCThrown(locNeutralParticles[loc_i]);
			if(locMCThrown == NULL)
				locPID = locNeutralParticles[loc_i]->Get_BestFOM()->PID();
			else
				locPID = (Particle_t)locMCThrown->type;
		}
		else
			locPID = locNeutralParticles[loc_i]->Get_BestFOM()->PID();

		if(locNumTracksByPID.find(locPID) != locNumTracksByPID.end())
			++locNumTracksByPID[locPID];
		else
			locNumTracksByPID[locPID] = 1;
	}

	vector<const DReaction*> locThrownReactions;
	locEventLoop->Get(locThrownReactions, "Thrown");

	//sort thrown pids
	string locThrownTopology;
	if(!locThrownReactions.empty())
	{
		deque<Particle_t> locThrownPIDs;
		locThrownReactions[0]->Get_DetectedFinalPIDs(locThrownPIDs, true);

		deque<pair<Particle_t, unsigned int> > locSortedNumParticlesByType_Thrown; //sorted by charge (+/0/-), then mass (high/low)
		deque<pair<Particle_t, unsigned int> >::iterator locIterator, locInsertLocation;
		for(size_t loc_i = 0; loc_i < locThrownPIDs.size(); ++loc_i)
		{
			bool locMatchFoundFlag = false;
			locInsertLocation = locSortedNumParticlesByType_Thrown.end();
			for(locIterator = locSortedNumParticlesByType_Thrown.begin(); locIterator != locSortedNumParticlesByType_Thrown.end(); ++locIterator)
			{
				Particle_t locStoredPID = (*locIterator).first;
				if(locStoredPID == locThrownPIDs[loc_i])
				{
					locMatchFoundFlag = true;
					++((*locIterator).second);
					break;
				}
				if(ParticleCharge(locThrownPIDs[loc_i]) > ParticleCharge(locStoredPID))
				{
					locInsertLocation = locIterator;
					break;
				}
				if((ParticleCharge(locThrownPIDs[loc_i]) == ParticleCharge(locStoredPID)) && (ParticleMass(locThrownPIDs[loc_i]) > ParticleMass(locStoredPID)))
				{
					locInsertLocation = locIterator;
					break;
				}
			}
			if(locMatchFoundFlag)
				continue;
			locSortedNumParticlesByType_Thrown.insert(locInsertLocation, pair<Particle_t, unsigned int>(locThrownPIDs[loc_i], 1));
		}
/*
cout << "SORTED THROWN:" << endl;
for(size_t loc_i = 0; loc_i < locSortedNumParticlesByType_Thrown.size(); ++loc_i)
{
	Particle_t locPID = locSortedNumParticlesByType_Thrown[loc_i].first;
	cout << "q, mass, # = " << ParticleCharge(locPID) << ", " << ParticleMass(locPID) << ", " << locSortedNumParticlesByType_Thrown[loc_i].second << endl;
}
*/
		for(size_t loc_i = 0; loc_i < locSortedNumParticlesByType_Thrown.size(); ++loc_i)
		{
			unsigned int locNumParticles = locSortedNumParticlesByType_Thrown[loc_i].second;
			if(locNumParticles > 1)
			{
				ostringstream locStream;
				locStream << locNumParticles;
				locThrownTopology += locStream.str();
			}
			locThrownTopology += ParticleName_ROOT(locSortedNumParticlesByType_Thrown[loc_i].first);
			if(loc_i != (locSortedNumParticlesByType_Thrown.size() - 1))
				locThrownTopology += ", ";
		}
	}

	//sort detected pids
	string locDetectedTopology;
	deque<pair<Particle_t, unsigned int> > locSortedNumParticlesByType_Detected; //sorted by charge (+/0/-), then mass (high/low)
	deque<pair<Particle_t, unsigned int> >::iterator locIterator, locInsertLocation;
	map<Particle_t, size_t>::iterator locMapIterator;
	for(locMapIterator = locNumTracksByPID.begin(); locMapIterator != locNumTracksByPID.end(); ++locMapIterator)
	{
		locInsertLocation = locSortedNumParticlesByType_Detected.end();
		for(locIterator = locSortedNumParticlesByType_Detected.begin(); locIterator != locSortedNumParticlesByType_Detected.end(); ++locIterator)
		{
			Particle_t locStoredPID = (*locIterator).first;
			if(ParticleCharge(locMapIterator->first) > ParticleCharge(locStoredPID))
			{
				locInsertLocation = locIterator;
				break;
			}
			if((ParticleCharge(locMapIterator->first) == ParticleCharge(locStoredPID)) && (ParticleMass(locMapIterator->first) > ParticleMass(locStoredPID)))
			{
				locInsertLocation = locIterator;
				break;
			}
		}
		locSortedNumParticlesByType_Detected.insert(locInsertLocation, pair<Particle_t, unsigned int>(locMapIterator->first, locMapIterator->second));
	}
/*
cout << "SORTED DETECTED:" << endl;
for(size_t loc_i = 0; loc_i < locSortedNumParticlesByType_Detected.size(); ++loc_i)
{
	Particle_t locPID = locSortedNumParticlesByType_Detected[loc_i].first;
	cout << "q, mass, # = " << ParticleCharge(locPID) << ", " << ParticleMass(locPID) << ", " << locSortedNumParticlesByType_Detected[loc_i].second << endl;
}
*/
	for(size_t loc_i = 0; loc_i < locSortedNumParticlesByType_Detected.size(); ++loc_i)
	{
		unsigned int locNumParticles = locSortedNumParticlesByType_Detected[loc_i].second;
		if(locNumParticles > 1)
		{
			ostringstream locStream;
			locStream << locNumParticles;
			locDetectedTopology += locStream.str();
		}
		locDetectedTopology += ParticleName_ROOT(locSortedNumParticlesByType_Detected[loc_i].first);
		if(loc_i != (locSortedNumParticlesByType_Detected.size() - 1))
			locDetectedTopology += ", ";
	}

	Get_Application()->RootWriteLock();
	{
		dHist_NumReconstructedTracks->Fill(0.0, (Double_t)(locChargedTracks.size() + locNeutralParticles.size()));
		dHist_NumReconstructedTracks->Fill(1.0, (Double_t)locNumPositiveTracks);
		dHist_NumReconstructedTracks->Fill(2.0, (Double_t)locNumNegativeTracks);
		dHist_NumReconstructedTracks->Fill(3.0, (Double_t)locNeutralParticles.size());
		dHist_NumReconstructedTracks->Fill(4.0, (Double_t)locChargedTracks.size());
		for(size_t loc_i = 0; loc_i < dFinalStatePIDs.size(); ++loc_i)
			dHist_NumReconstructedTracks->Fill(5.0 + (Double_t)loc_i, (Double_t)locNumTracksByPID[dFinalStatePIDs[loc_i]]);

		*dThrownTopology = locThrownTopology;
		*dDetectedTopology = locDetectedTopology;
		dTree_TrackTopologies->SetBranchAddress("Thrown_String", &dThrownTopology);
		dTree_TrackTopologies->SetBranchAddress("Detected_String", &dDetectedTopology);
		dTree_TrackTopologies->Fill();
	}
	Get_Application()->RootUnLock();

	return true;
}

DHistogramAction_TrackMultiplicity::~DHistogramAction_TrackMultiplicity(void)
{
	if(dThrownTopology != NULL)
		delete dThrownTopology;
	if(dDetectedTopology != NULL)
		delete dDetectedTopology;
}

void DHistogramAction_TruePID::Initialize(JEventLoop* locEventLoop)
{
	string locStepName, locStepROOTName, locHistTitle, locHistName, locParticleName, locParticleROOTName;
	Particle_t locPID;

	size_t locNumSteps = Get_Reaction()->Get_NumReactionSteps();
	dHistDeque_P_CorrectID.resize(locNumSteps);
	dHistDeque_P_IncorrectID.resize(locNumSteps);
	dHistDeque_PVsTheta_CorrectID.resize(locNumSteps);
	dHistDeque_PVsTheta_IncorrectID.resize(locNumSteps);

	deque<deque<Particle_t> > locDetectedPIDs;
	Get_Reaction()->Get_DetectedFinalPIDs(locDetectedPIDs);

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();
	for(size_t loc_i = 0; loc_i < locNumSteps; ++loc_i)
	{
		if(locDetectedPIDs[loc_i].empty())
			continue;

		const DReactionStep* locReactionStep = Get_Reaction()->Get_ReactionStep(loc_i);
		locStepName = locReactionStep->Get_StepName();
		locStepROOTName = locReactionStep->Get_StepROOTName();
		CreateAndChangeTo_Directory(locStepName, locStepName);

		for(size_t loc_j = 0; loc_j < locDetectedPIDs[loc_i].size(); ++loc_j)
		{
			locPID = locDetectedPIDs[loc_i][loc_j];
			locParticleName = ParticleType(locPID);
			locParticleROOTName = ParticleName_ROOT(locPID);

			if(dHistDeque_P_CorrectID[loc_i].find(locPID) != dHistDeque_P_CorrectID[loc_i].end())
				continue; //hists already created for this pid

			//P of Correct ID
			locHistName = string("Momentum_CorrectID_") + locParticleName;
			locHistTitle = string("Correct ") + locParticleROOTName + string(" ID, ") + locStepROOTName + string(";p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_P_CorrectID[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_P_CorrectID[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPBins, dMinP, dMaxP);

			//P of Incorrect ID
			locHistName = string("Momentum_IncorrectID_") + locParticleName;
			locHistTitle = string("Incorrect ") + locParticleROOTName + string(" ID, ") + locStepROOTName + string(";p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_P_IncorrectID[loc_i][locPID] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_P_IncorrectID[loc_i][locPID] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPBins, dMinP, dMaxP);

			//P Vs Theta of Correct ID
			locHistName = string("PVsTheta_CorrectID_") + locParticleName;
			locHistTitle = string("Correct ") + locParticleROOTName + string(" ID, ") + locStepROOTName + string(";#theta#circ;p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_PVsTheta_CorrectID[loc_i][locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_PVsTheta_CorrectID[loc_i][locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNumThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);

			//P Vs Theta of Incorrect ID
			locHistName = string("PVsTheta_IncorrectID_") + locParticleName;
			locHistTitle = string("Incorrect ") + locParticleROOTName + string(" ID, ") + locStepROOTName + string(";#theta#circ;p (GeV/c)");
			if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
				dHistDeque_PVsTheta_IncorrectID[loc_i][locPID] = static_cast<TH2D*>(gDirectory->Get(locHistName.c_str()));
			else
				dHistDeque_PVsTheta_IncorrectID[loc_i][locPID] = new TH2D(locHistName.c_str(), locHistTitle.c_str(), dNumThetaBins, dMinTheta, dMaxTheta, dNum2DPBins, dMinP, dMaxP);
		}
		gDirectory->cd("..");
	} //end of step loop

	//# Combos Pass/Fail All True PID
	locHistName = "Combo_TruePIDStatus";
	locHistTitle = Get_Reaction()->Get_ReactionName() + string(";# Combos;All Combo Particles True PID Status");
	if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
		dHist_TruePIDStatus = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
	else
		dHist_TruePIDStatus = new TH1D(locHistName.c_str(), locHistTitle.c_str(), 2, -0.5, 1.5);

	//# Combos in Signal Region Pass/Fail All True PID
	if(dMinMassSq < dMaxMassSq)
	{
		locHistName = "Combo_TruePIDStatus_SignalRegion";
		locHistTitle = Get_Reaction()->Get_ReactionName() + string(";# Combos in ") + ParticleName_ROOT(dInitialPID) + string(" Signal Region;All Combo Particles True PID Status");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHist_TruePIDStatus_SignalRegion = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHist_TruePIDStatus_SignalRegion = new TH1D(locHistName.c_str(), locHistTitle.c_str(), 2, -0.5, 1.5);
	}
	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_TruePID::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	vector<const DMCThrownMatching*> locMCThrownMatchingVector;
	locEventLoop->Get(locMCThrownMatchingVector);
	if(locMCThrownMatchingVector.empty())
		return true;
	const DMCThrownMatching* locMCThrownMatching = locMCThrownMatchingVector[0];
	double locP, locTheta;
	const DMCThrown* locMCThrown;
	Particle_t locPID;

	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);

	deque<const DKinematicData*> locParticles;
	int locComboTruePIDStatus = 1;
	for(size_t loc_i = 0; loc_i < locParticleCombo->Get_NumParticleComboSteps(); ++loc_i)
	{
		const DParticleComboStep* locParticleComboStep = locParticleCombo->Get_ParticleComboStep(loc_i);
		locParticleComboStep->Get_DetectedFinalParticles_Measured(locParticles);

		for(size_t loc_j = 0; loc_j < locParticles.size(); ++loc_j)
		{
			locPID = locParticles[loc_j]->PID();

			if(ParticleCharge(locPID) == 0)
				locMCThrown = locMCThrownMatching->Get_MatchingMCThrown(static_cast<const DNeutralParticleHypothesis*>(locParticles[loc_j]));
			else
				locMCThrown = locMCThrownMatching->Get_MatchingMCThrown(static_cast<const DChargedTrackHypothesis*>(locParticles[loc_j]));

			bool locCutResult = (locMCThrown == NULL) ? false : (((Particle_t)locMCThrown->type) == locPID);
			if(!locCutResult)
				locComboTruePIDStatus = 0;

			if(Get_AnalysisUtilities()->Find_SimilarCombos(pair<const DKinematicData*, size_t>(locParticles[loc_j], loc_i), locPreviousParticleCombos))
				continue;

			locP = locParticles[loc_j]->momentum().Mag();
			locTheta = locParticles[loc_j]->momentum().Theta()*180.0/TMath::Pi();

			Get_Application()->RootWriteLock();
			if(locCutResult)
			{
				dHistDeque_P_CorrectID[loc_i][locPID]->Fill(locP);
				dHistDeque_PVsTheta_CorrectID[loc_i][locPID]->Fill(locTheta, locP);
			}
			else
			{
				dHistDeque_P_IncorrectID[loc_i][locPID]->Fill(locP);
				dHistDeque_PVsTheta_IncorrectID[loc_i][locPID]->Fill(locTheta, locP);
			}
			Get_Application()->RootUnLock();
		}
	}
	dHist_TruePIDStatus->Fill(locComboTruePIDStatus);

	if(dMinMassSq < dMaxMassSq)
	{
		bool locInSignalRegionFlag = true; //all possibilities have to be in the signal region
		for(size_t loc_i = 0; loc_i < locParticleCombo->Get_NumParticleComboSteps(); ++loc_i)
		{
			const DParticleComboStep* locParticleComboStep = locParticleCombo->Get_ParticleComboStep(loc_i);
			if(locParticleComboStep->Get_InitialParticleID() != dInitialPID)
				continue;

			DLorentzVector locFinalStateP4 = Get_AnalysisUtilities()->Calc_FinalStateP4(locParticleCombo, loc_i, Get_UseKinFitResultsFlag());
			if((locFinalStateP4.M2() < dMinMassSq) || (locFinalStateP4.M2() > dMaxMassSq))
			{
				locInSignalRegionFlag = false;
				break;
			}
		}
		if(locInSignalRegionFlag)
			dHist_TruePIDStatus_SignalRegion->Fill(locComboTruePIDStatus);
	}
	return true;
}

void DHistogramAction_InvariantMass::Initialize(JEventLoop* locEventLoop)
{
	string locHistName, locHistTitle;
	double locMassPerBin = 1000.0*(dMaxMass - dMinMass)/((double)dNumMassBins);

	//setup dFinalParticleNames: all possible pid combos that can give rise to the desired particle
	deque<string> locParticleNamesForHist;
	Get_Reaction()->Get_DecayChainFinalParticlesROOTNames(dInitialPID, dFinalParticleNames, locParticleNamesForHist, Get_UseKinFitResultsFlag());

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();
	for(size_t loc_i = 0; loc_i < locParticleNamesForHist.size(); ++loc_i)
	{
		locHistName = "InvariantMass";
		if(locParticleNamesForHist.size() > 1)
			locHistName += string("_") + locParticleNamesForHist[loc_i];
		ostringstream locStream;
		locStream << locMassPerBin;
		locHistTitle = string(";") + locParticleNamesForHist[loc_i] + string(" Invariant Mass (GeV/c^{2});# Combos / ") + locStream.str() + string(" MeV/c^{2}");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHistDeque_InvaraintMass.push_back(static_cast<TH1D*>(gDirectory->Get(locHistName.c_str())));
		else
			dHistDeque_InvaraintMass.push_back(new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumMassBins, dMinMass, dMaxMass));
	}
	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_InvariantMass::Compare_ParticleNames(const deque<string>& locParticleNames1, const deque<string>& locParticleNames2) const
{
	deque<string>::const_iterator locIterator3;
	deque<string>::iterator locIterator4;
	if(locParticleNames1.size() != locParticleNames2.size())
		return false; //not same size, clearly can't be the same
	deque<string> locParticleNamesCopy = locParticleNames2;

	//loop over the lists of particles, see if they're identical
	for(locIterator3 = locParticleNames1.begin(); locIterator3 != locParticleNames1.end(); ++locIterator3)
	{
		for(locIterator4 = locParticleNamesCopy.begin(); locIterator4 != locParticleNamesCopy.end(); ++locIterator4)
		{
			if((*locIterator3) == (*locIterator4))
			{
				locParticleNamesCopy.erase(locIterator4); //particle name is identical, remove it from the list of remaining names
				break;
			}
		}
	}
	return locParticleNamesCopy.empty(); //all names removed means all names matched: duplicate
}

bool DHistogramAction_InvariantMass::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	deque<const DParticleComboStep*> locParticleComboSteps;
	deque<const DKinematicData*> locParticles;
	string locFinalParticleName;
	DLorentzVector locFinalStateP4;
	double locInvariantMass;

	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);

	for(size_t loc_i = 0; loc_i < locParticleCombo->Get_NumParticleComboSteps(); ++loc_i)
	{
		const DParticleComboStep* locParticleComboStep = locParticleCombo->Get_ParticleComboStep(loc_i);
		if(locParticleComboStep->Get_InitialParticleID() != dInitialPID)
			continue;
		if(!Get_UseKinFitResultsFlag()) //measured
		{
			if((!dEnableDoubleCounting) && Get_AnalysisUtilities()->Find_SimilarCombos_AnyStep(locParticleCombo, loc_i, locPreviousParticleCombos))
				continue; //dupe: already histed!
		}
		else
		{
			if((!dEnableDoubleCounting) && Get_AnalysisUtilities()->Find_SimilarCombos_KinFit(locParticleCombo, locPreviousParticleCombos))
				continue; //dupe: already histed!
		}

		locFinalStateP4 = Get_AnalysisUtilities()->Calc_FinalStateP4(locParticleCombo, loc_i, Get_UseKinFitResultsFlag());

		//get particle names so can select the correct histogram
		deque<string> locParticleNames;
		locParticleCombo->Get_DecayChainFinalParticlesROOTName(loc_i, locParticleNames, Get_UseKinFitResultsFlag());

		for(size_t loc_j = 0; loc_j < dFinalParticleNames.size(); ++loc_j)
		{
			if(Compare_ParticleNames(dFinalParticleNames[loc_j], locParticleNames))
			{
				locInvariantMass = locFinalStateP4.M();
				Get_Application()->RootWriteLock();
				dHistDeque_InvaraintMass[loc_j]->Fill(locInvariantMass);
				Get_Application()->RootUnLock();
				break;
			}
		}
	}
	return true;
}

void DHistogramAction_MissingMass::Initialize(JEventLoop* locEventLoop)
{
	string locHistName, locHistTitle;
	double locMassPerBin = 1000.0*(dMaxMass - dMinMass)/((double)dNumMassBins);
	string locFinalParticlesROOTName = Get_Reaction()->Get_DetectedParticlesROOTName();
	string locInitialParticlesROOTName = Get_Reaction()->Get_InitialParticlesROOTName();

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();
	locHistName = "MissingMass";
	ostringstream locStream;
	locStream << locMassPerBin;
	locHistTitle = string(";") + locInitialParticlesROOTName + string("#rightarrow") + locFinalParticlesROOTName + string(" Missing Mass (GeV/c^{2});# Combos / ") + locStream.str() + string(" MeV/c^{2}");
	if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
		dHist_MissingMass = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
	else
		dHist_MissingMass = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumMassBins, dMinMass, dMaxMass);
	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_MissingMass::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);

	if(!Get_UseKinFitResultsFlag()) //measured
	{
		if((!dEnableDoubleCounting) && Get_AnalysisUtilities()->Find_SimilarCombos_AnyStep(locParticleCombo, 0, locPreviousParticleCombos))
			return true; //dupe: already histed!
	}
	else //kinfit
	{
		if((!dEnableDoubleCounting) && Get_AnalysisUtilities()->Find_SimilarCombos_KinFit(locParticleCombo, locPreviousParticleCombos))
			return true; //dupe: already histed!
	}

	double locMissingMass = (Get_AnalysisUtilities()->Calc_MissingP4(locParticleCombo, Get_UseKinFitResultsFlag())).M();
	Get_Application()->RootWriteLock();
	dHist_MissingMass->Fill(locMissingMass);
	Get_Application()->RootUnLock();
	return true;
}

void DHistogramAction_MissingMassSquared::Initialize(JEventLoop* locEventLoop)
{
	string locHistName, locHistTitle;
	double locMassSqPerBin = 1000.0*1000.0*(dMaxMassSq - dMinMassSq)/((double)dNumMassBins);
	string locFinalParticlesROOTName = Get_Reaction()->Get_DetectedParticlesROOTName();
	string locInitialParticlesROOTName = Get_Reaction()->Get_InitialParticlesROOTName();

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();
	locHistName = "MissingMassSquared";
	ostringstream locStream;
	locStream << locMassSqPerBin;
	locHistTitle = string(";") + locInitialParticlesROOTName + string("#rightarrow") + locFinalParticlesROOTName + string(" Missing Mass Squared (GeV/c^{2})^{2};# Combos / ") + locStream.str() + string(" (MeV/c^{2})^{2}");
	if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
		dHist_MissingMassSquared = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
	else
		dHist_MissingMassSquared = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumMassBins, dMinMassSq, dMaxMassSq);
	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

bool DHistogramAction_MissingMassSquared::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);

	if(!Get_UseKinFitResultsFlag()) //measured
	{
		if((!dEnableDoubleCounting) && Get_AnalysisUtilities()->Find_SimilarCombos_AnyStep(locParticleCombo, 0, locPreviousParticleCombos))
			return true; //dupe: already histed!
	}
	else //kinfit
	{
		if((!dEnableDoubleCounting) && Get_AnalysisUtilities()->Find_SimilarCombos_KinFit(locParticleCombo, locPreviousParticleCombos))
			return true; //dupe: already histed!
	}

	double locMissingMassSq = (Get_AnalysisUtilities()->Calc_MissingP4(locParticleCombo, Get_UseKinFitResultsFlag())).M2();
	Get_Application()->RootWriteLock();
	dHist_MissingMassSquared->Fill(locMissingMassSq);
	Get_Application()->RootUnLock();
	return true;
}

void DHistogramAction_KinFitResults::Initialize(JEventLoop* locEventLoop)
{
	string locHistName, locHistTitle, locParticleName, locParticleROOTName, locStepName, locStepROOTName;
	Particle_t locPID;

	DKinFitType locKinFitType = Get_Reaction()->Get_KinFitType();
	if(locKinFitType == d_NoFit)
		return;

	deque<deque<Particle_t> > locDetectedPIDs;
	Get_Reaction()->Get_DetectedFinalPIDs(locDetectedPIDs);

	//CREATE THE HISTOGRAMS
	Get_Application()->RootWriteLock(); //ACQUIRE ROOT LOCK!!
	CreateAndChangeTo_ActionDirectory();

	string locKinFitTypeString;
	if(locKinFitType == d_P4Fit)
		locKinFitTypeString = "P4";
	else if(locKinFitType == d_VertexFit)
		locKinFitTypeString = "Vertex";
	else if(locKinFitType == d_SpacetimeFit)
		locKinFitTypeString = "Spacetime";
	else if(locKinFitType == d_P4AndVertexFit)
		locKinFitTypeString = "P4 & Vertex";
	else if(locKinFitType == d_P4AndSpacetimeFit)
		locKinFitTypeString = "P4 & Spacetime";

	// Confidence Level
	locHistName = "ConfidenceLevel";
	locHistTitle = locKinFitTypeString + string(" Kinematic Fit;Confidence Level;# Combos");
	if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
		dHist_ConfidenceLevel = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
	else
		dHist_ConfidenceLevel = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumConfidenceLevelBins, 0.0, 1.0);

	// Pulls
	map<DKinFitPullType, TH1D*> locParticlePulls;

	//beam pulls
	bool locBeamFlag = (Get_Reaction()->Get_ReactionStep(0)->Get_InitialParticleID() == Gamma);
	if(locBeamFlag)
	{
		CreateAndChangeTo_Directory("Beam", "Beam");
		Create_ParticlePulls(true, "Beam", Gamma, dHistMap_BeamPulls, locKinFitTypeString);
		gDirectory->cd("..");
	}

	//final particle pulls
	for(size_t loc_i = 0; loc_i < Get_Reaction()->Get_NumReactionSteps(); ++loc_i)
	{
		const DReactionStep* locReactionStep = Get_Reaction()->Get_ReactionStep(loc_i);
		locStepName = locReactionStep->Get_StepName();
		locStepROOTName = locReactionStep->Get_StepROOTName();

		if(locDetectedPIDs[loc_i].empty())
			continue;

		CreateAndChangeTo_Directory(locStepName, locStepName);

		for(size_t loc_j = 0; loc_j < locDetectedPIDs[loc_i].size(); ++loc_j)
		{
			locPID = locDetectedPIDs[loc_i][loc_j];
			locParticleName = ParticleType(locPID);
			CreateAndChangeTo_Directory(locParticleName, locParticleName);

			Create_ParticlePulls(false, locStepROOTName, locPID, locParticlePulls, locKinFitTypeString);
			dHistMap_Pulls[pair<size_t, Particle_t>(loc_i, locPID)] = locParticlePulls;

			gDirectory->cd("..");
		} //end of particle loop
		gDirectory->cd("..");
	} //end of step loop

	//RF Time Pull
	if(locBeamFlag && ((locKinFitType == d_SpacetimeFit) || (locKinFitType == d_P4AndSpacetimeFit)))
	{
		CreateAndChangeTo_Directory("RF", "RF");

		//T Pull
		locHistName = "Pull_RF_T";
		locHistTitle = string("RF Bunch, ") + locKinFitTypeString + string(" Fit;t Pull;# Combos");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			dHist_RFTimePull = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			dHist_RFTimePull = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPullBins, dMinPull, dMaxPull);

		gDirectory->cd("..");
	}

	Get_Application()->RootUnLock(); //RELEASE ROOT LOCK!!
}

void DHistogramAction_KinFitResults::Create_ParticlePulls(bool locIsBeamFlag, string locStepROOTName, Particle_t locPID, map<DKinFitPullType, TH1D*>& locParticlePulls, const string& locKinFitTypeString)
{
	string locHistName, locHistTitle, locParticleName, locParticleROOTName;
	locParticleName = ParticleType(locPID);
	locParticleROOTName = ParticleName_ROOT(locPID);

	DKinFitType locKinFitType = Get_Reaction()->Get_KinFitType();

	locParticlePulls.clear();

	bool locNeutralShowerFlag = ((ParticleCharge(locPID) == 0) && (!locIsBeamFlag) && (locKinFitType != d_P4Fit));
	//p4 pulls:
	if(locNeutralShowerFlag)
	{
		//neutral shower not in a p4-only fit
		//E Pull
		locHistName = "Pull_E";
		locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(", ") + locKinFitTypeString + string(" Fit;E Pull;# Combos");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			locParticlePulls[d_EPull] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			locParticlePulls[d_EPull] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPullBins, dMinPull, dMaxPull);
	}
	else
	{
		//Px Pull
		locHistName = "Pull_Px";
		locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(", ") + locKinFitTypeString + string(" Fit;p_{x} Pull;# Combos");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			locParticlePulls[d_PxPull] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			locParticlePulls[d_PxPull] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPullBins, dMinPull, dMaxPull);

		//Py Pull
		locHistName = "Pull_Py";
		locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(", ") + locKinFitTypeString + string(" Fit;p_{y} Pull;# Combos");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			locParticlePulls[d_PyPull] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			locParticlePulls[d_PyPull] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPullBins, dMinPull, dMaxPull);

		//Pz Pull
		locHistName = "Pull_Pz";
		locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(", ") + locKinFitTypeString + string(" Fit;p_{z} Pull;# Combos");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			locParticlePulls[d_PzPull] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			locParticlePulls[d_PzPull] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPullBins, dMinPull, dMaxPull);
	}

	//vertex pulls:
	if(locNeutralShowerFlag || (locKinFitType == d_VertexFit) || (locKinFitType == d_SpacetimeFit) || (locKinFitType == d_P4AndVertexFit) || (locKinFitType == d_P4AndSpacetimeFit))
	{
		//Xx Pull
		locHistName = "Pull_Xx";
		locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(", ") + locKinFitTypeString + string(" Fit;x_{x} Pull;# Combos");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			locParticlePulls[d_XxPull] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			locParticlePulls[d_XxPull] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPullBins, dMinPull, dMaxPull);

		//Xy Pull
		locHistName = "Pull_Xy";
		locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(", ") + locKinFitTypeString + string(" Fit;x_{y} Pull;# Combos");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			locParticlePulls[d_XyPull] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			locParticlePulls[d_XyPull] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPullBins, dMinPull, dMaxPull);

		//Xz Pull
		locHistName = "Pull_Xz";
		locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(", ") + locKinFitTypeString + string(" Fit;x_{z} Pull;# Combos");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			locParticlePulls[d_XzPull] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			locParticlePulls[d_XzPull] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPullBins, dMinPull, dMaxPull);
	}

	//time pulls:
	if((locKinFitType == d_SpacetimeFit) || (locKinFitType == d_P4AndSpacetimeFit))
	{
		//T Pull
		locHistName = "Pull_T";
		locHistTitle = locParticleROOTName + string(", ") + locStepROOTName + string(", ") + locKinFitTypeString + string(" Fit;t Pull;# Combos");
		if(gDirectory->Get(locHistName.c_str()) != NULL) //already created by another thread, or directory name is duplicate (e.g. two identical steps)
			locParticlePulls[d_TPull] = static_cast<TH1D*>(gDirectory->Get(locHistName.c_str()));
		else
			locParticlePulls[d_TPull] = new TH1D(locHistName.c_str(), locHistTitle.c_str(), dNumPullBins, dMinPull, dMaxPull);
	}
}

bool DHistogramAction_KinFitResults::Perform_Action(JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo)
{
	//kinfit results are unique for each DParticleCombo: no need to check for duplicates
	const DKinFitResults* locKinFitResults = locParticleCombo->Get_KinFitResults();
	if(locKinFitResults == NULL)
		return true;

	deque<pair<const DParticleCombo*, bool> > locPreviousParticleCombos;
	Get_PreviousParticleCombos(locPreviousParticleCombos);
	if(Get_AnalysisUtilities()->Find_SimilarCombos_KinFit(locParticleCombo, locPreviousParticleCombos))
		return true; //dupe: already histed!

	// Confidence Level
	double locConfidenceLevel = locKinFitResults->Get_ConfidenceLevel();
	Get_Application()->RootWriteLock();
	{
		dHist_ConfidenceLevel->Fill(locConfidenceLevel);
	}
	Get_Application()->RootUnLock();

	if(locConfidenceLevel < dPullHistConfidenceLevelCut)
		return true; //don't histogram pulls

	// Pulls
	map<const DKinematicData*, map<DKinFitPullType, double> > locPulls; //DKinematicData is the MEASURED particle
	locKinFitResults->Get_Pulls(locPulls);
	deque<const DKinematicData*> locParticles;
	map<DKinFitPullType, double> locParticlePulls;
	map<DKinFitPullType, double>::iterator locIterator;
	const DKinematicData* locKinematicData;

	// beam pulls
	bool locBeamFlag = (Get_Reaction()->Get_ReactionStep(0)->Get_InitialParticleID() == Gamma);
	if(locBeamFlag)
	{
		locKinematicData = locParticleCombo->Get_ParticleComboStep(0)->Get_InitialParticle_Measured();
		locParticlePulls = locPulls[locKinematicData];
		Get_Application()->RootWriteLock();
		for(locIterator = locParticlePulls.begin(); locIterator != locParticlePulls.end(); ++locIterator)
			dHistMap_BeamPulls[locIterator->first]->Fill(locIterator->second);
		Get_Application()->RootUnLock();
	}

	// final particle pulls
	for(size_t loc_i = 0; loc_i < locParticleCombo->Get_NumParticleComboSteps(); ++loc_i)
	{
		const DParticleComboStep* locParticleComboStep = locParticleCombo->Get_ParticleComboStep(loc_i);
		locParticleComboStep->Get_DetectedFinalParticles_Measured(locParticles);
		for(size_t loc_j = 0; loc_j < locParticles.size(); ++loc_j)
		{
			locParticlePulls = locPulls[locParticles[loc_j]];
			pair<size_t, Particle_t> locParticlePair(loc_i, locParticles[loc_j]->PID());
			Get_Application()->RootWriteLock();
			for(locIterator = locParticlePulls.begin(); locIterator != locParticlePulls.end(); ++locIterator)
				(dHistMap_Pulls[locParticlePair])[locIterator->first]->Fill(locIterator->second);
			Get_Application()->RootUnLock();
		}
	}

	//rf time pull
	DKinFitType locKinFitType = Get_Reaction()->Get_KinFitType();
	if(locBeamFlag && ((locKinFitType == d_SpacetimeFit) || (locKinFitType == d_P4AndSpacetimeFit)))
	{
		locParticlePulls = locPulls[NULL];
		Get_Application()->RootWriteLock();
		dHist_RFTimePull->Fill(locParticlePulls[d_TPull]);
		Get_Application()->RootUnLock();
	}

	return true;
}

