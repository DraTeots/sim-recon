#include "DParticleComboBlueprint_factory.h"

//------------------
// init
//------------------
jerror_t DParticleComboBlueprint_factory::init(void)
{
	MAX_DParticleComboBlueprintStepPoolSize = 40;
	return NOERROR;

	dDebugLevel = 0;
	dMinProtonMomentum = pair<bool, double>(false, -1.0);
	dMinIndividualChargedPIDFOM = pair<bool, double>(false, -1.0);
	dMinIndividualTrackingFOM = pair<bool, double>(false, -1.0);
}

//------------------
// brun
//------------------
jerror_t DParticleComboBlueprint_factory::brun(jana::JEventLoop* locEventLoop, int runnumber)
{
	gPARMS->SetDefaultParameter("COMBOBLUEPRINTS:DEBUGLEVEL", dDebugLevel);

	// In the following try-catch blocks, gPARMS->GetParameter will throw an
	// exception if the parameter doesn't exist leaving both the X.second and
	// X.first elements of the relevant variable untouched. If the parameter
	// does exist, the value is copied into X.second and the X.first value
	// gets set to true on the subsequent line.

	try{
		gPARMS->GetParameter("COMBO:MINPROTONMOMENTUM", dMinProtonMomentum.second);
		dMinProtonMomentum.first = true;
	}catch(...){}

	try{
		gPARMS->GetParameter("COMBO:MININDIVIDUALCHARGEDPIDFOM", dMinIndividualChargedPIDFOM.second);
		dMinIndividualChargedPIDFOM.first = true;
	}catch(...){}

	try{
		gPARMS->GetParameter("COMBO:MININDIVIDUALTRACKINGFOM", dMinIndividualTrackingFOM.second);
		dMinIndividualTrackingFOM.first = true;
	}catch(...){}

	return NOERROR;
}

//------------------
// evnt
//------------------
jerror_t DParticleComboBlueprint_factory::evnt(JEventLoop *locEventLoop, int eventnumber)
{
	Reset_Pools();

	// Get list of factories and find all the ones producing
	// DReaction objects. (A simpler way to do this would be to
	// just use locEventLoop->Get(...), but then only one plugin could
	// be used at a time.)
	vector<const DReaction*> locReactions;
	vector<JFactory_base*> locFactories = locEventLoop->GetFactories();
	for(size_t loc_i = 0; loc_i < locFactories.size(); ++loc_i)
	{
		JFactory<DReaction>* locFactory = dynamic_cast<JFactory<DReaction>* >(locFactories[loc_i]);
		if(locFactory == NULL)
			continue;
		if(string(locFactory->Tag()) == "Thrown")
			continue;
		// Found a factory producing DReactions. The reaction objects are
		// produced at the init stage and are persistent through all event
		// processing so we can grab the list here and append it to our
		// overall list.
		vector<const DReaction*> locReactionsSubset;
		locFactory->Get(locReactionsSubset);
		locReactions.insert(locReactions.end(), locReactionsSubset.begin(), locReactionsSubset.end());
	}

	for(size_t loc_i = 0; loc_i < locReactions.size(); ++loc_i)
		Build_ParticleComboBlueprints(locEventLoop, locReactions[loc_i]);

	return NOERROR;
}

