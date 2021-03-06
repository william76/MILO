/***********************************************************************
 Multiscale/Multiphysics Interfaces for Large-scale Optimization (MILO)
 
 Copyright 2018 National Technology & Engineering Solutions of Sandia,
 LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the
 U.S. Government retains certain rights in this software.”
 
 Questions? Contact Tim Wildey (tmwilde@sandia.gov) and/or
 Bart van Bloemen Waanders (bartv@sandia.gov)
 ************************************************************************/

#include "meshInterface.hpp"

meshInterface::meshInterface(Teuchos::RCP<Teuchos::ParameterList> & settings_, const Teuchos::RCP<Epetra_MpiComm> & Commptr_) :
settings(settings_), Commptr(Commptr_) {
  
  
  using Teuchos::RCP;
  using Teuchos::rcp;
  
  shape = settings->sublist("Mesh").get<string>("shape","quad");
  spaceDim = settings->sublist("Mesh").get<int>("dim",2);
  verbosity = settings->sublist("Postprocess").get<int>("Verbosity",1);
  
  have_mesh_data = false;
  compute_mesh_data = settings->sublist("Mesh").get<bool>("Compute mesh data",false);
  have_rotations = false;
  have_rotation_phi = false;
  have_multiple_data_files = false;
  mesh_data_file_tag = "none";
  mesh_data_pts_tag = "mesh_data_pts";
  number_mesh_data_files = 1;
  
  mesh_data_tag = settings->sublist("Mesh").get<string>("Data file","none");
  if (mesh_data_tag != "none") {
    mesh_data_pts_tag = settings->sublist("Mesh").get<string>("Data points file","mesh_data_pts");
    
    have_mesh_data = true;
    have_rotation_phi = settings->sublist("Mesh").get<bool>("Have mesh data phi",false);
    have_rotations = settings->sublist("Mesh").get<bool>("Have mesh data rotations",true);
    have_multiple_data_files = settings->sublist("Mesh").get<bool>("Have multiple mesh data files",false);
    number_mesh_data_files = settings->sublist("Mesh").get<int>("Number mesh data files",1);
    
  }
  
  shards::CellTopology cTopo;
  shards::CellTopology sTopo;
  
  if (spaceDim == 1) {
    //Sh_cellTopo = shards::CellTopology(shards::getCellTopologyData<shards::Line<> >() );// lin. cell topology on the interior
    //Sh_sideTopo = shards::CellTopology(shards::getCellTopologyData<shards::Node<> >() );          // line cell topology on the boundary
  }
  if (spaceDim == 2) {
    if (shape == "quad") {
      cTopo = shards::CellTopology(shards::getCellTopologyData<shards::Quadrilateral<> >() );// lin. cell topology on the interior
      sTopo = shards::CellTopology(shards::getCellTopologyData<shards::Line<> >() );          // line cell topology on the boundary
    }
    if (shape == "tri") {
      cTopo = shards::CellTopology(shards::getCellTopologyData<shards::Triangle<> >() );// lin. cell topology on the interior
      sTopo = shards::CellTopology(shards::getCellTopologyData<shards::Line<> >() );          // line cell topology on the boundary
    }
  }
  if (spaceDim == 3) {
    if (shape == "hex") {
      cTopo = shards::CellTopology(shards::getCellTopologyData<shards::Hexahedron<> >() );// lin. cell topology on the interior
      sTopo = shards::CellTopology(shards::getCellTopologyData<shards::Quadrilateral<> >() );          // line cell topology on the boundary
    }
    if (shape == "tet") {
      cTopo = shards::CellTopology(shards::getCellTopologyData<shards::Tetrahedron<> >() );// lin. cell topology on the interior
      sTopo = shards::CellTopology(shards::getCellTopologyData<shards::Triangle<> >() );          // line cell topology on the boundary
    }
    
  }
  // Get dimensions
  numNodesPerElem = cTopo.getNodeCount();
  settings->sublist("Mesh").set("numNodesPerElem",numNodesPerElem,"Number of nodes per element");
  sideDim = sTopo.getDimension();
  settings->sublist("Mesh").set("sideDim",sideDim,"Dimension of the sides of each element");
  numSides = cTopo.getSideCount();
  numFaces = cTopo.getFaceCount();
  if (spaceDim == 1)
    settings->sublist("Mesh").set("numSidesPerElem",2,"Number of sides per element");
  if (spaceDim == 2)
    settings->sublist("Mesh").set("numSidesPerElem",numSides,"Number of sides per element");
  if (spaceDim == 3)
    settings->sublist("Mesh").set("numSidesPerElem",numFaces,"Number of sides per element");
  
  // Define a parameter list with the required fields for the panzer_stk mesh factory
  RCP<Teuchos::ParameterList> pl = rcp(new Teuchos::ParameterList);
  
  if (settings->sublist("Mesh").get<std::string>("Source","Internal") ==  "Exodus") {
    mesh_factory = Teuchos::rcp(new panzer_stk::STK_ExodusReaderFactory());
    pl->set("File Name",settings->sublist("Mesh").get<std::string>("Mesh_File","mesh.exo"));
  }
  else if (settings->sublist("Mesh").get<std::string>("Source","Internal") ==  "Pamgen") { // NOT TESTED IN MILO YET
    mesh_factory = Teuchos::rcp(new panzer_stk::STK_PamgenReaderFactory());
    pl->set("File Name",settings->sublist("Mesh").get<std::string>("Mesh_File","mesh.pmg"));
  }
  else {
    pl->set("X Blocks",settings->sublist("Mesh").get("Xblocks",1));
    pl->set("X Elements",settings->sublist("Mesh").get("NX",20));
    pl->set("X0",settings->sublist("Mesh").get("xmin",0.0));
    pl->set("Xf",settings->sublist("Mesh").get("xmax",1.0));
    pl->set("X Procs", settings->sublist("Mesh").get("Xprocs",Commptr->NumProc()));
    if (spaceDim > 1) {
      pl->set("Y Blocks",settings->sublist("Mesh").get("Yblocks",1));
      pl->set("Y Elements",settings->sublist("Mesh").get("NY",20));
      pl->set("Y0",settings->sublist("Mesh").get("ymin",0.0));
      pl->set("Yf",settings->sublist("Mesh").get("ymax",1.0));
      pl->set("Y Procs", settings->sublist("Mesh").get("Yprocs",1));
    }
    if (spaceDim > 2) {
      pl->set("Z Blocks",settings->sublist("Mesh").get("Zblocks",1));
      pl->set("Z Elements",settings->sublist("Mesh").get("NZ",20));
      pl->set("Z0",settings->sublist("Mesh").get("zmin",0.0));
      pl->set("Zf",settings->sublist("Mesh").get("zmax",1.0));
      pl->set("Z Procs", settings->sublist("Mesh").get("Zprocs",1));
    }
    if (spaceDim == 1)
    mesh_factory = Teuchos::rcp(new panzer_stk::LineMeshFactory());
    if (spaceDim == 2) {
      if (shape == "quad") {
        mesh_factory = Teuchos::rcp(new panzer_stk::SquareQuadMeshFactory());
      }
      if (shape == "tri") {
        mesh_factory = Teuchos::rcp(new panzer_stk::SquareTriMeshFactory());
      }
    }
    if (spaceDim == 3) {
      if (shape == "hex") {
        mesh_factory = Teuchos::rcp(new panzer_stk::CubeHexMeshFactory());
      }
      if (shape == "tet") {
        mesh_factory = Teuchos::rcp(new panzer_stk::CubeTetMeshFactory());
      }
    }
  }
  
  mesh_factory->setParameterList(pl);
  
  // create the mesh
  mesh = mesh_factory->buildUncommitedMesh(Commptr->Comm());
  
  
  vector<string> eBlocks;
  mesh->getElementBlockNames(eBlocks);

  for (size_t b=0; b<eBlocks.size(); b++) {
    cellTopo.push_back(mesh->getCellTopology(eBlocks[b]));
  }
  
  for (size_t b=0; b<eBlocks.size(); b++) {
    topo_RCP cellTopo = mesh->getCellTopology(eBlocks[b]);
    string shape = cellTopo->getName();
    if (spaceDim == 1) {
      // nothing to do here?
    }
    if (spaceDim == 2) {
      if (shape == "Quadrilateral_4") {
        sideTopo.push_back(Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Line<> >() )));
      }
      if (shape == "Triangle_3") {
        sideTopo.push_back(Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Line<> >() )));
      }
    }
    if (spaceDim == 3) {
      if (shape == "Hexahedron_8") {
        sideTopo.push_back(Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Quadrilateral<> >() )));
      }
      if (shape == "Tetrahedron_4") {
        sideTopo.push_back(Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Triangle<> >() )));
      }
    }
    
  }

}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void meshInterface::finalize(Teuchos::RCP<physics> & phys) {
  
  ////////////////////////////////////////////////////////////////////////////////
  // Add fields to the mesh
  ////////////////////////////////////////////////////////////////////////////////
  
  vector<string> eBlocks;
  mesh->getElementBlockNames(eBlocks);
  
  for(std::size_t i=0;i<eBlocks.size();i++) {
    
    std::vector<string> varlist = phys->varlist[i];
    for (size_t j=0; j<varlist.size(); j++) {
      mesh->addSolutionField(varlist[j], eBlocks[i]);
    }
    mesh->addSolutionField("dispx", eBlocks[i]);
    mesh->addSolutionField("dispy", eBlocks[i]);
    mesh->addSolutionField("dispz", eBlocks[i]);
    
    bool plot_response = settings->sublist("Postprocess").get<bool>("Plot Response",false);
    if (plot_response) {
      std::vector<string> responsefields = phys->getResponseFieldNames(i);
      for (size_t j=0; j<responsefields.size(); j++) {
        mesh->addSolutionField(responsefields[j], eBlocks[i]);
      }
    }
    
    Teuchos::ParameterList efields;
    if (settings->sublist("Physics").isSublist(eBlocks[i])) {
      efields = settings->sublist("Physics").sublist(eBlocks[i]).sublist("extra fields");
    }
    else {
      efields = settings->sublist("Physics").sublist("extra fields");
    }
    Teuchos::ParameterList::ConstIterator ef_itr = efields.begin();
    while (ef_itr != efields.end()) {
      mesh->addSolutionField(ef_itr->first, eBlocks[i]);
      ef_itr++;
    }
    
    Teuchos::ParameterList ecfields;
    if (settings->sublist("Physics").isSublist(eBlocks[i])) {
      ecfields = settings->sublist("Physics").sublist(eBlocks[i]).sublist("extra cell fields");
    }
    else {
      ecfields = settings->sublist("Physics").sublist("extra cell fields");
    }
    Teuchos::ParameterList::ConstIterator ecf_itr = ecfields.begin();
    while (ecf_itr != ecfields.end()) {
      mesh->addCellField(ecf_itr->first, eBlocks[i]);
      if (settings->isSublist("Subgrid")) {
        string sgfn = "subgrid_mean_" + ecf_itr->first;
        mesh->addCellField(sgfn, eBlocks[i]);
      }
      ecf_itr++;
    }
    /*
    std::vector<string> extrafields = phys->getExtraFieldNames(i);
    for (size_t j=0; j<extrafields.size(); j++) {
      mesh->addSolutionField(extrafields[j], eBlocks[i]);
    }
    std::vector<string> extracellfields = phys->getExtraCellFieldNames(i);
    for (size_t j=0; j<extracellfields.size(); j++) {
      mesh->addCellField(extracellfields[j], eBlocks[i]);
    }
    if (settings->isSublist("Subgrid")) {
      for (size_t j=0; j<extracellfields.size(); j++) {
        string sgfn = "subgrid_mean_" + extracellfields[j];
        mesh->addCellField(sgfn, eBlocks[i]);
      }
    }
     */
    if (have_mesh_data || compute_mesh_data) {
      mesh->addCellField("mesh_data_seed", eBlocks[i]);
    }
    
    mesh->addCellField("subgrid model", eBlocks[i]);
    
    if (settings->isSublist("Parameters")) {
      Teuchos::ParameterList parameters = settings->sublist("Parameters");
      Teuchos::ParameterList::ConstIterator pl_itr = parameters.begin();
      while (pl_itr != parameters.end()) {
        Teuchos::ParameterList newparam = parameters.sublist(pl_itr->first);
        if (newparam.get<string>("usage") == "discretized") {
          mesh->addSolutionField(pl_itr->first, eBlocks[i]);
          mesh->addSolutionField(pl_itr->first + "_dRdP", eBlocks[i]);
        }
        pl_itr++;
      }
    }
  }
  
  mesh_factory->completeMeshConstruction(*mesh,Commptr->Comm());
  
  //this->perturbMesh();
  
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

DRV meshInterface::perturbMesh(const int & b, DRV & blocknodes) {
  
  ////////////////////////////////////////////////////////////////////////////////
  // Perturb the mesh (if requested)
  ////////////////////////////////////////////////////////////////////////////////
  
  vector<string> eBlocks;
  mesh->getElementBlockNames(eBlocks);
  
  //for (size_t b=0; b<eBlocks.size(); b++) {
    //vector<size_t> localIds;
    //DRV blocknodes;
    //panzer_stk::workset_utils::getIdsAndVertices(*mesh, eBlocks[b], localIds, blocknodes);
    int numNodesPerElem = blocknodes.dimension(1);
    DRV blocknodePert("blocknodePert",blocknodes.dimension(0),numNodesPerElem,spaceDim);
    
    if (settings->sublist("Mesh").get("Modify Mesh Height",false)) {
      vector<vector<double> > values;
      
      string ptsfile = settings->sublist("Mesh").get("Mesh Pert File","meshpert.dat");
      ifstream fin(ptsfile.c_str());
      
      for (string line; getline(fin, line); )
      {
        replace(line.begin(), line.end(), ',', ' ');
        istringstream in(line);
        values.push_back(vector<double>(istream_iterator<double>(in),
                                        istream_iterator<double>()));
      }
      
      DRV pertdata("pertdata",values.size(),3);
      for (size_t i=0; i<values.size(); i++) {
        for (size_t j=0; j<3; j++) {
          pertdata(i,j) = values[i][j];
        }
      }
      int Nz = settings->sublist("Mesh").get<int>("NZ",1);
      double zmin = settings->sublist("Mesh").get<double>("zmin",0.0);
      double zmax = settings->sublist("Mesh").get<double>("zmax",1.0);
      for (int k=0; k<blocknodes.dimension(0); k++) {
        for (int i=0; i<numNodesPerElem; i++){
          double x = blocknodes(k,i,0);
          double y = blocknodes(k,i,1);
          double z = blocknodes(k,i,2);
          int node;
          double dist = (double)RAND_MAX;
          for( size_t j=0; j<pertdata.dimension(0); j++ ) {
            double xhat = pertdata(j,0);
            double yhat = pertdata(j,1);
            double d = sqrt((x-xhat)*(x-xhat) + (y-yhat)*(y-yhat));
            if( d<dist ) {
              node = j;
              dist = d;
            }
          }
          double ch = pertdata(node,2);
          blocknodePert(k,i,0) = 0.0;
          blocknodePert(k,i,1) = 0.0;
          blocknodePert(k,i,2) = (ch)*(z-zmin)/(zmax-zmin);
        }
        //for (int k=0; k<blocknodeVert.dimension(0); k++) {
        //  for (int i=0; i<numNodesPerElem; i++){
        //    for (int s=0; s<spaceDim; s++) {
        //      blocknodeVert(k,i,s) += blocknodePert(k,i,s);
        //    }
        //  }
        //}
      }
    }
    
    if (settings->sublist("Mesh").get("Modify Mesh",false)) {
      for (int k=0; k<blocknodes.dimension(0); k++) {
        for (int i=0; i<numNodesPerElem; i++){
          blocknodePert(k,i,0) = 0.0;
          blocknodePert(k,i,1) = 0.0;
          blocknodePert(k,i,2) = 0.0 + 0.2*sin(2*3.14159*blocknodes(k,i,0))*sin(2*3.14159*blocknodes(k,i,1));
        }
      }
      //for (int k=0; k<blocknodeVert.dimension(0); k++) {
      //  for (int i=0; i<numNodesPerElem; i++){
      //    for (int s=0; s<spaceDim; s++) {
      //      blocknodeVert(k,i,s) += blocknodePert(k,i,s);
      //    }
      //  }
      //}
    }
    //nodepert.push_back(blocknodePert);
  //}
  return blocknodePert;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void meshInterface::createCells(Teuchos::RCP<physics> & phys, vector<vector<Teuchos::RCP<cell> > > & cells) {
  
  ////////////////////////////////////////////////////////////////////////////////
  // Create the cells
  ////////////////////////////////////////////////////////////////////////////////
  
  int numElem = settings->sublist("Solver").get<int>("Workset size",1);
  vector<string> eBlocks;
  mesh->getElementBlockNames(eBlocks);
  
  MPI_Group world_comm; // Grab MPI_COMM_WORLD and place in world_comm
  //MPI_Comm_group(MPI_COMM_WORLD,&world_comm);
  MPI_Comm_group(Commptr->Comm(),&world_comm); //to work with stochastic collocation batching
  MPI_Comm LocalMPIComm;
  MPI_Group group;
  int mypid = Commptr->MyPID();
  MPI_Group_incl(world_comm, 1, &mypid, &group);
  //MPI_Comm_create(MPI_COMM_WORLD, group, &LocalMPIComm);
  MPI_Comm_create(Commptr->Comm(), group, &LocalMPIComm); //to work with stochastic collocation batching
  Teuchos::RCP<Epetra_MpiComm> LocalComm;
  LocalComm = Teuchos::rcp( new Epetra_MpiComm(LocalMPIComm) ); // Create Epetra_Comm
  for (size_t b=0; b<eBlocks.size(); b++) {
    vector<Teuchos::RCP<cell> > blockcells;
    vector<stk::mesh::Entity> stk_meshElems;
    mesh->getMyElements(eBlocks[b], stk_meshElems);
    topo_RCP cellTopo = mesh->getCellTopology(eBlocks[b]);
    size_t numElem = stk_meshElems.size();
    vector<size_t> localIds;
    DRV blocknodes;
    panzer_stk::workset_utils::getIdsAndVertices(*mesh, eBlocks[b], localIds, blocknodes);
    DRV blocknodepert = perturbMesh(b, blocknodes);
    //elemnodes.push_back(blocknodes);
    int numNodesPerElem = blocknodes.dimension(1);
    int elemPerCell = settings->sublist("Solver").get<int>("Workset size",1);
    bool memeff = settings->sublist("Solver").get<bool>("Memory Efficient",false);
    int prog = 0;
    while (prog < numElem) {
    //for (size_t e=0; e<numElem; e++) {
      int currElem = elemPerCell;  // Avoid faults in last iteration
      if (prog+currElem > numElem){
        currElem = numElem-prog;
      }
      Kokkos::View<int*> eIndex("element indices",currElem);
      DRV currnodes("currnodes", currElem, numNodesPerElem, spaceDim);
      DRV currnodepert("currnodepert", currElem, numNodesPerElem, spaceDim);
      for (int e=0; e<currElem; e++) {
        for (int n=0; n<numNodesPerElem; n++) {
          for (int m=0; m<spaceDim; m++) {
            currnodes(e,n,m) = blocknodes(prog+e,n,m) + blocknodepert(prog+e,n,m);
            currnodepert(e,n,m) = blocknodepert(prog+e,n,m);
          }
        }
        eIndex(e) = prog+e;
      }
      blockcells.push_back(Teuchos::rcp(new cell(settings, LocalComm, cellTopo, phys, currnodes, b, eIndex, 0, memeff)));
      blockcells[blockcells.size()-1]->nodepert = currnodepert;
      prog += elemPerCell;
    }
    cells.push_back(blockcells);
  }
  if (have_mesh_data) {
    this->importMeshData(cells);
  }
  else if (compute_mesh_data) {
    this->computeMeshData(cells);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void meshInterface::importMeshData(vector<vector<Teuchos::RCP<cell> > > & cells) {
  Teuchos::Time meshimporttimer("mesh import", false);
  meshimporttimer.start();
  
  int numdata = 0;
  if (have_rotations) {
    numdata = 9;
  }
  else if (have_rotation_phi) {
    numdata = 3;
  }
  
  for (size_t b=0; b<cells.size(); b++) {
    for (size_t e=0; e<cells[b].size(); e++) {
      int numElem = cells[b][e]->numElem;
      Kokkos::View<double**,HostDevice> cell_data("cell_data",numElem,numdata);
      cells[b][e]->cell_data = cell_data;
      cells[b][e]->cell_data_distance = vector<double>(numElem);
      cells[b][e]->cell_data_seed = vector<size_t>(numElem);
      cells[b][e]->cell_data_seedindex = vector<size_t>(numElem);
    }
  }
  
  for (int p=0; p<number_mesh_data_files; p++) {
    
    if (verbosity>5) {
      cout << Commptr->MyPID() << "  " << p << endl;
    }
    
    Teuchos::RCP<data> mesh_data;
    
    string mesh_data_pts_file;
    string mesh_data_file;
    
    if (have_multiple_data_files) {
      stringstream ss;
      ss << p+1;
      mesh_data_pts_file = mesh_data_pts_tag + "." + ss.str() + ".dat";
      mesh_data_file = mesh_data_tag + "." + ss.str() + ".dat";
    }
    else {
      mesh_data_pts_file = mesh_data_pts_tag + ".dat";
      mesh_data_file = mesh_data_tag + ".dat";
    }
    
    mesh_data = Teuchos::rcp(new data("mesh data", spaceDim, mesh_data_pts_file,
                                      mesh_data_file, false));
    
    for (size_t b=0; b<cells.size(); b++) {
      for (size_t e=0; e<cells[b].size(); e++) {
        DRV nodes = cells[b][e]->nodes;
        int numElem = cells[b][e]->numElem;
        
        for (int c=0; c<numElem; c++) {
          Kokkos::View<double[1][3],HostDevice> center("center");
          for (size_t i=0; i<nodes.dimension(1); i++) {
            for (size_t j=0; j<spaceDim; j++) {
              center(0,j) += nodes(c,i,j)/(double)nodes.dimension(1);
            }
          }
          double distance = 0.0;
          
          int cnode = mesh_data->findClosestNode(center(0,0), center(0,1), center(0,2), distance);
          bool iscloser = true;
          if (p>0){
            if (cells[b][e]->cell_data_distance[c] < distance) {
              iscloser = false;
            }
          }
          if (iscloser) {
            Kokkos::View<double**,HostDevice> cdata = mesh_data->getdata(cnode);
            for (int i=0; i<cdata.dimension(1); i++) {
              cells[b][e]->cell_data(c,i) = cdata(0,i);
            }
            if (have_rotations)
              cells[b][e]->have_cell_rotation = true;
            if (have_rotation_phi)
              cells[b][e]->have_cell_phi = true;
            
            cells[b][e]->cell_data_seed[c] = cnode;
            cells[b][e]->cell_data_seedindex[c] = cnode % 50;
            cells[b][e]->cell_data_distance[c] = distance;
          }
        }
      }
    }
    
  }
  
  meshimporttimer.stop();
  if (verbosity>5 && Commptr->MyPID() == 0) {
    cout << "mesh data import time: " << meshimporttimer.totalElapsedTime(false) << endl;
  }
  
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void meshInterface::computeMeshData(vector<vector<Teuchos::RCP<cell> > > & cells) {
  
  Teuchos::Time meshimporttimer("mesh import", false);
  meshimporttimer.start();
  
  have_rotations = true;
  have_rotation_phi = false;
  
  Kokkos::View<double**,HostDevice> seeds;
  int randSeed = settings->sublist("Mesh").get<int>("Random seed",1234);
  randomSeeds.push_back(randSeed);
  std::default_random_engine generator(randSeed);
  numSeeds = 0;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Generate the micro-structure using seeds and nearest neighbors
  ////////////////////////////////////////////////////////////////////////////////
  
  bool fast_and_crude = settings->sublist("Mesh").get<bool>("Fast and crude microstructure",false);
  
  if (fast_and_crude) {
    int numxSeeds = settings->sublist("Mesh").get<int>("Number of xseeds",10);
    int numySeeds = settings->sublist("Mesh").get<int>("Number of yseeds",10);
    int numzSeeds = settings->sublist("Mesh").get<int>("Number of zseeds",10);
    
    double xmin = settings->sublist("Mesh").get<double>("x min",0.0);
    double ymin = settings->sublist("Mesh").get<double>("y min",0.0);
    double zmin = settings->sublist("Mesh").get<double>("z min",0.0);
    double xmax = settings->sublist("Mesh").get<double>("x max",1.0);
    double ymax = settings->sublist("Mesh").get<double>("y max",1.0);
    double zmax = settings->sublist("Mesh").get<double>("z max",1.0);
    
    double dx = (xmax-xmin)/(double)(numxSeeds+1);
    double dy = (ymax-ymin)/(double)(numySeeds+1);
    double dz = (zmax-zmin)/(double)(numzSeeds+1);
    
    double maxpert = 0.25;
    
    Kokkos::View<double*,HostDevice> xseeds("xseeds",numxSeeds);
    Kokkos::View<double*,HostDevice> yseeds("yseeds",numySeeds);
    Kokkos::View<double*,HostDevice> zseeds("zseeds",numzSeeds);
    
    for (int k=0; k<numxSeeds; k++) {
      xseeds(k) = xmin + (k+1)*dx;
    }
    for (int k=0; k<numySeeds; k++) {
      yseeds(k) = ymin + (k+1)*dy;
    }
    for (int k=0; k<numzSeeds; k++) {
      zseeds(k) = zmin + (k+1)*dz;
    }
    
    std::uniform_real_distribution<double> pdistribution(-maxpert,maxpert);
    numSeeds = numxSeeds*numySeeds*numzSeeds;
    seeds = Kokkos::View<double**,HostDevice>("seeds",numSeeds,3);
    int prog = 0;
    for (int i=0; i<numxSeeds; i++) {
      for (int j=0; j<numySeeds; j++) {
        for (int k=0; k<numzSeeds; k++) {
          double xp = pdistribution(generator);
          double yp = pdistribution(generator);
          double zp = pdistribution(generator);
          seeds(prog,0) = xseeds(i) + xp*dx;
          seeds(prog,1) = yseeds(j) + yp*dy;
          seeds(prog,2) = zseeds(k) + zp*dz;
          prog += 1;
        }
      }
    }
  }
  else {
    numSeeds = settings->sublist("Mesh").get<int>("Number of seeds",1000);
    seeds = Kokkos::View<double**,HostDevice>("seeds",numSeeds,3);
    
    double xwt = settings->sublist("Mesh").get<double>("x weight",1.0);
    double ywt = settings->sublist("Mesh").get<double>("y weight",1.0);
    double zwt = settings->sublist("Mesh").get<double>("z weight",1.0);
    double nwt = sqrt(xwt*xwt+ywt*ywt+zwt*zwt);
    xwt *= 3.0/nwt;
    ywt *= 3.0/nwt;
    zwt *= 3.0/nwt;
    
    double xmin = settings->sublist("Mesh").get<double>("x min",0.0);
    double ymin = settings->sublist("Mesh").get<double>("y min",0.0);
    double zmin = settings->sublist("Mesh").get<double>("z min",0.0);
    double xmax = settings->sublist("Mesh").get<double>("x max",1.0);
    double ymax = settings->sublist("Mesh").get<double>("y max",1.0);
    double zmax = settings->sublist("Mesh").get<double>("z max",1.0);
    
    std::uniform_real_distribution<double> xdistribution(xmin,xmax);
    std::uniform_real_distribution<double> ydistribution(ymin,ymax);
    std::uniform_real_distribution<double> zdistribution(zmin,zmax);
    
    
    // we use a relatively crude algorithm to obtain well-spaced points
    int batch_size = 10;
    size_t prog = 0;
    Kokkos::View<double**,HostDevice> cseeds("cand seeds",batch_size,3);
    
    while (prog<numSeeds) {
      // fill in the candidate seeds
      for (int k=0; k<batch_size; k++) {
        double x = xdistribution(generator);
        cseeds(k,0) = x;
        double y = ydistribution(generator);
        cseeds(k,1) = y;
        double z = zdistribution(generator);
        cseeds(k,2) = z;
      }
      int bestpt = 0;
      if (prog > 0) { // for prog = 0, just take the first one
        double mindist = 1.0e6;
        for (int k=0; k<batch_size; k++) {
          double cmindist = 1.0e6;
          for (int j=0; j<prog; j++) {
            double dx = cseeds(k,0)-seeds(j,0);
            double dy = cseeds(k,1)-seeds(j,1);
            double dz = cseeds(k,2)-seeds(j,2);
            double cval = sqrt(xwt*dx*dx + ywt*dy*dy + zwt*dz*dz);
            if (cval < cmindist) {
              cmindist = cval;
            }
          }
          if (cmindist<mindist) {
            mindist = cmindist;
            bestpt = k;
          }
        }
      }
      for (int j=0; j<3; j++) {
        seeds(prog,j) = cseeds(bestpt,j);
      }
      prog += 1;
    }
  }
  //KokkosTools::print(seeds);
  
  std::uniform_int_distribution<int> idistribution(0,50);
  Kokkos::View<int*,HostDevice> seedIndex("seed index",numSeeds);
  for (int i=0; i<numSeeds; i++) {
    int ci = idistribution(generator);
    seedIndex(i) = ci;
  }
  
  //KokkosTools::print(seedIndex);
  
  ////////////////////////////////////////////////////////////////////////////////
  // Set seed data
  ////////////////////////////////////////////////////////////////////////////////
  
  int numdata = 9;
  
  std::normal_distribution<double> ndistribution(0.0,1.0);
  Kokkos::View<double**,HostDevice> rotation_data("cell_data",numSeeds,numdata);
  for (int k=0; k<numSeeds; k++) {
    double x = ndistribution(generator);
    double y = ndistribution(generator);
    double z = ndistribution(generator);
    double w = ndistribution(generator);
    
    double r = sqrt(x*x + y*y + z*z + w*w);
    x *= 1.0/r;
    y *= 1.0/r;
    z *= 1.0/r;
    w *= 1.0/r;
    
    rotation_data(k,0) = w*w + x*x - y*y - z*z;
    rotation_data(k,1) = 2.0*(x*y - w*z);
    rotation_data(k,2) = 2.0*(x*z + w*y);
    
    rotation_data(k,3) = 2.0*(x*y + w*z);
    rotation_data(k,4) = w*w - x*x + y*y - z*z;
    rotation_data(k,5) = 2.0*(y*z - w*x);
    
    rotation_data(k,6) = 2.0*(x*z - w*y);
    rotation_data(k,7) = 2.0*(y*z + w*x);
    rotation_data(k,8) = w*w - x*x - y*y + z*z;
    
  }
  
  //KokkosTools::print(rotation_data);
  
  ////////////////////////////////////////////////////////////////////////////////
  // Initialize cell data
  ////////////////////////////////////////////////////////////////////////////////
  
  
  for (size_t b=0; b<cells.size(); b++) {
    for (size_t e=0; e<cells[b].size(); e++) {
      int numElem = cells[b][e]->numElem;
      Kokkos::View<double**,HostDevice> cell_data("cell_data",numElem,numdata);
      cells[b][e]->cell_data = cell_data;
      cells[b][e]->cell_data_distance = vector<double>(numElem);
      cells[b][e]->cell_data_seed = vector<size_t>(numElem);
      cells[b][e]->cell_data_seedindex = vector<size_t>(numElem);
    }
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Set cell data
  ////////////////////////////////////////////////////////////////////////////////
  
  for (size_t b=0; b<cells.size(); b++) {
    for (size_t e=0; e<cells[b].size(); e++) {
      DRV nodes = cells[b][e]->nodes;
      int numElem = cells[b][e]->numElem;
      for (int c=0; c<numElem; c++) {
        Kokkos::View<double[1][3],HostDevice> center("center");
        for (size_t i=0; i<nodes.dimension(1); i++) {
          for (size_t j=0; j<spaceDim; j++) {
            center(0,j) += nodes(c,i,j)/(double)nodes.dimension(1);
          }
        }
        double distance = 1.0e6;
        int cnode = 0;
        for (int k=0; k<numSeeds; k++) {
          double dx = center(0,0)-seeds(k,0);
          double dy = center(0,1)-seeds(k,1);
          double dz = center(0,2)-seeds(k,2);
          double cdist = sqrt(dx*dx + dy*dy + dz*dz);
          if (cdist<distance) {
            cnode = k;
            distance = cdist;
          }
        }
        
        for (int i=0; i<9; i++) {
          cells[b][e]->cell_data(c,i) = rotation_data(cnode,i);
        }
        
        cells[b][e]->have_cell_rotation = true;
        cells[b][e]->have_cell_phi = false;
        
        cells[b][e]->cell_data_seed[c] = cnode;
        cells[b][e]->cell_data_seedindex[c] = seedIndex(cnode);
        cells[b][e]->cell_data_distance[c] = distance;
        
      }
    }
    
  }
  
  meshimporttimer.stop();
  if (verbosity>5 && Commptr->MyPID() == 0) {
    cout << "mesh data compute time: " << meshimporttimer.totalElapsedTime(false) << endl;
  }
  
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

DRV meshInterface::getElemNodes(const int & block, const int & elemID) {
  vector<size_t> localIds;
  DRV blocknodes;
  vector<string> eBlocks;
  mesh->getElementBlockNames(eBlocks);
  
  panzer_stk::workset_utils::getIdsAndVertices(*mesh, eBlocks[block], localIds, blocknodes);
  int nnodes = blocknodes.dimension(1);
  
  DRV cnodes("element nodes",1,nnodes,spaceDim);
  for (int i=0; i<nnodes; i++) {
    for (int j=0; j<spaceDim; j++) {
      cnodes(0,i,j) = blocknodes(elemID,i,j);
    }
  }
  return cnodes;
}
