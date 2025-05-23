#include "../src/CutManager.C"
#include "../src/CLAS12Ana.C"
#include "../src/TreeManager.C"
#include "../src/HipoBankInterface.C"
#include "../src/Constants.h"
#include "../src/Structs.h"
#include "../src/Kinematics.C"

int hipo2tree_pi0(
    const char* hipoFile = "",
    const char* outputFile = "",
    const int maxEvents = 100
) {
    // Create a TFile to save the data
    TFile* fOut = new TFile(outputFile, "RECREATE");

    // Create a TTree to store the data
    EventTree* treeReco = new EventTree("EventTree");
    // Configure CLAS12 Reader and HipoChain
    // -------------------------------------
    clas12root::HipoChain _chain;
    clas12::clas12reader* _config_c12{ nullptr };

    _chain.Add(hipoFile);
    _config_c12 = _chain.GetC12Reader();

    _config_c12->addAtLeastPid(11, 1);
    _config_c12->addAtLeastPid(22, 2);

    // Establish CLAS12 event parser
    // -------------------------------------
    auto& _c12 = _chain.C12ref();
    // Check if "rg-a" is present in the hipoFile path
    bool doQADB = false;
    std::string hipoFilePath = hipoFile;
    if (hipoFilePath.find("rg-a") != std::string::npos &&
        hipoFilePath.find("montecarlo") == std::string::npos) {
        doQADB = true;
    }

    if (doQADB) {
        _config_c12->applyQA("pass1");
        _config_c12->db()->qadb_addQARequirement("OkForAsymmetry");
    }

    // Add Analysis Objects
    // -------------------------------------
    CutManager _cm = CutManager();
    CLAS12Ana clas12ana = CLAS12Ana(_c12);
    clas12ana.set_run_config(_c12);

    // Add Analysis Structs
    // -------------------------------------
    std::vector<part> vec_particles;
    std::vector<part> vec_mcparticles;
    EVENT_INFO event_info;
    EVENT event;

    int whileidx = 0;
    int _ievent = 0;
    int badAsym = 0;

    while (_chain.Next() == true && (whileidx < maxEvents || maxEvents < 0)) {
        if (whileidx % 10000 == 0 && whileidx != 0) {
            std::cout << whileidx << " events read | "
                      << _ievent * 100.0 / whileidx << "% passed event selection | "
                      << badAsym << " events skipped from QADB"
                      << std::endl;
        }
        clas12ana.get_event_info(_c12, event_info);
        event_info.uID = whileidx;
        whileidx++;

        // Set run specific information
        // -------------------------------------
        _cm.set_run(event_info.run);
        // if (doQADB == true) {
        //     if (!_c12->db()->qa()->isOkForAsymmetry(event_info.run, event_info.evnum)) {
        //         badAsym++;
        //         continue;
        //     }
        // }
        // *******************************************************************
        //     Reconstructed Particles
        //

        vec_particles = clas12ana.load_reco_particles(_c12);
        int idx_scattered_ele = clas12ana.find_reco_scattered_electron(vec_particles);
        if (idx_scattered_ele == -1)
            continue; // No scattered electron found
        vec_particles[idx_scattered_ele].is_scattered_electron = 1;
        clas12ana.fill_reco_event_variables(event, vec_particles);
        if (event.y > 0.8 || event.Q2 < 1)
            continue; // Maximum y cut
        vec_particles = _cm.filter_particles(vec_particles); // Apply Cuts

        int num_e = 0;
        int num_piplus = 0;
        int num_piminus = 0;
        int num_gamma = 0;
        int num_proton = 0;
        for (auto part : vec_particles) {
            if (part.pid == 11) num_e++;
            if (part.pid == 211) num_piplus++;
            if (part.pid == -211) num_piminus++;
            if (part.pid == 22) num_gamma++;
            if (part.pid == 2212) num_proton++;
        }
        if (num_e == 0 || num_gamma < 2) continue;
        // 
        //
        // *******************************************************************
        treeReco->FillTree(vec_particles, event, event_info);
        _ievent++;
    }

    fOut->cd();
    treeReco->Write();
    fOut->Close();

    // Now put together the photon-tree
    
    const char* input_file = outputFile;

    //Define the variables "m_g" , "m_ch" , "m_nh"
    //Should not be changed because the model was trained with this specific set of inputs
    int m_g = 3;  // Number of neighboring gammas
    int m_ch = 2; // Number of neighboring charged hadrons
    int m_nh = 2; // Number of neighboring neutral hadrons

    //Read the TFile
    TFile* f = new TFile(input_file, "UPDATE");

    //Read the TTree
    TTree* EventTree = (TTree*)f->Get("EventTree");
    TString treename = "MLinput";

    //If MLInput tree already exists, remove it
    if (f->Get(treename)) f->Delete("MLinput*;*");

    TTree* MLInput = new TTree(treename, "Nearest neighbor information");

    //Define the branches in MLInput
    int photon_has_match = 0;
    double gE = 0;
    double gEpcal = 0;
    double gTheta = 0;
    double gm2u = 0;
    double gm2v = 0;
    double R_e;
    double dE_e;

    double R_gamma[m_g], dE_gamma[m_g], Epcal_gamma[m_g], m2u_gamma[m_g], m2v_gamma[m_g];
    double R_ch[m_ch], dE_ch[m_ch], Epcal_ch[m_ch], m2u_ch[m_ch], m2v_ch[m_ch];
    double R_nh[m_nh], dE_nh[m_nh], Epcal_nh[m_nh], m2u_nh[m_nh], m2v_nh[m_nh];
    double num_photons_0_1, num_photons_0_2, num_photons_0_35;

    MLInput->Branch("photon_has_match", &photon_has_match, "photon_has_match/I");
    MLInput->Branch("m_g", &m_g, "m_g/I");
    MLInput->Branch("m_ch", &m_ch, "m_ch/I");
    MLInput->Branch("m_nh", &m_nh, "m_nh/I");

    MLInput->Branch("gE", &gE, "gE/D");
    MLInput->Branch("gEpcal", &gEpcal, "gEpcal/D");
    MLInput->Branch("gTheta", &gTheta, "gTheta/D");
    MLInput->Branch("gm2u", &gm2u, "gm2u/D");
    MLInput->Branch("gm2v", &gm2v, "gm2v/D");

    MLInput->Branch("R_e", &R_e, "R_e/D");
    MLInput->Branch("dE_e", &dE_e, "dE_e/D");

    MLInput->Branch("R_gamma", R_gamma, "R_gamma[m_g]/D");
    MLInput->Branch("dE_gamma", dE_gamma, "dE_gamma[m_g]/D");
    MLInput->Branch("Epcal_gamma", Epcal_gamma, "Epcal_gamma[m_g]/D");
    MLInput->Branch("m2u_gamma", m2u_gamma, "m2u_gamma[m_g]/D");
    MLInput->Branch("m2v_gamma", m2v_gamma, "m2v_gamma[m_g]/D");

    MLInput->Branch("R_ch", R_ch, "R_ch[m_ch]/D");
    MLInput->Branch("dE_ch", dE_ch, "dE_ch[m_ch]/D");
    MLInput->Branch("Epcal_ch", Epcal_ch, "Epcal_ch[m_ch]/D");
    MLInput->Branch("m2u_ch", m2u_ch, "m2u_ch[m_ch]/D");
    MLInput->Branch("m2v_ch", m2v_ch, "m2v_ch[m_ch]/D");

    MLInput->Branch("R_nh", R_nh, "R_nh[m_nh]/D");
    MLInput->Branch("dE_nh", dE_nh, "dE_nh[m_nh]/D");
    MLInput->Branch("Epcal_nh", Epcal_nh, "Epcal_nh[m_nh]/D");
    MLInput->Branch("m2u_nh", m2u_nh, "m2u_nh[m_nh]/D");
    MLInput->Branch("m2v_nh", m2v_nh, "m2v_nh[m_nh]/D");

    MLInput->Branch("num_photons_0_1", &num_photons_0_1, "num_photons_0_1/D");
    MLInput->Branch("num_photons_0_2", &num_photons_0_2, "num_photons_0_2/D");
    MLInput->Branch("num_photons_0_35", &num_photons_0_35, "num_photons_0_35/D");

    // Variables to read from EventTree
    const int kNmax = 500;
    int Nmax;
    double E[kNmax], th[kNmax], phi[kNmax], pcal_e[kNmax], pcal_m2u[kNmax], pcal_m2v[kNmax];
    int pid[kNmax], truepid[kNmax];
    double pcal_x[kNmax], pcal_y[kNmax], pcal_z[kNmax];
    double ecin_x[kNmax], ecin_y[kNmax], ecin_z[kNmax];
    double ecout_x[kNmax], ecout_y[kNmax], ecout_z[kNmax];

    EventTree->SetBranchAddress("Nmax", &Nmax);
    EventTree->SetBranchAddress("E", E);
    EventTree->SetBranchAddress("theta", th);
    EventTree->SetBranchAddress("phi", phi);
    EventTree->SetBranchAddress("pid", pid);
    EventTree->SetBranchAddress("truepid", truepid);
    EventTree->SetBranchAddress("pcal_x", pcal_x);
    EventTree->SetBranchAddress("pcal_y", pcal_y);
    EventTree->SetBranchAddress("pcal_z", pcal_z);
    EventTree->SetBranchAddress("ecin_x", ecin_x);
    EventTree->SetBranchAddress("ecin_y", ecin_y);
    EventTree->SetBranchAddress("ecin_z", ecin_z);
    EventTree->SetBranchAddress("ecout_x", ecout_x);
    EventTree->SetBranchAddress("ecout_y", ecout_y);
    EventTree->SetBranchAddress("ecout_z", ecout_z);
    EventTree->SetBranchAddress("pcal_e", pcal_e);
    EventTree->SetBranchAddress("pcal_m2u", pcal_m2u);
    EventTree->SetBranchAddress("pcal_m2v", pcal_m2v);

    // Loop over the events in EventTree
    for (int iEvent = 0; iEvent < EventTree->GetEntries(); ++iEvent) {
        EventTree->GetEntry(iEvent);
        for (int ipart = 0; ipart < Nmax; ++ipart) {
            if (pid[ipart] != 22) continue;

            R_e = 0; dE_e = 0;
            for (int i = 0; i < m_g; ++i) {
                R_gamma[i] = dE_gamma[i] = Epcal_gamma[i] = m2u_gamma[i] = m2v_gamma[i] = 0;
            }
            for (int i = 0; i < m_ch; ++i) {
                R_ch[i] = dE_ch[i] = Epcal_ch[i] = m2u_ch[i] = m2v_ch[i] = 0;
            }
            for (int i = 0; i < m_nh; ++i) {
                R_nh[i] = dE_nh[i] = Epcal_nh[i] = m2u_nh[i] = m2v_nh[i] = 0;
            }

            num_photons_0_1 = num_photons_0_2 = num_photons_0_35 = 0;

            // Set vars
            gE       = E[ipart];
            gEpcal   = pcal_e[ipart];
            gTheta   = th[ipart];
            gm2u     = pcal_m2u[ipart];
            gm2v     = pcal_m2v[ipart];
            photon_has_match = (truepid[ipart] == 22);

            // Find neighbors
            for (int jpart = 0; jpart < Nmax; ++jpart) {
                if (jpart == ipart) continue;

                double x1, y1, z1, x2, y2, z2;
                if (pcal_x[ipart] == -999) {
                    if (ecin_x[ipart] == -999) {
                        x1 = ecout_x[ipart]; y1 = ecout_y[ipart]; z1 = ecout_z[ipart];
                    } else {
                        x1 = ecin_x[ipart]; y1 = ecin_y[ipart]; z1 = ecin_z[ipart];
                    }
                } else {
                    x1 = pcal_x[ipart]; y1 = pcal_y[ipart]; z1 = pcal_z[ipart];
                }
                if (pcal_x[jpart] == -999) {
                    if (ecin_x[jpart] == -999) {
                        x2 = ecout_x[jpart]; y2 = ecout_y[jpart]; z2 = ecout_z[jpart];
                    } else {
                        x2 = ecin_x[jpart]; y2 = ecin_y[jpart]; z2 = ecin_z[jpart];
                    }
                } else {
                    x2 = pcal_x[jpart]; y2 = pcal_y[jpart]; z2 = pcal_z[jpart];
                }

                TVector3 v1(x1, y1, z1), v2(x2, y2, z2);
                float R = v1.Angle(v2);

                if (pid[jpart] == 22) {
                    if (R < 0.1) num_photons_0_1++;
                    if (R < 0.2) num_photons_0_2++;
                    if (R < 0.35) num_photons_0_35++;
                    for (int i = 0; i < m_g; ++i) {
                        if (R < R_gamma[i] || R_gamma[i] == 0) {
                            for (int j = m_g - 1; j > i; --j) {
                                R_gamma[j] = R_gamma[j - 1];
                                dE_gamma[j] = dE_gamma[j - 1];
                                Epcal_gamma[j] = Epcal_gamma[j - 1];
                                m2u_gamma[j] = m2u_gamma[j - 1];
                                m2v_gamma[j] = m2v_gamma[j - 1];
                            }
                            R_gamma[i] = R;
                            dE_gamma[i] = E[ipart] - E[jpart];
                            Epcal_gamma[i] = pcal_e[jpart];
                            m2u_gamma[i] = pcal_m2u[jpart];
                            m2v_gamma[i] = pcal_m2v[jpart];
                            break;
                        }
                    }
                }
                else if (pid[jpart] == 211 || pid[jpart] == -211 || pid[jpart] == 2212 || pid[jpart] == -2212 || pid[jpart] == 321 || pid[jpart] == -321) {
                    for (int i = 0; i < m_ch; ++i) {
                        if (R < R_ch[i] || R_ch[i] == 0) {
                            for (int j = m_ch - 1; j > i; --j) {
                                R_ch[j] = R_ch[j - 1];
                                dE_ch[j] = dE_ch[j - 1];
                                Epcal_ch[j] = Epcal_ch[j - 1];
                                m2u_ch[j] = m2u_ch[j - 1];
                                m2v_ch[j] = m2v_ch[j - 1];
                            }
                            R_ch[i] = R;
                            dE_ch[i] = E[ipart] - E[jpart];
                            Epcal_ch[i] = pcal_e[jpart];
                            m2u_ch[i] = pcal_m2u[jpart];
                            m2v_ch[i] = pcal_m2v[jpart];
                            break;
                        }
                    }
                }
                else if (pid[jpart] == 2112 || pid[jpart] == -2112) {
                    for (int i = 0; i < m_nh; ++i) {
                        if (R < R_nh[i] || R_nh[i] == 0) {
                            for (int j = m_nh - 1; j > i; --j) {
                                R_nh[j] = R_nh[j - 1];
                                dE_nh[j] = dE_nh[j - 1];
                                Epcal_nh[j] = Epcal_nh[j - 1];
                                m2u_nh[j] = m2u_nh[j - 1];
                                m2v_nh[j] = m2v_nh[j - 1];
                            }
                            R_nh[i] = R;
                            dE_nh[i] = E[ipart] - E[jpart];
                            Epcal_nh[i] = pcal_e[jpart];
                            m2u_nh[i] = pcal_m2u[jpart];
                            m2v_nh[i] = pcal_m2v[jpart];
                            break;
                        }
                    }
                }
                else if (pid[jpart] == 11) {
                    if (R < R_e || R_e == 0) {
                        R_e = R;
                        dE_e = E[ipart] - E[jpart];
                    }
                }
            }

            //Fill the MLInput TTree for each photon found
            MLInput->Fill();
        }
    }

    //Write the MLInput TTree to disk
    MLInput->Write();

    //Close the TFile
    f->Close();
    return 0;
}
