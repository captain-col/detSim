#include "DSimKinemPassThrough.hh"
#include "DSimRootPersistencyManager.hh"

#include <TROOT.h>
#include <TList.h>

#include "DSimLog.hh"

#include <memory>
#include <cstdlib>

#define PASSTHRUDIR "DetSimPassThru"

DSimKinemPassThrough* DSimKinemPassThrough::fKinemPassThrough = NULL;

DSimKinemPassThrough::~DSimKinemPassThrough() {
    CleanUp();
}

DSimKinemPassThrough::DSimKinemPassThrough() {
    Init();
    DSimNamedDebug("PassThru",
                    "Have called the DSimKinemPassThrough constructor."); 
}

DSimKinemPassThrough * DSimKinemPassThrough::GetInstance() {
    DSimNamedTrace("PassThru",
                    "Get pointer to DSimKinemPassThrough instance."); 
    if (fKinemPassThrough) return fKinemPassThrough;
    fKinemPassThrough = new DSimKinemPassThrough();
    if (!fKinemPassThrough) std::abort();
    return fKinemPassThrough;
}

bool DSimKinemPassThrough::AddInputTree(const TTree * inputTreePtr,
                                         const char * inputFileName, 
                                         const char* generatorName) {
    DSimNamedDebug("PassThru",
                    "Adding a generator mc-truth input"
                    " tree to the list of input trees that can be used."); 

    CreateInternalTrees();
  
    if (inputTreePtr == NULL) {
        DSimError("NULL input tree pointer.  TTree not saved.");  
        return false;
    }
  
    std::string inputTreeName(inputTreePtr->GetName()); 
    // check that all input trees have the same name. In future may add  
    // functionality so that maintain two persistent trees simultaneously 
    if (!fInputTreeChain) {
        fFirstTreeName = inputTreeName;
        fInputTreeChain = new TChain(fFirstTreeName.c_str());
    }
  
    if (fFirstTreeName != inputTreeName) {
        DSimError("Input tree name not compatible: "
                   "  All pass-through trees must have same name.");
        return false;
    }

    // check if its already been added
    if (fInputTreeMap.find(inputTreePtr) != fInputTreeMap.end()) {
        DSimError("Input tree already in chain.");
        return false;
    }
  
    //  Add input tree to TChain
    fInputTreeChain->Add(inputFileName);

    // Clone tree if this has not been done already. We check if fPersistent
    // tree is NULL rather than looking to see if fInputTreeChain has a list
    // of clones (as for a TChain this always returns false). 
    if (fPersistentTree == NULL) {
        DSimNamedDebug("PassThru", "Clone the input TTree");
        fPersistentTree = (TTree*) fInputTreeChain->CloneTree(0);
    }

    // Add the input file to the file list so it can be saved in the output
    // tree.  This is used for error reporting and debugging. 
    fFileList.push_back(SetInputFileName(inputFileName));

    // fill in input tree maps
    fInputTreeMap[inputTreePtr]  = fFileList.size() - 1;
    fFirstEntryMap[inputTreePtr] = (fInputTreeChain->GetEntries() 
                                    - inputTreePtr->GetEntries());
    
    // Copy the input file name and make sure it's 0 terminated.
    std::strncpy(fInputFileName, fFileList.back().c_str(),
                 sizeof(fInputFileName));
    fInputFileName[sizeof(fInputFileName)-1] = 0;
    // Copy the generator name and make sure it's 0 terminated.
    std::strncpy(fInputFileGenerator, generatorName, 
                 sizeof(fInputFileGenerator));
    // Copy the generator name and make sure it's 0 terminated.
    std::strncpy(fInputFileTreeName, inputTreeName.c_str(), 
                 sizeof(fInputFileTreeName));
    fInputFileTreeName[sizeof(fInputFileTreeName)-1] = 0;
    fInputFilePOT = inputTreePtr->GetWeight();
    fInputFileEntries = inputTreePtr->GetEntries();
    fInputFilesTree->Fill();

    DSimNamedDebug("PassThru", 
                    "Have added a " << fFirstTreeName 
                    << " tree from the input file "<< inputFileName); 
    
    // Now can start copying events
    return true;
}

