/***********************************************************************
 Multiscale/Multiphysics Interfaces for Large-scale Optimization (MILO)
 
 Copyright 2018 National Technology & Engineering Solutions of Sandia,
 LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the
 U.S. Government retains certain rights in this software.”
 
 Questions? Contact Tim Wildey (tmwilde@sandia.gov) and/or
 Bart van Bloemen Waanders (bartv@sandia.gov)
 ************************************************************************/

#ifndef SHALLOWWATER_H
#define SHALLOWWATER_H

#include "physics_base.hpp"

static void shallowwaterHelp() {
  cout << "********** Help and Documentation for the Shallow Water Physics Module **********" << endl << endl;
  cout << "Model:" << endl << endl;
  cout << "User defined functions: " << endl << endl;
}

class shallowwater : public physicsbase {
public:
  
  shallowwater() {} ;
  
  ~shallowwater() {};
  
  // ========================================================================================
  /* Constructor to set up the problem */
  // ========================================================================================
  
  shallowwater(Teuchos::RCP<Teuchos::ParameterList> & settings, const int & numip_,
               const size_t & numip_side_, const int & numElem_,
               Teuchos::RCP<FunctionInterface> & functionManager_,
               const size_t & blocknum_) :
  numip(numip_), numip_side(numip_side_), numElem(numElem_), functionManager(functionManager_),
  blocknum(blocknum_) {
    
    label = "shallowwater";
    
    spaceDim = settings->sublist("Mesh").get<int>("dim",2);
    numElem = settings->sublist("Solver").get<int>("Workset size",1);
    
    myvars.push_back("H");
    myvars.push_back("Hu");
    myvars.push_back("Hv");
    mybasistypes.push_back("HGRAD");
    mybasistypes.push_back("HGRAD");
    mybasistypes.push_back("HGRAD");
    
    if (settings->sublist("Physics").get<int>("solver",0) == 1)
      isTD = true;
    else
      isTD = false;
    
    multiscale = settings->isSublist("Subgrid");
    analysis_type = settings->sublist("Analysis").get<string>("analysis type","forward");
    
    numResponses = settings->sublist("Physics").get<int>("numResp_thermal",1);
    useScalarRespFx = settings->sublist("Physics").get<bool>("use scalar response function (thermal)",false);
    
    gravity = settings->sublist("Physics").get<double>("gravity",9.8);
    
    formparam = settings->sublist("Physics").get<double>("form_param",1.0);
    
    
    Teuchos::ParameterList fs = settings->sublist("Functions");
    functionManager->addFunction("source Hu",fs.get<string>("source Hu","0.0"),numElem,numip,"ip",blocknum);
    functionManager->addFunction("source Hv",fs.get<string>("source Hv","0.0"),numElem,numip,"ip",blocknum);
    functionManager->addFunction("Neumann source H",fs.get<string>("Neumann source H","0.0"),numElem,numip_side,"side ip",blocknum);
    functionManager->addFunction("Neumann source Hu",fs.get<string>("Neumann source Hu","0.0"),numElem,numip_side,"side ip",blocknum);
    functionManager->addFunction("Neumann source Hv",fs.get<string>("Neumann source Hv","0.0"),numElem,numip_side,"side ip",blocknum);
    
  }
  
  // ========================================================================================
  // ========================================================================================
  
