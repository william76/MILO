/***********************************************************************
 Multiscale/Multiphysics Interfaces for Large-scale Optimization (MILO)
 
 Copyright 2018 National Technology & Engineering Solutions of Sandia,
 LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the
 U.S. Government retains certain rights in this software.”
 
 Questions? Contact Tim Wildey (tmwilde@sandia.gov) and/or
 Bart van Bloemen Waanders (bartv@sandia.gov)
 ************************************************************************/

#include "postprocessInterface.hpp"

// ========================================================================================
/* Constructor to set up the problem */
// ========================================================================================

postprocess::postprocess(const Teuchos::RCP<Epetra_MpiComm> & Comm_,
                         Teuchos::RCP<Teuchos::ParameterList> & settings,
                         Teuchos::RCP<panzer_stk::STK_Interface> & mesh_,
                         Teuchos::RCP<discretization> & disc_, Teuchos::RCP<physics> & phys_,
                         Teuchos::RCP<solver> & solve_, Teuchos::RCP<panzer::DOFManager<int,int> > & DOF_,
                         vector<vector<Teuchos::RCP<cell> > > cells_,
                         Teuchos::RCP<FunctionInterface> & functionManager) :
Comm(Comm_), mesh(mesh_), disc(disc_), phys(phys_), solve(solve_),
DOF(DOF_), cells(cells_) {
  
  verbosity = settings->sublist("Postprocess").get<int>("Verbosity",1);
  
  E_overlapped_map = solve->LA_overlapped_map;
  param_overlapped_map = solve->param_overlapped_map;
  mesh->getElementBlockNames(blocknames);
  
  numNodesPerElem = settings->sublist("Mesh").get<int>("numNodesPerElem",4);
  spaceDim = settings->sublist("Mesh").get<int>("dim",2);
  numVars = phys->numVars; //
  
  response_type = settings->sublist("Postprocess").get("response type", "pointwise"); // or "global"
  have_sensor_data = settings->sublist("Analysis").get("Have Sensor Data", false); // or "global"
  save_sensor_data = settings->sublist("Analysis").get("Save Sensor Data",false);
  sname = settings->sublist("Analysis").get("Sensor Prefix","sensor");
  stddev = settings->sublist("Analysis").get("Additive Normal Noise Standard Dev",0.0);
  write_dakota_output = settings->sublist("Postprocess").get("Write Dakota Output",false);
  
  use_sol_mod_mesh = settings->sublist("Postprocess").get<bool>("Solution Based Mesh Mod",false);
  sol_to_mod_mesh = settings->sublist("Postprocess").get<int>("Solution For Mesh Mod",0);
  meshmod_TOL = settings->sublist("Postprocess").get<double>("Solution Based Mesh Mod TOL",1.0);
  layer_size = settings->sublist("Postprocess").get<double>("Solution Based Mesh Mod Layer Thickness",0.1);
  compute_subgrid_error = settings->sublist("Postprocess").get<bool>("Subgrid Error",false);
  error_type = settings->sublist("Postprocess").get<string>("Error type","L2"); // or "H1"
  use_sol_mod_height = settings->sublist("Postprocess").get<bool>("Solution Based Height Mod",false);
  sol_to_mod_height = settings->sublist("Postprocess").get<int>("Solution For Height Mod",0);
  
  have_subgrids = false;
  if (settings->isSublist("Subgrid"))
  have_subgrids = true;
  
  isTD = false;
  if (settings->sublist("Solver").get<string>("solver","steady-state") == "transient") {
    isTD = true;
  }
  plot_response = settings->sublist("Postprocess").get<bool>("Plot Response",false);
  save_height_file = settings->sublist("Postprocess").get("Save Height File",false);
  
  vector<vector<int> > cards = disc->cards;
  vector<vector<string> > phys_varlist = phys->varlist;
  
  //offsets = phys->offsets;
  
  for (size_t b=0; b<blocknames.size(); b++) {
    
    vector<int> curruseBasis(numVars[b]);
    vector<int> currnumBasis(numVars[b]);
    vector<string> currvarlist(numVars[b]);
    
    int currmaxbasis = 0;
    for (int j=0; j<numVars[b]; j++) {
      string var = phys_varlist[b][j];
      int vnum = DOF->getFieldNum(var);
      int vub = phys->getUniqueIndex(b,var);
      //currvarlist[vnum] = var;
      //curruseBasis[vnum] = vub;
      //currnumBasis[vnum] = cards[b][vub];
      currvarlist[j] = var;
      curruseBasis[j] = vub;
      currnumBasis[j] = cards[b][vub];
      currmaxbasis = std::max(currmaxbasis,cards[b][vub]);
    }
    
    //phys->setVars(currvarlist);
    
    varlist.push_back(currvarlist);
    useBasis.push_back(curruseBasis);
    numBasis.push_back(currnumBasis);
    maxbasis.push_back(currmaxbasis);
  
  
    int numElemPerCell = settings->sublist("Solver").get<int>("Workset size",1);
    int numip = disc->ref_ip[0].dimension(0);
    
    if (settings->sublist("Postprocess").isSublist("Responses")) {
      Teuchos::ParameterList resps = settings->sublist("Postprocess").sublist("Responses");
      Teuchos::ParameterList::ConstIterator rsp_itr = resps.begin();
      while (rsp_itr != resps.end()) {
        string entry = resps.get<string>(rsp_itr->first);
        functionManager->addFunction(rsp_itr->first,entry,numElemPerCell,numip,"ip",b);
        rsp_itr++;
      }
    }
    if (settings->sublist("Postprocess").isSublist("Weights")) {
      Teuchos::ParameterList wts = settings->sublist("Postprocess").sublist("Weights");
      Teuchos::ParameterList::ConstIterator wts_itr = wts.begin();
      while (wts_itr != wts.end()) {
        string entry = wts.get<string>(wts_itr->first);
        functionManager->addFunction(wts_itr->first,entry,numElemPerCell,numip,"ip",b);
        wts_itr++;
      }
    }
    if (settings->sublist("Postprocess").isSublist("Targets")) {
      Teuchos::ParameterList tgts = settings->sublist("Postprocess").sublist("Targets");
      Teuchos::ParameterList::ConstIterator tgt_itr = tgts.begin();
      while (tgt_itr != tgts.end()) {
        string entry = tgts.get<string>(tgt_itr->first);
        functionManager->addFunction(tgt_itr->first,entry,numElemPerCell,numip,"ip",b);
        tgt_itr++;
      }
    }
  }
  
  wkset = solve->wkset;
  
}