jerror_t DParticleComboBlueprint_factory::Build_ParticleComboBlueprints(JEventLoop* locEventLoop, const DReaction* locReaction)
{
	vector<const DChargedTrack*> locChargedTrackVector;
	locEventLoop->Get(locChargedTrackVector);
	vector<const DNeutralShower*> locNeutralShowerVector;
	locEventLoop->Get(locNeutralShowerVector);

	deque<const JObject*> locNeutralShowerDeque;
	for(size_t loc_i = 0; loc_i < locNeutralShowerVector.size(); ++loc_i)
		locNeutralShowerDeque.push_back(static_cast<const JObject*>(locNeutralShowerVector[loc_i]));

	if(dDebugLevel > 0)
		cout << "Reaction Name, # Reaction steps = " << locReaction->Get_ReactionName() << ", " << locReaction->Get_NumReactionSteps() << endl;
	if(locReaction->Get_NumReactionSteps() == 0)
		return RESOURCE_UNAVAILABLE;

	//make sure not more than one missing particle
	size_t locNumMissingParticles = 0;
	for(size_t loc_i = 0; loc_i < locReaction->Get_NumReactionSteps(); ++loc_i)
	{
		if(locReaction->Get_ReactionStep(loc_i)->Get_MissingParticleIndex() != -1)
			++locNumMissingParticles;
	}
	if(locNumMissingParticles > 1)
	{
		cout << "ERROR: Too many missing particles in DReaction.  No DParticleComboBlueprints generated." << endl;
		return RESOURCE_UNAVAILABLE;
	}

	//sort charged particles into +/-
	deque<const JObject*> locChargedTrackDeque_Positive;
	deque<const JObject*> locChargedTrackDeque_Negative;
	const DChargedTrack* locChargedTrack;
	//Note that a DChargedTrack object can sometimes contain both positively and negatively charged hypotheses simultaneously: sometimes the tracking flips the sign of the track
	for(size_t loc_i = 0; loc_i < locChargedTrackVector.size(); ++loc_i)
	{
		locChargedTrack = locChargedTrackVector[loc_i];
		bool locCouldBePositiveFlag = false;
		bool locCouldBeNegativeFlag = false;
		for(size_t loc_j = 0; loc_j < locChargedTrack->dChargedTrackHypotheses.size(); ++loc_j)
		{
			if(locChargedTrack->dChargedTrackHypotheses[loc_j]->charge() > 0.0)
				locCouldBePositiveFlag = true;
			else
				locCouldBeNegativeFlag = true;
		}
		if(locCouldBePositiveFlag)
			locChargedTrackDeque_Positive.push_back(static_cast<const JObject*>(locChargedTrack));
		if(locCouldBeNegativeFlag)
			locChargedTrackDeque_Negative.push_back(static_cast<const JObject*>(locChargedTrack));
	}

	if(dDebugLevel > 0)
		cout << "#+, #-, #0 particles = " << locChargedTrackDeque_Positive.size() << ", " << locChargedTrackDeque_Negative.size() << ", " << locNeutralShowerDeque.size() << endl;

	//set up combo loop
	deque<deque<int> > locResumeAtIndexDeque; //1st index is step, 2nd is particle (the initial particle, then the final particles)
	deque<deque<int> > locNumPossibilitiesDeque; //1st index is step, 2nd is particle (the initial particle, then the final particles)
	if(!Setup_ComboLoop(locReaction, locNeutralShowerDeque.size(), locChargedTrackVector.size(), locChargedTrackDeque_Positive.size(), locChargedTrackDeque_Negative.size(), locResumeAtIndexDeque, locNumPossibilitiesDeque))
	{
		if(dDebugLevel > 0)
			cout << "not enough detected particles with the correct charges for the event: no combos found." << endl;
		return NOERROR;
	}

	if(dDebugLevel > 10)
	{
		cout << "locResumeAtIndexDeque: ";
		for(size_t loc_i = 0; loc_i < locResumeAtIndexDeque.size(); ++loc_i)
		{
			for(size_t loc_j = 0; loc_j < locResumeAtIndexDeque[loc_i].size(); ++loc_j)
				cout << locResumeAtIndexDeque[loc_i][loc_j] << ", ";
			cout << ";";
		}
		cout << endl;
		cout << "locNumPossibilitiesDeque: ";
		for(size_t loc_i = 0; loc_i < locNumPossibilitiesDeque.size(); ++loc_i)
		{
			for(size_t loc_j = 0; loc_j < locNumPossibilitiesDeque[loc_i].size(); ++loc_j)
				cout << locNumPossibilitiesDeque[loc_i][loc_j] << ", ";
			cout << ";";
		}
		cout << endl;
	}

	//find the combos!!
	vector<DParticleComboBlueprint*> locParticleComboBlueprints;
	Find_Combos(locReaction, locNeutralShowerDeque, locChargedTrackDeque_Positive, locChargedTrackDeque_Negative, locResumeAtIndexDeque, locNumPossibilitiesDeque, locParticleComboBlueprints);

	if(dDebugLevel > 10)
	{
		cout << "print pointers: " << endl;
		for(size_t loc_i = 0; loc_i < locParticleComboBlueprints.size(); ++loc_i)
		{
			cout << "COMBO " << loc_i << endl;
			for(size_t loc_j = 0; loc_j < locParticleComboBlueprints[loc_i]->Get_NumParticleComboBlueprintSteps(); ++loc_j)
			{
				cout << "Step " << loc_j << " pointers: ";
				for(size_t loc_k = 0; loc_k < locParticleComboBlueprints[loc_i]->Get_ParticleComboBlueprintStep(loc_j)->Get_NumFinalParticleSourceObjects(); ++loc_k)
					cout << locParticleComboBlueprints[loc_i]->Get_ParticleComboBlueprintStep(loc_j)->Get_FinalParticle_SourceObject(loc_k) << ", ";
				cout << endl;
			}
		}
	}

	for(size_t loc_i = 0; loc_i < locParticleComboBlueprints.size(); ++loc_i)
		_data.push_back(locParticleComboBlueprints[loc_i]);

	return NOERROR;
}

bool DParticleComboBlueprint_factory::Setup_ComboLoop(const DReaction* locReaction, int locNumDetectedNeutralParticles, int locNumDetectedChargedParticles, int locNumDetectedPositiveParticles, int locNumDetectedNegativeParticles, deque<deque<int> >& locResumeAtIndexDeque, deque<deque<int> >& locNumPossibilitiesDeque)
{
	//setup locResumeAtIndexDeque, & locNumPossibilitiesDeque
	Particle_t locAnalysisPID;
	int locMissingParticleIndex;
	unsigned int locNumSteps = locReaction->Get_NumReactionSteps();
	locResumeAtIndexDeque.clear();
	locResumeAtIndexDeque.clear();
	int locCharge;
	int locNumNeededChargedParticles = 0, locNumNeededPositiveParticles = 0, locNumNeededNegativeParticles = 0, locNumNeededNeutralParticles = 0;
	for(size_t loc_i = 0; loc_i < locNumSteps; ++loc_i)
	{
		const DReactionStep* locReactionStep = locReaction->Get_ReactionStep(loc_i);
		size_t locNumFinalParticles = locReactionStep->Get_NumFinalParticleIDs();

		//setup final state particles num possibilities & resume-at index deque
		deque<int> locTempDeque(locNumFinalParticles, 0);
		locResumeAtIndexDeque.push_back(locTempDeque);
		locMissingParticleIndex = locReactionStep->Get_MissingParticleIndex();
		for(size_t loc_j = 0; loc_j < locNumFinalParticles; ++loc_j)
		{
			locAnalysisPID = locReactionStep->Get_FinalParticleID(loc_j);

			if(locMissingParticleIndex == int(loc_j))
			{
				locTempDeque[loc_j] = 1;
				continue;
			}

			//check to see if it's decay is represented in a future step
			locTempDeque[loc_j] = 0;
			for(size_t loc_k = loc_i + 1; loc_k < locNumSteps; ++loc_k)
			{
				if(locReaction->Get_ReactionStep(loc_k)->Get_InitialParticleID() == locAnalysisPID)
					++(locTempDeque[loc_j]);
			}
			if(locTempDeque[loc_j] > 0)
				continue; //does decay, got the #combos

			//else use detected particles
			locCharge = ParticleCharge(locAnalysisPID);
			if(locCharge > 0)
			{
				++locNumNeededPositiveParticles;
				++locNumNeededChargedParticles;
				locTempDeque[loc_j] = locNumDetectedPositiveParticles;
			}
			else if(locCharge < 0)
			{
				++locNumNeededNegativeParticles;
				++locNumNeededChargedParticles;
				locTempDeque[loc_j] = locNumDetectedNegativeParticles;
			}
			else
			{
				++locNumNeededNeutralParticles;
				locTempDeque[loc_j] = locNumDetectedNeutralParticles;
			}
		}
		locNumPossibilitiesDeque.push_back(locTempDeque);
	}

	if((locNumNeededPositiveParticles > locNumDetectedPositiveParticles) || (locNumNeededNegativeParticles > locNumDetectedNegativeParticles))
		return false; //not enough particles of a given charge for the event
	if((locNumNeededChargedParticles > locNumDetectedChargedParticles) || (locNumNeededNeutralParticles > locNumDetectedNeutralParticles))
		return false; //not enough particles of a given charge for the event //#charged can fail here if a charged track has hypotheses with different charges
	return true;
}