  void volumeResidual() {
    
    // NOTES:
    // 1. basis and basis_grad already include the integration weights
    
    
    {
      Teuchos::TimeMonitor funceval(*volumeResidualFunc);
      source_Hu = functionManager->evaluate("source Hu","ip",blocknum);
      source_Hv = functionManager->evaluate("source Hv","ip",blocknum);
    }
    
    sol = wkset->local_soln;
    sol_dot = wkset->local_soln_dot;
    sol_grad = wkset->local_soln_grad;
    offsets = wkset->offsets;
    res = wkset->res;
    
    Teuchos::TimeMonitor resideval(*volumeResidualFill);
    
    int H_basis_num = wkset->usebasis[H_num];
    Hbasis = wkset->basis[H_basis_num];
    Hbasis_grad = wkset->basis_grad[H_basis_num];
    
    int Hu_basis_num = wkset->usebasis[Hu_num];
    Hubasis = wkset->basis[Hu_basis_num];
    Hubasis_grad = wkset->basis_grad[Hu_basis_num];
    
    int Hv_basis_num = wkset->usebasis[Hv_num];
    Hvbasis = wkset->basis[Hv_basis_num];
    Hvbasis_grad = wkset->basis_grad[Hv_basis_num];
    
    parallel_for(RangePolicy<AssemblyDevice>(0,res.dimension(0)), KOKKOS_LAMBDA (const int e ) {
      double v = 0.0;
      double dvdx = 0.0;
      double dvdy = 0.0;
      for (int k=0; k<sol.dimension(2); k++ ) {
      
        AD H = sol(e,H_num,k,0);
        AD H_dot = sol_dot(e,H_num,k,0);
        
        AD Hu = sol(e,Hu_num,k,0);
        AD Hu_dot = sol_dot(e,Hu_num,k,0);
        
        AD Hv = sol(e,Hv_num,k,0);
        AD Hv_dot = sol_dot(e,Hv_num,k,0);
        
        
        for (int i=0; i<Hbasis.dimension(1); i++ ) {
          
          int resindex = offsets(H_num,i);
          v = Hbasis(e,i,k);
          
          dvdx = Hbasis_grad(e,i,k,0);
          dvdy = Hbasis_grad(e,i,k,1);
          
          res(e,resindex) += H_dot*v - Hu*dvdx - Hv*dvdy;
          
        }
        
        for (int i=0; i<Hubasis.dimension(1); i++ ) {
          
          int resindex = offsets(Hu_num,i);
          v = Hubasis(e,i,k);
          
          dvdx = Hubasis_grad(e,i,k,0);
          dvdy = Hubasis_grad(e,i,k,1);
          
          res(e,resindex) += Hu_dot*v - (Hu*Hu/H + 0.5*gravity*H*H)*dvdx - Hv*Hu/H*dvdy + gravity*source_Hu(e,i)*v;
          
        }
        
        for (int i=0; i<Hvbasis.dimension(1); i++ ) {
          
          int resindex = offsets(Hv_num,i);
          v = Hvbasis(e,i,k);
          
          dvdx = Hvbasis_grad(e,i,k,0);
          dvdy = Hvbasis_grad(e,i,k,1);
          
          res(e,resindex) += Hv_dot*v - (Hu*Hu/H)*dvdx - (Hv*Hu/H + 0.5*gravity*H*H)*dvdy + gravity*source_Hv(e,i)*v;
          
        }
        
      }
      
    });
    
  }
  
  
  // ========================================================================================
  // ========================================================================================
  
  void boundaryResidual() {
    
    // NOTES:
    // 1. basis and basis_grad already include the integration weights
    
    
    {
      Teuchos::TimeMonitor localtime(*boundaryResidualFunc);
      nsource_H = functionManager->evaluate("Neumann source H","side ip",blocknum);
      nsource_Hu = functionManager->evaluate("Neumann source Hu","side ip",blocknum);
      nsource_Hv = functionManager->evaluate("Neumann source Hv","side ip",blocknum);
    }
    
    sideinfo = wkset->sideinfo;
    sol = wkset->local_soln_side;
    sol_grad = wkset->local_soln_grad_side;
    offsets = wkset->offsets;
    res = wkset->res;
    
    int H_basis_num = wkset->usebasis[H_num];
    Hbasis = wkset->basis_side[H_basis_num];
    Hbasis_grad = wkset->basis_grad_side[H_basis_num];
    
    int Hu_basis_num = wkset->usebasis[Hu_num];
    Hubasis = wkset->basis_side[Hu_basis_num];
    Hubasis_grad = wkset->basis_grad_side[Hu_basis_num];
    
    int Hv_basis_num = wkset->usebasis[Hv_num];
    Hvbasis = wkset->basis_side[Hv_basis_num];
    Hvbasis_grad = wkset->basis_grad_side[Hv_basis_num];
    
    Teuchos::TimeMonitor localtime(*boundaryResidualFill);

    int cside = wkset->currentside;
    for (int e=0; e<sideinfo.dimension(0); e++) {
      if (sideinfo(e,H_num,cside,0) == 2) { // Element e is on the side
        for (int k=0; k<Hbasis.dimension(2); k++ ) {
          for (int i=0; i<Hbasis.dimension(1); i++ ) {
            int resindex = offsets(H_num,i);
            double v = Hbasis(e,i,k);
            res(e,resindex) += -nsource_H(e,k)*v;
          }
        }
      }
      if (sideinfo(e,Hu_num,cside,0) == 2) { // Element e is on the side
        for (int k=0; k<Hubasis.dimension(2); k++ ) {
          for (int i=0; i<Hubasis.dimension(1); i++ ) {
            int resindex = offsets(Hu_num,i);
            double v = Hubasis(e,i,k);
            res(e,resindex) += -nsource_Hu(e,k)*v;
          }
        }
      }
      if (sideinfo(e,Hv_num,cside,0) == 2) { // Element e is on the side
        for (int k=0; k<Hvbasis.dimension(2); k++ ) {
          for (int i=0; i<Hvbasis.dimension(1); i++ ) {
            int resindex = offsets(Hv_num,i);
            double v = Hvbasis(e,i,k);
            res(e,resindex) += -nsource_Hv(e,k)*v;
          }
        }
      }
    }
    
  }
  
  
  // ========================================================================================
  // The boundary/edge flux
  // ========================================================================================
  