// ========================================================================================
// ========================================================================================

void postprocess::computeError(const vector_RCP & F_soln) {
  
  
  if(Comm->MyPID() == 0) {
    cout << endl << "*********************************************************" << endl;
    cout << "***** Performing verification ******" << endl << endl;
  }
  
  int numSteps = F_soln->NumVectors();
  vector<double> solvetimes = solve->solvetimes;
  
  
  for (size_t b=0; b<cells.size(); b++) {
    Kokkos::View<double**,AssemblyDevice> localerror("error",numSteps,numVars[b]);
    for (size_t t=0; t<solvetimes.size(); t++) {
      solve->performGather(b,F_soln,0,t);
      for (size_t e=0; e<cells[b].size(); e++) {
        int numElem = cells[b][e]->numElem;
        Kokkos::View<double**,AssemblyDevice> localerrs = cells[b][e]->computeError(solvetimes[t], t, compute_subgrid_error, error_type);
        
        for (int p=0; p<numElem; p++) {
          for (int n=0; n<numVars[b]; n++) {
            localerror(t,n) += localerrs(p,n);
          }
        }
      }
    }
    
    for (size_t t=0; t<solvetimes.size(); t++) {
      for (int n=0; n<numVars[b]; n++) {
        double lerr = localerror(t,n);
        double gerr = 0.0;
        Comm->SumAll(&lerr, &gerr, 1);
        if(Comm->MyPID() == 0) {
          cout << "***** " << error_type << " norm of the error for " << varlist[b][n] << " = " << sqrt(gerr) << "  (time = " << solvetimes[t] << ")" <<  endl;
        }
      }
    }
    
  }
}

// ========================================================================================
// ========================================================================================