void DParticleComboBlueprint_factory::Find_Combos(const DReaction* locReaction, deque<const JObject*>& locNeutralShowerDeque, deque<const JObject*>& locChargedTrackDeque_Positive, deque<const JObject*>& locChargedTrackDeque_Negative, deque<deque<int> >& locResumeAtIndexDeque, const deque<deque<int> >& locNumPossibilitiesDeque, vector<DParticleComboBlueprint*>& locParticleComboBlueprints)
{
	DParticleComboBlueprint* locParticleComboBlueprint = new DParticleComboBlueprint();
	locParticleComboBlueprint->Set_Reaction(locReaction);

	int locStepIndex = 0;
	int locParticleIndex = 0; //final = 0 -> (#final - 1)
	DParticleComboBlueprintStep* locParticleComboBlueprintStep = Get_ParticleComboBlueprintStepResource();
	locParticleComboBlueprintStep->Set_ReactionStep(locReaction->Get_ReactionStep(0));
	locParticleComboBlueprintStep->Set_InitialParticleDecayFromStepIndex(-1);

	do{
		if(dDebugLevel > 10)
			cout << "do loop: step & particle indices = " << locStepIndex << ", " << locParticleIndex << endl;
		if(locParticleIndex == int(locNumPossibilitiesDeque[locStepIndex].size()))
		{
			if(dDebugLevel > 10)
				cout << "end of reaction step" << endl;
			if(!Handle_EndOfReactionStep(locReaction, locParticleComboBlueprint, locParticleComboBlueprintStep, locStepIndex, locParticleIndex, locResumeAtIndexDeque, locNumPossibilitiesDeque, locParticleComboBlueprints))
				break;
			continue;
		}

		//get Analysis PID & the resume-at index
		int locMissingParticleIndex = locReaction->Get_ReactionStep(locStepIndex)->Get_MissingParticleIndex();
		Particle_t locAnalysisPID = locReaction->Get_ReactionStep(locStepIndex)->Get_FinalParticleID(locParticleIndex);
		int& locResumeAtIndex = locResumeAtIndexDeque[locStepIndex][locParticleIndex];

		if(dDebugLevel > 10)
			cout << "do loop: locAnalysisPID, locMissingParticleIndex, locResumeAtIndex = " << ParticleType(locAnalysisPID) << ", " << locMissingParticleIndex << ", " << locResumeAtIndex << endl;

		//handle if this is a missing particle
		if(locMissingParticleIndex == locParticleIndex)
		{
			if(dDebugLevel > 10)
				cout << "missing particle" << endl;
			// e.g. on neutron in g, p -> pi+, (n)
			//only one possibility ("missing"), so just set NULL and advance
			locParticleComboBlueprintStep->Add_FinalParticle_SourceObject(NULL, -1); //missing
			locResumeAtIndex = 1;
			++locParticleIndex;
			continue;
		}

		//if two particles of same type in a step: locResumeAtIndex must always be >= the previous locResumeAtIndex (prevents duplicates) e.g. g, D -> p, p, pi-
		//search for same pid previously in this step (and non-missing)
		for(int loc_i = locParticleIndex - 1; loc_i >= 0; --loc_i)
		{
			if(loc_i == locMissingParticleIndex)
				continue;
			if(locReaction->Get_ReactionStep(locStepIndex)->Get_FinalParticleID(loc_i) == locAnalysisPID)
			{
				if(locResumeAtIndex < locResumeAtIndexDeque[locStepIndex][loc_i])
					locResumeAtIndex = locResumeAtIndexDeque[locStepIndex][loc_i];
				if(dDebugLevel > 10)
					cout << "dupe type in step; locResumeAtIndex = " << locResumeAtIndex << endl;
				break;
			}
		}

		//if all of the particle types in this step are identical to all of the particle types in a previous step (and none of either are missing), regardless of the order they are listed:
			//the locResumeAtIndex system of the new step's particles must be >= the locResumeAtIndex system of the particles in the LAST OF the matching previous steps (e.g. could be 3x pi0s)
				//in other words: if 2x pi0 -> g, g; AB, CD ok and CD, AB bad; but also AD, BC ok (BC system occurs after AD system)
		//only check if on the last particle index of the step: particles could be in a different order, difficult to tell in advance if the combo will be bad
		if((locStepIndex > 0) && (locParticleIndex == int(locNumPossibilitiesDeque[locStepIndex].size() - 1)) && (locMissingParticleIndex == -1))
		{
			//last particle of the step
			if(Check_IfDuplicateStepCombo(locReaction, locStepIndex, locResumeAtIndexDeque, locNumPossibilitiesDeque)) //make sure none of the dupe particles are missing
			{
				if(dDebugLevel > 10)
					cout << "duplicate step combo" << endl;
				if(!Handle_Decursion(locParticleComboBlueprint, locResumeAtIndexDeque, locNumPossibilitiesDeque, locParticleIndex, locStepIndex, locParticleComboBlueprintStep))
					break;
				continue;
			}
		}

		// check to see if this particle has a decay that is represented in a future step
		// e.g. on Lambda in g, p -> K+, Lambda; where a later step is Lambda -> p, pi-
		int locDecayStepIndex = Grab_DecayingParticle(locParticleComboBlueprint, locAnalysisPID, locResumeAtIndex, locReaction, locStepIndex, locParticleIndex, locParticleComboBlueprintStep);
		if(locDecayStepIndex >= 0)
		{
			if(dDebugLevel > 10)
				cout << "decaying particle" << endl;
			locParticleComboBlueprintStep->Add_FinalParticle_SourceObject(NULL, locDecayStepIndex); //decaying
			++locParticleIndex;
			continue;
		}

		// else grab a detected track
		const JObject* locSourcObject = Grab_DetectedTrack(locParticleComboBlueprint, locAnalysisPID, locResumeAtIndex, locNeutralShowerDeque, locChargedTrackDeque_Positive, locChargedTrackDeque_Negative);
		if(locSourcObject == NULL)
		{
			if(dDebugLevel > 10)
				cout << "can't find detected particle" << endl;
			if(!Handle_Decursion(locParticleComboBlueprint, locResumeAtIndexDeque, locNumPossibilitiesDeque, locParticleIndex, locStepIndex, locParticleComboBlueprintStep))
				break;
			continue;
		}

		if(dDebugLevel > 10)
			cout << "detected track found, locResumeAtIndex now = " << locResumeAtIndex << endl;
		locParticleComboBlueprintStep->Add_FinalParticle_SourceObject(locSourcObject, -2); //detected
		++locParticleIndex;
	}
	while(true);
	delete locParticleComboBlueprint; //delete the last, extra one
}