  void computeFlux() {
        
  }
  
  // ========================================================================================
  // ========================================================================================
  
  void setVars(std::vector<string> & varlist_) {
    varlist = varlist_;
    for (size_t i=0; i<varlist.size(); i++) {
      if (varlist[i] == "H")
        H_num = i;
      if (varlist[i] == "Hu")
        Hu_num = i;
      if (varlist[i] == "Hv")
        Hv_num = i;
    }
  }
  
  
  // ========================================================================================
  // ========================================================================================
  
private:
  
  Teuchos::RCP<FunctionInterface> functionManager;

  data grains;
  
  size_t numip, numip_side, blocknum;
  
  int spaceDim, numElem, numParams, numResponses;
  vector<string> varlist;
  int H_num, Hu_num, Hv_num;
  double alpha;
  double gravity;
  bool isTD;
  //int test, simNum;
  //string simName;
  
  FDATA source_Hu, source_Hv, nsource_H, nsource_Hu, nsource_Hv;
  Kokkos::View<AD****,AssemblyDevice> sol, sol_dot, sol_grad;
  Kokkos::View<AD***,AssemblyDevice> aux;
  Kokkos::View<AD**,AssemblyDevice> res;
  Kokkos::View<int**,AssemblyDevice> offsets;
  Kokkos::View<int****,AssemblyDevice> sideinfo;
  DRV Hbasis, Hbasis_grad, Hubasis, Hubasis_grad, Hvbasis, Hvbasis_grad;
  
  
  string analysis_type; //to know when parameter is a sample that needs to be transformed
  
  bool useScalarRespFx;
  bool multiscale;
  double formparam;
  
  Teuchos::RCP<Teuchos::Time> volumeResidualFunc = Teuchos::TimeMonitor::getNewCounter("MILO::shallowwater::volumeResidual() - function evaluation");
  Teuchos::RCP<Teuchos::Time> volumeResidualFill = Teuchos::TimeMonitor::getNewCounter("MILO::shallowwater::volumeResidual() - evaluation of residual");
  Teuchos::RCP<Teuchos::Time> boundaryResidualFunc = Teuchos::TimeMonitor::getNewCounter("MILO::shallowwater::boundaryResidual() - function evaluation");
  Teuchos::RCP<Teuchos::Time> boundaryResidualFill = Teuchos::TimeMonitor::getNewCounter("MILO::shallowwater::boundaryResidual() - evaluation of residual");
  Teuchos::RCP<Teuchos::Time> fluxFunc = Teuchos::TimeMonitor::getNewCounter("MILO::shallowwater::computeFlux() - function evaluation");
  Teuchos::RCP<Teuchos::Time> fluxFill = Teuchos::TimeMonitor::getNewCounter("MILO::shallowwater::computeFlux() - evaluation of flux");
  
  //Teuchos::RCP<DRVAD> src_test;
  //Teuchos::RCP<FunctionBase> source_Hu_fct, source_Hv_fct, nsource_H_fct, nsource_Hu_fct, nsource_Hv_fct;
  
  
};

#endif