AD postprocess::computeObjective(const vector_RCP & F_soln) {
  
  if(Comm->MyPID() == 0 ) {
    if (verbosity > 0) {
      cout << endl << "*********************************************************" << endl;
      cout << "***** Computing Objective Function ******" << endl << endl;
    }
  }
  
  AD totaldiff = 0.0;
  AD regDomain = 0.0;
  AD regBoundary = 0.0;
  //bvbw    AD classicParamPenalty = 0.0;
  vector<double> solvetimes = solve->solvetimes;
  vector<int> domainRegTypes = solve->domainRegTypes;
  vector<double> domainRegConstants = solve->domainRegConstants;
  vector<int> domainRegIndices = solve->domainRegIndices;
  int numDomainParams = domainRegIndices.size();
  vector<int> boundaryRegTypes = solve->boundaryRegTypes;
  vector<double> boundaryRegConstants = solve->boundaryRegConstants;
  vector<int> boundaryRegIndices = solve->boundaryRegIndices;
  int numBoundaryParams = boundaryRegIndices.size();
  vector<string> boundaryRegSides = solve->boundaryRegSides;
  
  
  int numSensors = 1;
  if (response_type == "pointwise") {
    numSensors = solve->numSensors;
  }
  solve->sacadoizeParams(true);
  int numClassicParams = solve->getNumParams(1);
  int numDiscParams = solve->getNumParams(4);
  int numParams = numClassicParams + numDiscParams;
  vector<double> regGradient(numParams);
  vector<double> dmGradient(numParams);
  vector_RCP P_soln = solve->Psol[0];
  
  //cout << solvetimes.size() << endl;
  //for (int i=0; i<solvetimes.size(); i++) {
  //  cout << solvetimes[i] << endl;
  //}
  for (size_t tt=0; tt<solvetimes.size(); tt++) {
    for (size_t b=0; b<cells.size(); b++) {
      
      solve->performGather(b,F_soln,0,tt);
      solve->performGather(b,P_soln,4,0);
      
      for (size_t e=0; e<cells[b].size(); e++) {
        //cout << e << endl;
        
        Kokkos::View<AD**,AssemblyDevice> obj = cells[b][e]->computeObjective(solvetimes[tt], tt, 0);
        
        int numElem = cells[b][e]->numElem;
        
        vector<vector<int> > paramoffsets = solve->paramoffsets;
        //for (size_t tt=0; tt<solvetimes.size(); tt++) { // skip initial condition in 0th position
        if (obj.dimension(1) > 0) {
          for (int c=0; c<numElem; c++) {
            for (size_t i=0; i<obj.dimension(1); i++) {
              totaldiff += obj(c,i);
              if (numClassicParams > 0) {
                if (obj(c,i).size() > 0) {
                  double val;
                  val = obj(c,i).fastAccessDx(0);
                  dmGradient[0] += val;
                }
              }
              if (numDiscParams > 0) {
                vector<int> paramGIDs = cells[b][e]->paramGIDs[c];
                
                for (int row=0; row<paramoffsets[0].size(); row++) {
                  int rowIndex = paramGIDs[paramoffsets[0][row]];
                  int poffset = paramoffsets[0][row];
                  double val;
                  if (obj(c,i).size() > numClassicParams) {
                    val = obj(c,i).fastAccessDx(poffset+numClassicParams);
                    dmGradient[rowIndex+numClassicParams] += val;
                  }
                }
              }
            }
          }
        }
        //}
        if ((numDomainParams > 0) || (numBoundaryParams > 0)) {
          for (int c=0; c<numElem; c++) {
            vector<int> paramGIDs = cells[b][e]->paramGIDs[c];
            vector<vector<int> > paramoffsets = solve->paramoffsets;
            
            if (numDomainParams > 0) {
              int paramIndex, rowIndex, poffset;
              double val;
              regDomain = cells[b][e]->computeDomainRegularization(domainRegConstants,
                                                                   domainRegTypes, domainRegIndices);
              
              for (size_t p = 0; p < numDomainParams; p++) {
                paramIndex = domainRegIndices[p];
                for( size_t row=0; row<paramoffsets[paramIndex].size(); row++ ) {
                  if (regDomain.size() > 0) {
                    rowIndex = paramGIDs[paramoffsets[paramIndex][row]];
                    poffset = paramoffsets[paramIndex][row];
                    val = regDomain.fastAccessDx(poffset);
                    regGradient[rowIndex+numClassicParams] += val;
                  }
                }
              }
            }
         
          
            if (numBoundaryParams > 0) {
              int paramIndex, rowIndex, poffset;
              double val;
              regBoundary = cells[b][e]->computeBoundaryRegularization(boundaryRegConstants,
                                                                       boundaryRegTypes, boundaryRegIndices,
                                                                       boundaryRegSides);
              for (size_t p = 0; p < numBoundaryParams; p++) {
                paramIndex = boundaryRegIndices[p];
                for( size_t row=0; row<paramoffsets[paramIndex].size(); row++ ) {
                  if (regBoundary.size() > 0) {
                    rowIndex = paramGIDs[paramoffsets[paramIndex][row]];
                    poffset = paramoffsets[paramIndex][row];
                    val = regBoundary.fastAccessDx(poffset);
                    regGradient[rowIndex+numClassicParams] += val;
                  }
                }
              }
            }
            
            totaldiff += (regDomain + regBoundary);
          }
        }
      }
      totaldiff += phys->computeTopoResp(b);
    }
  }
  
  //to gather contributions across processors
  double meep = 0.0;
  Comm->SumAll(&totaldiff.val(), &meep, 1);
  totaldiff.val() = meep;
  AD fullobj(numParams,meep);
  
  for (size_t j=0; j< numParams; j++) {
    double dval;
    double ldval = dmGradient[j] + regGradient[j];
    Comm->SumAll(&ldval,&dval,1);
    fullobj.fastAccessDx(j) = dval;
  }
  
  if(Comm->MyPID() == 0 ) {
    if (verbosity > 0) {
      cout << "********** Value of Objective Function = " << std::setprecision(16) << fullobj.val() << endl;
      cout << "*********************************************************" << endl;
    }
  }
  
  if(Comm->MyPID() == 0) {
    std::string sname2 = "obj.dat";
    ofstream objOUT(sname2.c_str());
    objOUT.precision(16);
    objOUT << fullobj.val() << endl;
    objOUT.close();
  }
  
  return fullobj;
}

// ========================================================================================
// ========================================================================================