bool DParticleComboBlueprint_factory::Handle_EndOfReactionStep(const DReaction* locReaction, DParticleComboBlueprint*& locParticleComboBlueprint, DParticleComboBlueprintStep*& locParticleComboBlueprintStep, int& locStepIndex, int& locParticleIndex, deque<deque<int> >& locResumeAtIndexDeque, const deque<deque<int> >& locNumPossibilitiesDeque, vector<DParticleComboBlueprint*>& locParticleComboBlueprints)
{
	//end of step, advance to next step

	//first check to see if identical to a previous step; if so, just save the old step and recycle the current one
	bool locRecycleFlag = false;
	for(size_t loc_i = 0; loc_i < locParticleComboBlueprints.size(); ++loc_i)
	{
		for(size_t loc_j = 0; loc_j < locParticleComboBlueprints[loc_i]->Get_NumParticleComboBlueprintSteps(); ++loc_j)
		{
			//could have two identical steps in the same reaction (e.g. pi0 decays)
			const DParticleComboBlueprintStep* locPreviousParticleComboBlueprintStep = locParticleComboBlueprints[loc_i]->Get_ParticleComboBlueprintStep(loc_j);
			if((*locPreviousParticleComboBlueprintStep) != (*locParticleComboBlueprintStep))
				continue; //else identical: just use the previously saved one and recycle the newly constructed one
			if(dDebugLevel > 10)
				cout << "step identical to previous one: copying & recycling." << endl;
			Recycle_ParticleComboBlueprintStep(locParticleComboBlueprintStep);
			locParticleComboBlueprintStep = NULL;
			locParticleComboBlueprint->Add_ParticleComboBlueprintStep(locPreviousParticleComboBlueprintStep);
			locRecycleFlag = true;
			break;
		}
		if(locRecycleFlag)
			break;
	}
	//if match not found, try searching previous dreactions too (may be identical reactions or similar enough)
	if(!locRecycleFlag)
	{
		for(size_t loc_i = 0; loc_i < _data.size(); ++loc_i)
		{
			for(size_t loc_j = 0; loc_j < _data[loc_i]->Get_NumParticleComboBlueprintSteps(); ++loc_j)
			{
				const DParticleComboBlueprintStep* locPreviousParticleComboBlueprintStep = _data[loc_i]->Get_ParticleComboBlueprintStep(loc_j);
				if((*locPreviousParticleComboBlueprintStep) != (*locParticleComboBlueprintStep))
					continue; //else identical: just use the previously saved one and recycle the newly constructed one
				if(dDebugLevel > 10)
					cout << "step identical to one from previous DReaction: copying & recycling." << endl;
				Recycle_ParticleComboBlueprintStep(locParticleComboBlueprintStep);
				locParticleComboBlueprintStep = NULL;
				locParticleComboBlueprint->Add_ParticleComboBlueprintStep(locPreviousParticleComboBlueprintStep);
				locRecycleFlag = true;
				break;
			}
			if(locRecycleFlag)
				break;
		}
	}
	if(!locRecycleFlag)
		locParticleComboBlueprint->Add_ParticleComboBlueprintStep(locParticleComboBlueprintStep);

	++locStepIndex;
	if(dDebugLevel > 10)
		cout << "handle end: new step index, #steps = " << locStepIndex << ", " << locReaction->Get_NumReactionSteps() << endl;
	if(locStepIndex != int(locReaction->Get_NumReactionSteps()))
	{
		// did not complete the chain yet
		locParticleIndex = 0;

		locParticleComboBlueprintStep = Get_ParticleComboBlueprintStepResource();
		locParticleComboBlueprintStep->Set_ReactionStep(locReaction->Get_ReactionStep(locStepIndex));
		//loop back through previous steps, see where the initial particle of the next (not-just-finished) step decayed from
		bool locMatchFoundFlag = false;
		for(int loc_i = 0; loc_i < locStepIndex; ++loc_i)
		{
			for(int loc_j = 0; loc_j < int(locParticleComboBlueprint->Get_ParticleComboBlueprintStep(loc_i)->Get_NumFinalParticleSourceObjects()); ++loc_j)
			{
				if(dDebugLevel > 10)
					cout << "previous step index, previous step final particle index, dDecayStepIndex, locStepIndex = " << loc_i << ", " << loc_j << ", " << locParticleComboBlueprint->Get_ParticleComboBlueprintStep(loc_i)->Get_DecayStepIndex(loc_j) << ", " << locStepIndex << endl;
				if(locParticleComboBlueprint->Get_ParticleComboBlueprintStep(loc_i)->Get_DecayStepIndex(loc_j) == locStepIndex)
				{
					locParticleComboBlueprintStep->Set_InitialParticleDecayFromStepIndex(loc_i);
					if(dDebugLevel > 10)
						cout << "initial particle (" << ParticleType(locParticleComboBlueprintStep->Get_InitialParticleID()) << ") found in previous step index: " << loc_i << endl;
					locMatchFoundFlag = true;
					break;
				}
			}
			if(locMatchFoundFlag)
				break;
		}
		if(!locMatchFoundFlag)
			return false; //reaction setup incorrectly: just break

		return true;
	}

	//constructed, handle decursion
	locParticleComboBlueprints.push_back(locParticleComboBlueprint);
	locParticleComboBlueprint = Clone_ParticleComboBlueprint(locParticleComboBlueprint);
	locParticleComboBlueprintStep = NULL;
	if(!Handle_Decursion(locParticleComboBlueprint, locResumeAtIndexDeque, locNumPossibilitiesDeque, locParticleIndex, locStepIndex, locParticleComboBlueprintStep))
		return false;
	return true;
}

