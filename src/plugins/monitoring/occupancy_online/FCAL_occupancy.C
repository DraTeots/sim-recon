//
// Guidance: --------------------------------------------
//
// Single channels may show low or no number of entries in occupancy plots
// during normal operation. This could number anywhere from 0 to ~20
// channels, with slow time dependency (0-3 channels/day). These channels are
// expected to be randomly spread around the FCAL, and may or may not also
// line up with yellow/red status on the FCAL HV GUI. Any behavior other than
// what's described here should be reported to FCAL experts.
//
// End Guidance: ----------------------------------------
//
//
//
// The following are special comments used by RootSpy to know
// which histograms to fetch for the macro.
//
// hnamepath: /occupancy/fcal_occ
// hnamepath: /occupancy/fcal_num_events
//
// e-mail: davidl@jlab.org
// e-mail: adesh@jlab.org
// e-mail: jzarling@jlab.org
//

{
	// RootSpy saves the current directory and style before
	// calling the macro and restores it after so it is OK to
	// change them and not change them back.
        TDirectory *savedir = gDirectory;
	TDirectory *dir = (TDirectory*)gDirectory->FindObjectAny("occupancy");
	if(dir) dir->cd();

	TH2I *fcal_occ = (TH2I*)gDirectory->FindObjectAny("fcal_occ");
	TH1I *fcal_num_events = (TH1I*)gDirectory->FindObjectAny("fcal_num_events");

	double Nevents = 1.0;
	if(fcal_num_events) Nevents = (double)fcal_num_events->GetBinContent(1);

	TLegend *legend_sa = new TLegend(0.1,0.85,0.3,0.9);
	if(fcal_occ)legend_sa->AddEntry(fcal_occ, "fADC","f");

	// Just for testing
	if(gPad == NULL){
		TCanvas *c1 = new TCanvas("c1");
		c1->cd(0);
		c1->Draw();
		c1->Update();
	}
	if(!gPad) {savedir->cd(); return;}

	TCanvas *c1 = gPad->GetCanvas();
	c1->cd(0);
	c1->Clear();

	gPad->SetTicks();
	gPad->SetGrid();
	gPad->SetRightMargin(2.0);
	gPad->SetLeftMargin(2.0);
	if(fcal_occ){
		fcal_occ->SetStats(0);
		TH1* h = fcal_occ->DrawCopy("colz");
		h->Scale(1./Nevents);
		h->GetZaxis()->SetRangeUser(0.0001, 0.25);
		
		char str[256];
		sprintf(str,"%0.0f events", Nevents);
		TLatex lat(30.0, 61.75, str);
		lat.SetTextAlign(22);
		lat.SetTextSize(0.035);
		lat.Draw();
	}

	legend_sa->Draw();

}