Kokkos::View<double***,HostDevice> postprocess::computeResponse(const vector_RCP & F_soln, const int & b) {
  
  solve->sacadoizeParams(false);
  vector<double> solvetimes = solve->solvetimes;
  
  //FC responses = this->computeResponse(F_soln);
  
  int numresponses = phys->getNumResponses(b);
  int numSensors = 1;
  if (response_type == "pointwise" ) {
    numSensors = solve->numSensors;
  }
  
  Kokkos::View<double***,HostDevice> responses("responses",numSensors, numresponses, solvetimes.size());
  vector_RCP P_soln = solve->Psol[0];
  
  for (size_t tt=0; tt<solvetimes.size(); tt++) {
    
    solve->performGather(b,F_soln,0,tt);
    solve->performGather(b,P_soln,4,0);
    
    for (size_t e=0; e<cells[b].size(); e++) {
      wkset[b]->update(cells[b][e]->ip, cells[b][e]->ijac, cells[b][e]->orientation);
      
      Kokkos::View<AD***,AssemblyDevice> responsevals = cells[b][e]->computeResponse(solvetimes[tt], tt, 0);
      
      int numElem = cells[b][e]->numElem;
      for (int r=0; r<numresponses; r++) {
        if (response_type == "global" ) {
          DRV wts = wkset[b]->wts;
          for (int p=0; p<numElem; p++) {
            for (size_t j=0; j<wts.dimension(1); j++) {
              responses(0,r,tt) += responsevals(p,r,j).val() * wts(p,j);
            }
          }
        }
        else if (response_type == "pointwise" ) {
          if (responsevals.dimension(1) > 0) {
            vector<int> sensIDs = cells[b][e]->mySensorIDs;
            for (int p=0; p<numElem; p++) {
              for (size_t j=0; j<responsevals.dimension(2); j++) {
                responses(sensIDs[j],r,tt) += responsevals(p,r,j).val();
              }
            }
          }
        }
      }
      
    }
  }
  //KokkosTools::print(responses);
  
  return responses;
}

// ========================================================================================
// ========================================================================================

void postprocess::computeResponse(const vector_RCP & F_soln) {
  
  
  if(Comm->MyPID() == 0 ) {
    if (verbosity > 0) {
      cout << endl << "*********************************************************" << endl;
      cout << "***** Computing Responses ******" << endl;
      cout << "*********************************************************" << endl;
    }
  }
  
  solve->sacadoizeParams(false);
  vector<double> solvetimes = solve->solvetimes;
  for (size_t b=0; b<cells.size(); b++) {
    
    Kokkos::View<double***,HostDevice> responses = this->computeResponse(F_soln, b);
    
    int numresponses = phys->getNumResponses(b);
    int numSensors = 1;
    if (response_type == "pointwise" ) {
      numSensors = solve->numSensors;
    }
    
    
    if (response_type == "pointwise" && save_sensor_data) {
      
      srand(time(0)); //use current time as seed for random generator for noise
      
      double err = 0.0;
      
      
      for (int k=0; k<numSensors; k++) {
        stringstream ss;
        ss << k;
        string str = ss.str();
        string sname2 = sname + "." + str + ".dat";
        ofstream respOUT(sname2.c_str());
        respOUT.precision(16);
        for (size_t tt=0; tt<solvetimes.size(); tt++) { // skip the initial condition
          if(Comm->MyPID() == 0){
            respOUT << solvetimes[tt] << "  ";
          }
          for (int n=0; n<responses.dimension(1); n++) {
            double tmp1 = responses(k,n,tt);
            double tmp2 = 0.0;//globalresp(k,n,tt);
            Comm->SumAll(&tmp1, &tmp2, 1);
            err = this->makeSomeNoise(stddev);
            if(Comm->MyPID() == 0) {
              respOUT << tmp2+err << "  ";
            }
          }
          if(Comm->MyPID() == 0){
            respOUT << endl;
          }
        }
        respOUT.close();
      }
    }
    
    //KokkosTools::print(responses);
    
    if (write_dakota_output) {
      string sname2 = "results.out";
      ofstream respOUT(sname2.c_str());
      respOUT.precision(16);
      for (int k=0; k<responses.dimension(0); k++) {
        for (int n=0; n<responses.dimension(1); n++) {
          for (int m=0; m<responses.dimension(2); m++) {
            double tmp1 = responses(k,n,m);
            double tmp2 = 0.0;//globalresp(k,n,tt);
            Comm->SumAll(&tmp1, &tmp2, 1);
            if(Comm->MyPID() == 0) {
              respOUT << tmp2 << "  ";
            }
          }
        }
      }
      if(Comm->MyPID() == 0){
        respOUT << endl;
      }
      respOUT.close();
    }
    
  }
}

// ========================================================================================
// ========================================================================================