bool DParticleComboBlueprint_factory::Handle_Decursion(DParticleComboBlueprint* locParticleComboBlueprint, deque<deque<int> >& locResumeAtIndexDeque, const deque<deque<int> >& locNumPossibilitiesDeque, int& locParticleIndex, int& locStepIndex, DParticleComboBlueprintStep*& locParticleComboBlueprintStep)
{
	do
	{
		if(dDebugLevel > 50)
			cout << "decursion: step, particle indices = " << locStepIndex << ", " << locParticleIndex << endl;
		if(locStepIndex == int(locResumeAtIndexDeque.size())) //just saved a test reaction
		{
			if(dDebugLevel > 50)
				cout << "saved test reaction" << endl;
			--locStepIndex;
			locParticleComboBlueprintStep = Get_ParticleComboBlueprintStepResource();
			*locParticleComboBlueprintStep = *(locParticleComboBlueprint->Pop_ParticleComboBlueprintStep());
			locParticleIndex = locResumeAtIndexDeque[locStepIndex].size() - 1;
			locParticleComboBlueprintStep->Pop_FinalParticle_SourceObject();
			if(dDebugLevel > 50)
				cout << "step index, particle index, resume at, #possible = " << locStepIndex << ", " << locParticleIndex << ", " << locResumeAtIndexDeque[locStepIndex][locParticleIndex] << ", " << locNumPossibilitiesDeque[locStepIndex][locParticleIndex] << endl;
			continue;
		}
		else if(locParticleIndex == int(locNumPossibilitiesDeque[locStepIndex].size())) //end of a step: step was found to be a duplicate
		{
			//is this even possible anymore?
			if(dDebugLevel > 50)
				cout << "end of a step" << endl;
			--locParticleIndex;
			locParticleComboBlueprintStep->Pop_FinalParticle_SourceObject();
			if(dDebugLevel > 50)
				cout << "step index, particle index, resume at, #possible = " << locStepIndex << ", " << locParticleIndex << ", " << locResumeAtIndexDeque[locStepIndex][locParticleIndex] << ", " << locNumPossibilitiesDeque[locStepIndex][locParticleIndex] << endl;
			continue;
		}

		//locParticleIndex will represent the particle it failed to find a combo for
		locResumeAtIndexDeque[locStepIndex][locParticleIndex] = 0;
		--locParticleIndex;
		if(locParticleIndex >= 0) //else is initial particle (will pop the entire step)
			locParticleComboBlueprintStep->Pop_FinalParticle_SourceObject();
		else
		{
			if(dDebugLevel > 50)
				cout << "pop the step" << endl;
			--locStepIndex;
			if(locStepIndex < 0)
				return false;
			dParticleComboBlueprintStepPool_Available.push_back(locParticleComboBlueprintStep); //recycle step!
			locParticleComboBlueprintStep = Get_ParticleComboBlueprintStepResource();
			*locParticleComboBlueprintStep = *(locParticleComboBlueprint->Pop_ParticleComboBlueprintStep());
			locParticleIndex = locResumeAtIndexDeque[locStepIndex].size() - 1;
			locParticleComboBlueprintStep->Pop_FinalParticle_SourceObject();
		}
		if(dDebugLevel > 50)
			cout << "resume at, #possible = " << locResumeAtIndexDeque[locStepIndex][locParticleIndex] << ", " << locNumPossibilitiesDeque[locStepIndex][locParticleIndex] << endl;
	}
	while(locResumeAtIndexDeque[locStepIndex][locParticleIndex] == locNumPossibilitiesDeque[locStepIndex][locParticleIndex]);
	return true;
}