void DSimKinemPassThrough::CreateInternalTrees() {
    TFile* output = DSimRootPersistencyManager::GetInstance()->GetTFile();
    
    output->cd();

    // Check if the directory exists (and possibly create it).
    if (!output->Get(PASSTHRUDIR)) {
        output->mkdir(PASSTHRUDIR,"DETSIM Pass-Through Information");
    }

    // Make sure we are in the pass-thru directory.
    output->cd(PASSTHRUDIR);

    // Create the book keeping three that connects a particular entry to the
    // entry in the original file.
    if (fInputKinemTree == NULL) {
        DSimNamedDebug("PassThru", "Create InputKinem TTree");
        // This adds the tree to the current output file.
        fInputKinemTree = new TTree("InputKinem",
                                     "Map kinematics with input files");
        fInputKinemTree->Branch("inputFileNum" , &fInputFileNumber); 
        fInputKinemTree->Branch("inputEntryNum", &fOrigEntryNumber);
    }

    if (fInputFilesTree == NULL) {
        DSimNamedDebug("PassThru","Create inputFileList TTree");
        // This adds the tree to the current output file.
        fInputFilesTree = new TTree("InputFiles","Input file information");
        fInputFilesTree->Branch("fileName", &fInputFileName,
                                "fileName/C");
        fInputFilesTree->Branch("generatorName", &fInputFileGenerator, 
                                "generatorName/C");
        fInputFilesTree->Branch("treeName", &fInputFileTreeName, 
                                "treeName/C");
        fInputFilesTree->Branch("filePOT", &fInputFilePOT);
        fInputFilesTree->Branch("fileEntries", &fInputFileEntries);
    }
}

bool 
DSimKinemPassThrough::AddEntry(const TTree* inputTree, int origEntry) {
    if (!fPersistentTree) {       
        DSimNamedDebug("PassThru", "Cannot copy entry from tree "
                        "since  fPersistentTree is NULL."); 
        return false;
    }

    CreateInternalTrees();

    // Search the input tree maps for a TTree pointer that matches the
    // inputTree.
    TreeToInt::iterator treeid_iter = fInputTreeMap.find(inputTree);
    TreeToInt::iterator firstentry_iter = fFirstEntryMap.find(inputTree);
    if (treeid_iter == fInputTreeMap.end() 
        || firstentry_iter == fFirstEntryMap.end()) {
        DSimError("Cannot copy entry from tree not in list of input trees.");
        return false;
    }
  
    fInputFileNumber = treeid_iter->second;
    int first_event_in_chain = firstentry_iter->second;
  
    // Now fill temp tree with i'th entry from first_event_in_chain of TChain
    // of input trees.
    if (fInputTreeChain->GetEntry(origEntry+first_event_in_chain)) {
        fOrigEntryNumber = origEntry;
        fPersistentTree->Fill();
        fInputKinemTree->Fill(); // Also store book keeping info.
    
        DSimNamedTrace("PassThru", 
                        "Copied entry " << origEntry
                        << " from " << fFirstTreeName
                        <<  " tree in file "
                        << fFileList[fInputFileNumber]); 
    
        return true;
    }

    DSimError("Cannot copy entry " << origEntry+first_event_in_chain 
               << " from TChained Tree.  Make sure entry is in input tree!"); 
    return false;
}

int DSimKinemPassThrough::LastEntryNumber() {
    // The most recent entry number is the sum of the entry number in
    // the current tree plus the number of entries already stored in 
    // persistent tree.
    if (!fPersistentTree) {
        DSimError("No entries in fPersistent tree.");
    }
    return fPersistentTree->GetEntries() - 1;
}

std::string DSimKinemPassThrough::SetInputFileName(std::string name) {
    // Remove the path from the input file name. 
    std::string::size_type start_pos = name.rfind("/");
    if (start_pos == std::string::npos) start_pos = 0; else ++start_pos;
    std::string baseName(name,start_pos);

    return baseName;
}

void DSimKinemPassThrough::CleanUp() {
    // Delete the output trees so that they can not be written twice (which
    // they would be as detsim calls TFile::Write() which writes anything in
    // memory to the TFile.
  
    if (fPersistentTree) {delete fPersistentTree;}
    if (fInputFilesTree) {delete fInputFilesTree;}
    if (fInputKinemTree) {delete fInputKinemTree;}
    if (fInputTreeChain) {delete fInputTreeChain;}

    Init();

    return;
}

void DSimKinemPassThrough::Init() {
    fPersistentTree = NULL; 
    fInputFilesTree = NULL;
    fInputKinemTree = NULL;
    fInputTreeChain = NULL;
    fFirstTreeName.clear();
    fFileList.clear();
    fInputFileNumber = -1;
    fOrigEntryNumber = -1;
    fInputFileName[0] = 0;
    fInputFileGenerator[0] = 0;
    fInputFileTreeName[0] = 0;
    return;
}