vector<double> postprocess::computeSensitivities(const vector_RCP & F_soln, const vector_RCP & A_soln) {
  
  Teuchos::RCP<Teuchos::Time> sensitivitytimer = Teuchos::rcp(new Teuchos::Time("sensitivity",false));
  sensitivitytimer->start();
  
  vector<string> active_paramnames = solve->getParamsNames(1);
  vector<size_t> active_paramlengths = solve->getParamsLengths(1);
  
  vector<double> dwr_sens;
  vector<double> disc_sens;
  
  int numClassicParams = solve->getNumParams(1);
  int numDiscParams = solve->getNumParams(4);
  int numParams = numClassicParams + numDiscParams;
  
  vector<double> gradient(numParams);
  
  AD obj_sens = this->computeObjective(F_soln);
  
  if (numClassicParams > 0 )
  dwr_sens = solve->computeSensitivities(F_soln, A_soln);
  if (numDiscParams > 0)
  disc_sens = solve->computeDiscretizedSensitivities(F_soln, A_soln);
  
  size_t pprog  = 0;
  for (size_t i=0; i<numClassicParams; i++) {
    double cobj = 0.0;
    if (i<obj_sens.size()) {
      cobj = obj_sens.fastAccessDx(i);
    }
    gradient[pprog] = cobj + dwr_sens[i];
    pprog++;
  }
  for (size_t i=0; i<numDiscParams; i++) {
    double cobj = 0.0;
    if (i<obj_sens.size()) {
      cobj = obj_sens.fastAccessDx(i+numClassicParams);
    }
    gradient[pprog] = cobj + disc_sens[i];
    pprog++;
  }
  
  sensitivitytimer->stop();
  
  if(Comm->MyPID() == 0 ) {
    if (verbosity > 0) {
      int pprog = 0;
      for (size_t p=0; p < active_paramnames.size(); p++) {
        for (size_t p2=0; p2 < active_paramlengths[p]; p2++) {
          cout << "Sensitivity of the response w.r.t " << active_paramnames[p] << " component " << p2 << " = " << gradient[pprog] << endl;
          pprog++;
        }
      }
      for (size_t p =0; p < numDiscParams; p++)
      cout << "sens w.r.t. discretized param " << p << " is " << gradient[p+numClassicParams] << endl;
      cout << "Sensitivity Calculation Time: " << sensitivitytimer->totalElapsedTime() << endl;
    }
  }
  
  return gradient;
}

// ========================================================================================
// ========================================================================================