bool DParticleComboBlueprint_factory::Check_IfDuplicateStepCombo(const DReaction* locReaction, int locStepIndex, deque<deque<int> >& locResumeAtIndexDeque, const deque<deque<int> >& locNumPossibilitiesDeque) const
{
	//note that final particle ids could be rearranged in a different order
	const DReactionStep* locCurrentStep = locReaction->Get_ReactionStep(locStepIndex);
	map<Particle_t, unsigned int> locParticleTypeCount_CurrentStep;
	for(size_t loc_i = 0; loc_i < locCurrentStep->Get_NumFinalParticleIDs(); ++loc_i)
	{
		Particle_t locPID = locCurrentStep->Get_FinalParticleID(loc_i);
		if(locParticleTypeCount_CurrentStep.find(locPID) == locParticleTypeCount_CurrentStep.end())
			locParticleTypeCount_CurrentStep[locPID] = 1;
		else
			++(locParticleTypeCount_CurrentStep[locPID]);
	}

	//search previous steps in reverse order for a match (identical particles, none missing, may be in a different order)
	for(int loc_i = locStepIndex - 1; loc_i > 0; --loc_i)
	{
		int locPreviousStepIndex = loc_i;
		const DReactionStep* locPreviousStep = locReaction->Get_ReactionStep(locPreviousStepIndex);
		if(locPreviousStep->Get_MissingParticleIndex() != -1)
			continue; //missing particle somewhere in the step: cannot be a duplicate (already have checked not more than one missing particle)

		if(locPreviousStep->Get_InitialParticleID() != locCurrentStep->Get_InitialParticleID())
			continue;
		if(locPreviousStep->Get_TargetParticleID() != locCurrentStep->Get_TargetParticleID())
			continue;

		map<Particle_t, unsigned int> locParticleTypeCount_CurrentStepCopy = locParticleTypeCount_CurrentStep;
		bool locMatchFlag = true;
		for(size_t loc_j = 0; loc_j < locPreviousStep->Get_NumFinalParticleIDs(); ++loc_j)
		{
			Particle_t locPID = locPreviousStep->Get_FinalParticleID(loc_j);
			if(locParticleTypeCount_CurrentStepCopy.find(locPID) == locParticleTypeCount_CurrentStepCopy.end())
			{
				locMatchFlag = false;
				break;
			}
			if(locParticleTypeCount_CurrentStepCopy[locPID] == 1)
				locParticleTypeCount_CurrentStepCopy.erase(locPID); //if another one, will fail .find() check
			else
				--(locParticleTypeCount_CurrentStepCopy[locPID]);
		}
		if(!locParticleTypeCount_CurrentStepCopy.empty())
			locMatchFlag = false;
		if(!locMatchFlag)
			continue;
		//step is a match
		if(dDebugLevel > 10)
			cout << "step at index = " << locStepIndex << " matches a previous step at index " << locPreviousStepIndex << endl;

		//the resume-at index of the first particle listed in the previous step MUST be less than that of the first particle listed in the current step that has the same pid
			//else the current combo was already previously used for the previous step
		Particle_t locPreviousFirstPID = locPreviousStep->Get_FinalParticleID(0);
		int locPreviousResumeAtIndex = locResumeAtIndexDeque[locPreviousStepIndex][0];
		for(size_t loc_j = 0; loc_j < locCurrentStep->Get_NumFinalParticleIDs(); ++loc_j)
		{
			if(locCurrentStep->Get_FinalParticleID(loc_j) != locPreviousFirstPID)
				continue;
			int locCurrentResumeAtIndex = locResumeAtIndexDeque[locStepIndex][loc_j];
			if(dDebugLevel > 10)
				cout << "previous first pid, previous resume index, current resume index = " << locPreviousFirstPID << ", " << locPreviousResumeAtIndex << ", " << locCurrentResumeAtIndex << endl;
			if(locCurrentResumeAtIndex < locPreviousResumeAtIndex)
			{
				if(loc_j == (locCurrentStep->Get_NumFinalParticleIDs() - 1))
				{
					//on the last particle index (particle not selected yet): advance resume-at index to the smallest-possible, non-duplicate value
					locResumeAtIndexDeque[locStepIndex][loc_j] = locPreviousResumeAtIndex;
					if(dDebugLevel > 10)
						cout << "resume-at index updated to " << locPreviousResumeAtIndex << endl;
				}
				else
				{
					//not the last particle index ((bad) particle already selected): invalid combo, force abort
					if(dDebugLevel > 10)
						cout << "not the last particle index ((bad) particle already selected): invalid combo, force abort" << endl;
					return true;
				}
			}

			if(dDebugLevel > 10)
				cout << "combo is ok (step is duplicate, but combo is not)" << endl;
			return false; //else combo is ok (step is duplicate, but combo is not)
		}
	}

	if(dDebugLevel > 10)
		cout << "step at index = " << locStepIndex << " has no similar steps." << endl;

	return false;
}

int DParticleComboBlueprint_factory::Grab_DecayingParticle(DParticleComboBlueprint* locParticleComboBlueprint, Particle_t locAnalysisPID, int& locResumeAtIndex, const DReaction* locReaction, int locStepIndex, int locParticleIndex, DParticleComboBlueprintStep* locParticleComboBlueprintStep)
{
	if(dDebugLevel > 10)
		cout << "check if " << ParticleType(locAnalysisPID) << " decays later in the reaction." << endl;
	if((locAnalysisPID == Gamma) || (locAnalysisPID == Electron) || (locAnalysisPID == Positron) || (locAnalysisPID == Proton) || (locAnalysisPID == AntiProton))
	{
		if(dDebugLevel > 10)
			cout << ParticleType(locAnalysisPID) << " does not decay later in the reaction." << endl;
		return -2; //these particles don't decay: don't search!
	}

	int locCurrentIndex = -1;
	const DReactionStep* locReactionStep;
	for(size_t loc_i = locStepIndex + 1; loc_i < locReaction->Get_NumReactionSteps(); ++loc_i)
	{
		locReactionStep = locReaction->Get_ReactionStep(loc_i);
		if(locReactionStep->Get_InitialParticleID() != locAnalysisPID)
			continue;

		//have a match, but skip it if not at least at the correct resume index
		++locCurrentIndex;
		if(dDebugLevel > 10)
			cout << "match: current, resume at indices = " << locCurrentIndex << ", " << locResumeAtIndex << endl;
		if(locCurrentIndex < locResumeAtIndex)
			continue;

		//check to make sure not already used previously
		bool locPreviouslyUsedFlag = false;
		for(int loc_j = 0; loc_j < locStepIndex; ++loc_j)
		{
			for(size_t loc_k = 0; loc_k < locParticleComboBlueprint->Get_ParticleComboBlueprintStep(loc_j)->Get_NumFinalParticleSourceObjects(); ++loc_k)
			{
				if(dDebugLevel > 15)
					cout << "i, j, k, dDecayStepIndex = " << loc_i << ", " << loc_j << ", " << loc_k << ", " << locParticleComboBlueprint->Get_ParticleComboBlueprintStep(loc_j)->Get_DecayStepIndex(loc_k) << endl;
				if(locParticleComboBlueprint->Get_ParticleComboBlueprintStep(loc_j)->Get_DecayStepIndex(loc_k) == int(loc_i))
				{
					if(dDebugLevel > 15)
						cout << "used previously" << endl;
					locPreviouslyUsedFlag = true;
					break;
				}
			}
			if(locPreviouslyUsedFlag)
				break;
		}
		if(locPreviouslyUsedFlag)
			continue; //this one has been previously used: continue;

		//check to see if it's been used previously in the current step also:
		for(int loc_j = 0; loc_j < locParticleIndex; ++loc_j)
		{
			if(dDebugLevel > 15)
				cout << "i, j, dDecayStepIndex = " << loc_i << ", " << loc_j << ", " << locParticleComboBlueprintStep->Get_DecayStepIndex(loc_j) << endl;
			if(locParticleComboBlueprintStep->Get_DecayStepIndex(loc_j) == int(loc_i))
			{
				if(dDebugLevel > 15)
					cout << "used previously" << endl;
				locPreviouslyUsedFlag = true;
				break;
			}
		}
		if(locPreviouslyUsedFlag)
			continue; //this one has been previously used: continue;

		//else it's good!!
		locResumeAtIndex = locCurrentIndex + 1;
		if(dDebugLevel > 10)
			cout << ParticleType(locAnalysisPID) << " decays later in the reaction, at step index " << loc_i << endl;

		return loc_i;
	}

	if(dDebugLevel > 10)
		cout << ParticleType(locAnalysisPID) << " does not decay later in the reaction." << endl;
	return -2;
}

