#include "../src/CutManager.C"
#include "../src/CLAS12Ana.C"
#include "../src/TreeManager.C"
#include "../src/HipoBankInterface.C"
#include "../src/Constants.h"
#include "../src/Structs.h"
#include "../src/Kinematics.C"

int hipo2tree_pippim(
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
    _config_c12->addAtLeastPid(211, 1);
    _config_c12->addAtLeastPid(-211, 1);
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
        if (num_e == 0 || num_piplus < 1 || num_piminus < 1) continue;
        // 
        //
        // *******************************************************************
        treeReco->FillTree(vec_particles, event, event_info);
        _ievent++;
    }

    fOut->cd();
    treeReco->Write();
    fOut->Close();
    return 0;
}