void postprocess::writeSolution(const vector_RCP & E_soln, const std::string & filelabel) {
  
  string filename = filelabel+".exo";
  
  if(Comm->MyPID() == 0) {
    cout << endl << "*********************************************************" << endl;
    cout << "***** Writing the solution to " << filename << endl;
    cout << "*********************************************************" << endl;
  }
  
  
  //vector<stk_classic::mesh::Entity*> stk_meshElems;
  //mesh->getMyElements(blockID, stk_meshElems);
  
  if (isTD) {
    mesh->setupExodusFile(filename);
  }
  
  int numSteps = E_soln->NumVectors();
  vector<double> solvetimes = solve->solvetimes;
  
  Kokkos::View<double**,HostDevice> dispz("dispz",cells[0].size(), numNodesPerElem);
  for(int m=0; m<numSteps; m++) {
    for (size_t b=0; b<cells.size(); b++) {
      std::string blockID = blocknames[b];
      vector<vector<int> > curroffsets = phys->offsets[b];
      vector<size_t> myElements = disc->myElements[b];
      for (int n = 0; n<numVars[b]; n++) {
        Kokkos::View<double**,HostDevice> soln_computed;
        if (numBasis[b][n]>1) {
          soln_computed = Kokkos::View<double**,HostDevice>("solution",myElements.size(), numBasis[b][n]);
        }
        else {
          soln_computed = Kokkos::View<double**,HostDevice>("solution",myElements.size(), numNodesPerElem);
        }
        std::string var = varlist[b][n];
        size_t eprog = 0;
        for( size_t e=0; e<cells[b].size(); e++ ) {
          int numElem = cells[b][e]->numElem;
          for (int p=0; p<numElem; p++) {
            vector<int> GIDs = cells[b][e]->GIDs[p];
            for( int i=0; i<numBasis[b][n]; i++ ) {
              int pindex = E_overlapped_map->LID(GIDs[curroffsets[n][i]]);
              if (numBasis[b][n] == 1) {
                for( int j=0; j<numNodesPerElem; j++ ) {
                  soln_computed(eprog,j) = (*E_soln)[m][pindex];
                }
              }
              else {
                soln_computed(eprog,i) = (*E_soln)[m][pindex];
              }
              if (use_sol_mod_mesh && sol_to_mod_mesh == n) {
                if (abs(soln_computed(e,i)) >= meshmod_TOL) {
                  dispz(eprog,i) += layer_size;
                }
              }
              else if (use_sol_mod_height && sol_to_mod_height == n) {
                dispz(eprog,i) = 10.0*soln_computed(eprog,i);
              }
            }
            eprog++;
          }
        }
        if (use_sol_mod_mesh && sol_to_mod_mesh == n) {
          mesh->setSolutionFieldData("dispz", blockID, myElements, dispz);
        }
        else if (use_sol_mod_height && sol_to_mod_height == n) {
          mesh->setSolutionFieldData("dispz", blockID, myElements, dispz);
        }
        else {
          if (var == "dx") {
            mesh->setSolutionFieldData("dispx", blockID, myElements, soln_computed);
          }
          if (var == "dy") {
            mesh->setSolutionFieldData("dispy", blockID, myElements, soln_computed);
          }
          if (var == "dz" || var == "H") {
            mesh->setSolutionFieldData("dispz", blockID, myElements, soln_computed);
          }
        }
        
        mesh->setSolutionFieldData(var, blockID, myElements, soln_computed);
      }
      
      ////////////////////////////////////////////////////////////////
      // Discretized Parameters
      ////////////////////////////////////////////////////////////////
      
      vector<string> dpnames = solve->discretized_param_names;
      vector<int> numParamBasis = solve->paramNumBasis;
      if (dpnames.size() > 0) {
        vector_RCP P_soln = solve->Psol[0];
        for (size_t n=0; n<dpnames.size(); n++) {
          Kokkos::View<double**,HostDevice> soln_computed;
          if (numParamBasis[n]>1) {
            soln_computed = Kokkos::View<double**,HostDevice>("solution",myElements.size(), numParamBasis[n]);
          }
          else {
            soln_computed = Kokkos::View<double**,HostDevice>("solution",myElements.size(), numNodesPerElem);
          }
          size_t eprog = 0;
          for( size_t e=0; e<cells[b].size(); e++ ) {
            int numElem = cells[b][e]->numElem;
            for (int p=0; p<numElem; p++) {
              vector<int> paramGIDs = cells[b][e]->paramGIDs[p];
              vector<vector<int> > paramoffsets = solve->paramoffsets;
              for( int i=0; i<numParamBasis[n]; i++ ) {
                int pindex = param_overlapped_map->LID(paramGIDs[paramoffsets[n][i]]);
                if (numParamBasis[n] == 1) {
                  for( int j=0; j<numNodesPerElem; j++ ) {
                    soln_computed(e,j) = (*P_soln)[0][pindex];
                  }
                }
                else {
                  soln_computed(e,i) = (*P_soln)[0][pindex];
                }
              }
              eprog++;
            }
          }
          mesh->setSolutionFieldData(dpnames[n], blockID, myElements, soln_computed);
        }
        bool have_dRdP = solve->have_dRdP;
        if (have_dRdP) {
          vector_RCP P_soln = solve->dRdP[0];
          for (size_t n=0; n<dpnames.size(); n++) {
            Kokkos::View<double**,HostDevice> soln_computed;
            if (numParamBasis[n]>1) {
              soln_computed = Kokkos::View<double**,HostDevice>("solution",myElements.size(), numParamBasis[n]);
            }
            else {
              soln_computed = Kokkos::View<double**,HostDevice>("solution",myElements.size(), numNodesPerElem);
            }
            size_t eprog = 0;

            for( size_t e=0; e<cells[b].size(); e++ ) {
              int numElem = cells[b][e]->numElem;
              for (int p=0; p<numElem; p++) {
                vector<int> paramGIDs = cells[b][e]->paramGIDs[p];
                vector<vector<int> > paramoffsets = solve->paramoffsets;
                for( int i=0; i<numParamBasis[n]; i++ ) {
                  int pindex = param_overlapped_map->LID(paramGIDs[paramoffsets[n][i]]);
                  if (numParamBasis[n] == 1) {
                    for( int j=0; j<numNodesPerElem; j++ ) {
                      soln_computed(eprog,j) = (*P_soln)[0][pindex];
                    }
                  }
                  else {
                    soln_computed(eprog,i) = (*P_soln)[0][pindex];
                  }
                }
                eprog++;
              }
            }
            mesh->setSolutionFieldData(dpnames[n]+"_dRdP", blockID, myElements, soln_computed);
          }
        }
        
      }
      
      ////////////////////////////////////////////////////////////////
      // Mesh movement
      ////////////////////////////////////////////////////////////////
      
      bool meshpert = false;
      if (meshpert) {
        Kokkos::View<double**,HostDevice> dispx("dispx",myElements.size(), numNodesPerElem);
        Kokkos::View<double**,HostDevice> dispy("dispy",myElements.size(), numNodesPerElem);
        Kokkos::View<double**,HostDevice> dispz("dispz",myElements.size(), numNodesPerElem);
        size_t eprog = 0;
        for( size_t e=0; e<cells[b].size(); e++ ) {
          DRV nodePert = cells[b][e]->nodepert;
          for (int p=0; p<cells[b][e]->numElem; p++) {
            for( int j=0; j<numNodesPerElem; j++ ) {
              dispx(eprog,j) = nodePert(p,j,0);
              if (spaceDim > 1)
                dispy(eprog,j) = nodePert(p,j,1);
              if (spaceDim > 2)
                dispz(eprog,j) = nodePert(p,j,2);
            }
            eprog++;
          }
        }
        mesh->setSolutionFieldData("dispx", blockID, myElements, dispx);
        mesh->setSolutionFieldData("dispy", blockID, myElements, dispy);
        mesh->setSolutionFieldData("dispz", blockID, myElements, dispz);
      }
      
      ////////////////////////////////////////////////////////////////
      // Plot response
      ////////////////////////////////////////////////////////////////
      
      
      if (plot_response) {
        vector_RCP P_soln = solve->Psol[0];
        vector<string> responsefieldnames = phys->getResponseFieldNames(b);
        vector<Kokkos::View<double**,HostDevice> > responsefields;
        for (size_t j=0; j<responsefieldnames.size(); j++) {
          Kokkos::View<double**,HostDevice> rfdata("response data",myElements.size(), numNodesPerElem);
          responsefields.push_back(rfdata);
        }
        Kokkos::View<AD***,AssemblyDevice> rfields; // response for each cell
        size_t eprog = 0;
        for (size_t k=0; k<cells[b].size(); k++) {
          DRV nodes = cells[b][k]->nodes;
          rfields = cells[b][k]->computeResponseAtNodes(nodes, m, solvetimes[m]);
          for (int p=0; p<cells[b][k]->numElem; p++) {
            for (size_t j=0; j<responsefieldnames.size(); j++) {
              for (size_t i=0; i<nodes.dimension(1); i++) {
                responsefields[j](eprog,i) = rfields(p,j,i).val();
              }
            }
            eprog++;
          }
        }
        for (size_t j=0; j<responsefieldnames.size(); j++) {
          mesh->setSolutionFieldData(responsefieldnames[j], blockID, myElements, responsefields[j]);
        }
      }
      
      
      ////////////////////////////////////////////////////////////////
      // Extra nodal fields
      ////////////////////////////////////////////////////////////////
      
      
      vector<string> extrafieldnames = phys->getExtraFieldNames(b);
      vector<Kokkos::View<double**,HostDevice> > extrafields;
      for (size_t j=0; j<extrafieldnames.size(); j++) {
        Kokkos::View<double**,HostDevice> efdata("field data",myElements.size(), numNodesPerElem);
        extrafields.push_back(efdata);
      }
      
      Kokkos::View<double***,AssemblyDevice> cfields;
      size_t eprog = 0;
      for (size_t k=0; k<cells[b].size(); k++) {
        DRV nodes = cells[b][k]->nodes;
        cfields = phys->getExtraFields(b, nodes, solvetimes[m], wkset[b]);
        for (int p=0; p<cells[b][k]->numElem; p++) {
          size_t j = 0;
          for (size_t h=0; h<cfields.dimension(1); h++) {
            for (size_t i=0; i<cfields.dimension(2); i++) {
              extrafields[j](eprog,i) = cfields(p,h,i);
            }
            ++j;
          }
          eprog++;
        }
      }
      
      for (size_t j=0; j<extrafieldnames.size(); j++) {
        mesh->setSolutionFieldData(extrafieldnames[j], blockID, myElements, extrafields[j]);
      }
      
      ////////////////////////////////////////////////////////////////
      // Extra cell fields
      ////////////////////////////////////////////////////////////////
      
      
      vector<string> extracellfieldnames = phys->getExtraCellFieldNames(b);
      
      vector<Kokkos::View<double**,HostDevice> > extracellfields;
      for (size_t j=0; j<extracellfieldnames.size(); j++) {
        Kokkos::View<double**,HostDevice> efdata("cell data",myElements.size(), 1);
        extracellfields.push_back(efdata);
      }
      eprog = 0;
      for (size_t k=0; k<cells[b].size(); k++) {
        DRV nodes = cells[b][k]->nodes;
        Kokkos::View<double***,HostDevice> center("center",nodes.dimension(0),1,spaceDim);
        int numnodes = nodes.dimension(1);
        for (int p=0; p<cells[b][k]->numElem; p++) {
          for (int i=0; i<numnodes; i++) {
            for (int d=0; d<spaceDim; d++) {
              center(p,0,d) += nodes(p,i,d) / numnodes;
            }
          }
        }
        cells[b][k]->updateSolnWorkset(E_soln, m); // also updates ip, ijac
        cells[b][k]->updateData();
        wkset[b]->time = solvetimes[m];
        Kokkos::View<double***,HostDevice> cfields = phys->getExtraCellFields(b, cells[b][k]->numElem);
        for (int p=0; p<cells[b][k]->numElem; p++) {
          size_t j = 0;
          for (size_t h=0; h<cfields.dimension(1); h++) {
            extracellfields[j](eprog,0) = cfields(p,h,0);
            ++j;
          }
          eprog++;
        }
      }
      for (size_t j=0; j<extracellfieldnames.size(); j++) {
        mesh->setCellFieldData(extracellfieldnames[j], blockID, myElements, extracellfields[j]);
      }
      
      
      if (cells[b][0]->have_cell_phi || cells[b][0]->have_cell_rotation) {
        Kokkos::View<double**,HostDevice> cdata("cell data",myElements.size(), 1);
        int eprog = 0;
        for (size_t k=0; k<cells[b].size(); k++) {
          vector<size_t> cell_data_seed = cells[b][k]->cell_data_seed;
          vector<size_t> cell_data_seedindex = cells[b][k]->cell_data_seedindex;
          Kokkos::View<double**> cell_data = cells[b][k]->cell_data;
          for (int p=0; p<cells[b][k]->numElem; p++) {
            /*
            if (cell_data.dimension(1) == 3) {
              cdata(eprog,0) = cell_data(p,0);//cell_data_seed[p];
            }
            else if (cell_data.dimension(1) == 9) {
              cdata(eprog,0) = cell_data(p,0);//*cell_data(p,4)*cell_data(p,8);//cell_data_seed[p];
            }
             */
            cdata(eprog,0) = cell_data_seedindex[p];
            eprog++;
          }
        }
        mesh->setCellFieldData("mesh_data_seed", blockID, myElements, cdata);
      }
      
      if (have_subgrids) {
        Kokkos::View<double**,HostDevice> cdata("cell data",myElements.size(), 1);
        int eprog = 0;
        for (size_t k=0; k<cells[b].size(); k++) {
          vector<vector<size_t> > subgrid_model_index = cells[b][k]->subgrid_model_index;

          for (int p=0; p<cells[b][k]->numElem; p++) {
            cdata(eprog,0) = subgrid_model_index[p][m];
            eprog++;
          }
        }
        mesh->setCellFieldData("subgrid model", blockID, myElements, cdata);
      }
      /*
      if (have_subgrids) {
        Kokkos::View<double**,HostDevice> subgrid_mean_fields = solve->multiscale_manager->getMeanCellFields(b, m,
                                                                                                             solvetimes[m],
                                                                                                             extracellfieldnames.size());
        Kokkos::View<double**,HostDevice> csf("csf",myElements.size(),1);
        int eprog = 0;
        for (size_t j=0; j<extracellfieldnames.size(); j++) {
          for (size_t e=0; e<cells[b].size(); e++) {
            for (int p=0; p<cells[b][e]->numElem; p++) {
              csf(eprog,0) = subgrid_mean_fields(e,j);
              eprog++;
            }
          }
          string sgfn = "subgrid_mean_" + extracellfieldnames[j];
          mesh->setCellFieldData(sgfn, blockID, myElements, csf);
        }
       
      }
       */
      
      
      if(isTD) {
        mesh->writeToExodus(solvetimes[m]);
      }
      else
      mesh->writeToExodus(filename);
    }
  }
  
  if (save_height_file) {
    ofstream hOUT("meshpert.dat");
    hOUT.precision(10);
    int numsteps = E_soln->NumVectors();
    for (size_t b=0; b<numBlocks; b++) {
      std::string blockID = blocknames[b];
      vector<vector<int> > curroffsets = phys->offsets[b];
      vector<size_t> myElements = disc->myElements[b];
      for (int n = 0; n<numVars[b]; n++) {
        for( size_t e=0; e<cells[b].size(); e++ ) {
          DRV nodes = cells[b][e]->nodes;
          for (int p=0; p<cells[b][e]->numElem; p++) {
            vector<int> GIDs = cells[b][e]->GIDs[p];
            for( int i=0; i<numBasis[b][n]; i++ ) {
              int pindex = E_overlapped_map->LID(GIDs[curroffsets[n][i]]);
              double soln = (*E_soln)[numsteps-1][pindex];
              hOUT << nodes(p,i,0) << "  " << nodes(p,i,1) << "  " << soln << endl;
            }
          }
        }
      }
    }
    hOUT.close();
  }
  
  solve->multiscale_manager->writeSolution(filelabel, solvetimes, Comm->MyPID());
  
  //for (size_t b=0; b<cells.size(); b++) {
  //  for (size_t e=0; e<cells[b].size(); e++) {
  //    stringstream ss;
  //    ss << Comm->MyPID() << "." << e;
  //    string blockname = "subgrid_data/subgrid.exo." + ss.str();// + ".exo";
  //    cells[b][e]->writeSubgridSolution(blockname);
  //  }
  //}
  if(Comm->MyPID() == 0) {
    cout << endl << "*********************************************************" << endl;
    cout << "***** Finished Writing the solution to " << filename << endl;
    cout << "*********************************************************" << endl;
  }
}


// ========================================================================================
// ========================================================================================

double postprocess::makeSomeNoise(double stdev) {
  //generate sample from 0-centered normal with stdev
  //Box-Muller method
  //srand(time(0)); //doing this more frequently than once-per-second results in getting the same numbers...
  double U1 = rand()/double(RAND_MAX);
  double U2 = rand()/double(RAND_MAX);
  
  return stdev*sqrt(-2*log(U1))*cos(2*PI*U2);
}