const JObject* DParticleComboBlueprint_factory::Grab_DetectedTrack(DParticleComboBlueprint* locParticleComboBlueprint, Particle_t locAnalysisPID, int& locResumeAtIndex, deque<const JObject*>& locNeutralShowerDeque, deque<const JObject*>& locChargedTrackDeque_Positive, deque<const JObject*>& locChargedTrackDeque_Negative)
{
	int locAnalysisCharge = ParticleCharge(locAnalysisPID);
	if(dDebugLevel > 10)
		cout << "Grab_DetectedTrack: PID, Charge = " << ParticleType(locAnalysisPID) << ", " << locAnalysisCharge << endl;
	if(locAnalysisCharge == 0)
		return Choose_SourceObject(locParticleComboBlueprint->Get_Reaction(), locAnalysisPID, locParticleComboBlueprint, locNeutralShowerDeque, locResumeAtIndex);
	else if(locAnalysisCharge > 0)
		return Choose_SourceObject(locParticleComboBlueprint->Get_Reaction(), locAnalysisPID, locParticleComboBlueprint, locChargedTrackDeque_Positive, locResumeAtIndex);
	else
		return Choose_SourceObject(locParticleComboBlueprint->Get_Reaction(), locAnalysisPID, locParticleComboBlueprint, locChargedTrackDeque_Negative, locResumeAtIndex);
}

const JObject* DParticleComboBlueprint_factory::Choose_SourceObject(const DReaction* locReaction, Particle_t locAnalysisPID, DParticleComboBlueprint* locParticleComboBlueprint, deque<const JObject*>& locSourceObjects, int& locResumeAtIndex) const
{
	if(dDebugLevel > 10)
		cout << "Choose_SourceObject: resume at, #possible = " << locResumeAtIndex << ", " << locSourceObjects.size() << endl;
	if(locResumeAtIndex >= int(locSourceObjects.size()))
		return NULL;
	const JObject* locObject;

	const DParticleComboBlueprintStep* locParticleComboBlueprintStep;
	do
	{
		locObject = locSourceObjects[locResumeAtIndex];
		//make sure not used currently
		bool locTrackInUseFlag = false;
		for(size_t loc_i = 0; loc_i < locParticleComboBlueprint->Get_NumParticleComboBlueprintSteps(); ++loc_i)
		{
			locParticleComboBlueprintStep = locParticleComboBlueprint->Get_ParticleComboBlueprintStep(loc_i);
			for(size_t loc_j = 0; loc_j < locParticleComboBlueprintStep->Get_NumFinalParticleSourceObjects(); ++loc_j)
			{
				if(locObject == locParticleComboBlueprintStep->Get_FinalParticle_SourceObject(loc_j))
				{
					if(dDebugLevel > 20)
						cout << "Source object already in use for locResumeAtIndex = " << locResumeAtIndex << endl;
					locTrackInUseFlag = true;
					break;
				}
			}
			if(locTrackInUseFlag)
				break;
		}
		++locResumeAtIndex;
		if(locTrackInUseFlag)
			continue;

		const DChargedTrack* locChargedTrack = dynamic_cast<const DChargedTrack*>(locObject); //NULL if not charged


		//if charged, check to make sure the PID FOM is OK (cut garbage tracks and wildly bad combos)
		if(locChargedTrack != NULL)
		{
			const DChargedTrackHypothesis* locChargedTrackHypothesis = locChargedTrack->Get_Hypothesis(locAnalysisPID);
			if(locChargedTrackHypothesis != NULL)
			{
				if((!Cut_PIDFOM(locReaction, locChargedTrackHypothesis)) || (!Cut_TrackingFOM(locReaction, locChargedTrackHypothesis)))
				{
					if(dDebugLevel > 20)
						cout << "Bad PID or Tracking FOM; PID FOM = " << locChargedTrackHypothesis->dFOM << endl;
					continue;
				}
			}
		}

		//check to make sure the track momentum isn't too low (e.g. testing a 100 MeV pion to be a proton)
		bool locTrackMomentumTooLowFlag = false;
		pair<bool, double> locMinProtonMomentum = dMinProtonMomentum.first ? dMinProtonMomentum : locReaction->Get_MinProtonMomentum();
		if((locChargedTrack != NULL) && locMinProtonMomentum.first && (ParticleMass(locAnalysisPID) >= (ParticleMass(Proton) - 0.001)))
		{
			if(locChargedTrack->Get_Hypothesis(Proton) == NULL)
			{
				if(locChargedTrack->Get_Hypothesis(KPlus) != NULL)
				{
					if(dDebugLevel > 20)
						cout << "Proton candidate: K+ momentum = " << locChargedTrack->Get_Hypothesis(KPlus)->momentum().Mag() << endl;
					if(locChargedTrack->Get_Hypothesis(KPlus)->momentum().Mag() < locMinProtonMomentum.second)
						locTrackMomentumTooLowFlag = true;
				}
				else if(locChargedTrack->Get_Hypothesis(PiPlus) != NULL)
				{
					if(dDebugLevel > 20)
						cout << "Proton candidate: pi+ momentum = " << locChargedTrack->Get_Hypothesis(PiPlus)->momentum().Mag() << endl;
					if(locChargedTrack->Get_Hypothesis(PiPlus)->momentum().Mag() < locMinProtonMomentum.second)
						locTrackMomentumTooLowFlag = true;
				}
				else if(locChargedTrack->Get_Hypothesis(KMinus) != NULL)
				{
					if(dDebugLevel > 20)
						cout << "Proton candidate: K- momentum = " << locChargedTrack->Get_Hypothesis(KMinus)->momentum().Mag() << endl;
					if(locChargedTrack->Get_Hypothesis(KMinus)->momentum().Mag() < locMinProtonMomentum.second)
						locTrackMomentumTooLowFlag = true;
				}
				else if(locChargedTrack->Get_Hypothesis(PiMinus) != NULL)
				{
					if(dDebugLevel > 20)
						cout << "Proton candidate: Pi- momentum = " << locChargedTrack->Get_Hypothesis(PiMinus)->momentum().Mag() << endl;
					if(locChargedTrack->Get_Hypothesis(PiMinus)->momentum().Mag() < locMinProtonMomentum.second)
						locTrackMomentumTooLowFlag = true;
				}
				else
				{
					if(dDebugLevel > 20)
						cout << "Proton candidate: Best FOM momentum = " << locChargedTrack->Get_BestFOM()->momentum().Mag() << endl;
					if(locChargedTrack->Get_BestFOM()->momentum().Mag() < locMinProtonMomentum.second)
						locTrackMomentumTooLowFlag = true;
				}
			}
		}
		if(locTrackMomentumTooLowFlag)
		{
			if(dDebugLevel > 20)
				cout << "Track momentum too low to be " << ParticleType(locAnalysisPID) << endl;
			continue; //probably reconstructed a low-momentum pion: can't possibly be a proton (would stop too soon)
		}

		return locObject;
	}
	while(locResumeAtIndex < int(locSourceObjects.size()));
	return NULL;
}

bool DParticleComboBlueprint_factory::Cut_TrackingFOM(const DReaction* locReaction, const DChargedTrackHypothesis* locChargedTrackHypothesis) const
{
	pair<bool, double> locMinIndividualTrackingFOM = dMinIndividualTrackingFOM.first ? dMinIndividualTrackingFOM : locReaction->Get_MinIndividualTrackingFOM();
	if(!locMinIndividualTrackingFOM.first)
		return true;
	double locFOM = TMath::Prob(locChargedTrackHypothesis->dChiSq_Track, locChargedTrackHypothesis->dNDF_Track);
	return ((locChargedTrackHypothesis->dNDF_Track == 0) ? true : (locFOM >= locMinIndividualTrackingFOM.second));
}

bool DParticleComboBlueprint_factory::Cut_PIDFOM(const DReaction* locReaction, const DChargedTrackHypothesis* locChargedTrackHypothesis) const
{
	pair<bool, double> locMinIndividualChargedPIDFOM = dMinIndividualChargedPIDFOM.first ? dMinIndividualChargedPIDFOM : locReaction->Get_MinIndividualChargedPIDFOM();
	if(!locMinIndividualChargedPIDFOM.first)
		return true;
	return ((locChargedTrackHypothesis->dNDF == 0) ? true : (locChargedTrackHypothesis->dFOM >= locMinIndividualChargedPIDFOM.second));
}

DParticleComboBlueprintStep* DParticleComboBlueprint_factory::Get_ParticleComboBlueprintStepResource(void)
{
	DParticleComboBlueprintStep* locParticleComboBlueprintStep;
	if(dParticleComboBlueprintStepPool_Available.empty())
	{
		locParticleComboBlueprintStep = new DParticleComboBlueprintStep;
		dParticleComboBlueprintStepPool_All.push_back(locParticleComboBlueprintStep);
	}
	else
	{
		locParticleComboBlueprintStep = dParticleComboBlueprintStepPool_Available.back();
		locParticleComboBlueprintStep->Reset();
		dParticleComboBlueprintStepPool_Available.pop_back();
	}
	return locParticleComboBlueprintStep;
}

DParticleComboBlueprint* DParticleComboBlueprint_factory::Clone_ParticleComboBlueprint(const DParticleComboBlueprint* locParticleComboBlueprint)
{
	DParticleComboBlueprint* locNewParticleComboBlueprint = new DParticleComboBlueprint();
	*locNewParticleComboBlueprint = *locParticleComboBlueprint;
	return locNewParticleComboBlueprint;
}

void DParticleComboBlueprint_factory::Reset_Pools(void)
{
	// delete pool sizes if too large, preventing memory-leakage-like behavor.
	if(dParticleComboBlueprintStepPool_All.size() > MAX_DParticleComboBlueprintStepPoolSize){
		for(size_t loc_i = MAX_DParticleComboBlueprintStepPoolSize; loc_i < dParticleComboBlueprintStepPool_All.size(); ++loc_i)
			delete dParticleComboBlueprintStepPool_All[loc_i];
		dParticleComboBlueprintStepPool_All.resize(MAX_DParticleComboBlueprintStepPoolSize);
	}
	dParticleComboBlueprintStepPool_Available = dParticleComboBlueprintStepPool_All;
}

//------------------
// erun
//------------------
jerror_t DParticleComboBlueprint_factory::erun(void)
{
	return NOERROR;
}

//------------------
// fini
//------------------
jerror_t DParticleComboBlueprint_factory::fini(void)
{
	for(size_t loc_i = 0; loc_i < dParticleComboBlueprintStepPool_All.size(); ++loc_i)
		delete dParticleComboBlueprintStepPool_All[loc_i];

	return NOERROR;
}

