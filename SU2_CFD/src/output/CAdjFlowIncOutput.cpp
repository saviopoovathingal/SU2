/*!
 * \file output_adj_flow_inc.cpp
 * \brief Main subroutines for flow discrete adjoint output
 * \author R. Sanchez
 * \version 6.2.0 "Falcon"
 *
 * The current SU2 release has been coordinated by the
 * SU2 International Developers Society <www.su2devsociety.org>
 * with selected contributions from the open-source community.
 *
 * The main research teams contributing to the current release are:
 *  - Prof. Juan J. Alonso's group at Stanford University.
 *  - Prof. Piero Colonna's group at Delft University of Technology.
 *  - Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *  - Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *  - Prof. Rafael Palacios' group at Imperial College London.
 *  - Prof. Vincent Terrapon's group at the University of Liege.
 *  - Prof. Edwin van der Weide's group at the University of Twente.
 *  - Lab. of New Concepts in Aeronautics at Tech. Institute of Aeronautics.
 *
 * Copyright 2012-2018, Francisco D. Palacios, Thomas D. Economon,
 *                      Tim Albring, and the SU2 contributors.
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../include/output/CAdjFlowIncOutput.hpp"

CAdjFlowIncOutput::CAdjFlowIncOutput(CConfig *config, CGeometry *geometry, unsigned short val_iZone) : COutput(config) {
  
  cont_adj = config->GetContinuous_Adjoint();
  
  nDim = geometry->GetnDim();
 
  turb_model = config->GetKind_Turb_Model();
  
  heat = config->GetEnergy_Equation();
  
  weakly_coupled_heat = config->GetWeakly_Coupled_Heat();
  
  /*--- Set the default history fields if nothing is set in the config file ---*/
  
  if (nRequestedHistoryFields == 0){
    RequestedHistoryFields.push_back("ITER");
    RequestedHistoryFields.push_back("RMS_RES");
    RequestedHistoryFields.push_back("SENSITIVITY");
    nRequestedHistoryFields = RequestedHistoryFields.size();
  }
  
  if (nRequestedScreenFields == 0){
    if (config->GetTime_Domain()) RequestedScreenFields.push_back("TIME_ITER");    
    if (multizone) RequestedScreenFields.push_back("OUTER_ITER");
    RequestedScreenFields.push_back("INNER_ITER");    
    RequestedScreenFields.push_back("RMS_ADJ_PRESSURE");
    RequestedScreenFields.push_back("RMS_ADJ_VELOCITY-X");
    RequestedScreenFields.push_back("SENS_GEO");
    RequestedScreenFields.push_back("SENS_AOA");
    nRequestedScreenFields = RequestedScreenFields.size();
  }
  
  if (nRequestedVolumeFields == 0){
    RequestedVolumeFields.push_back("COORDINATES");
    RequestedVolumeFields.push_back("SOLUTION");    
    RequestedVolumeFields.push_back("SENSITIVITY");
    nRequestedVolumeFields = RequestedVolumeFields.size();
  }
  
  stringstream ss;
  ss << "Zone " << config->GetiZone() << " (Adj. Incomp. Fluid)";
  MultiZoneHeaderString = ss.str();
  
  /*--- Set the volume filename --- */
  
  VolumeFilename = config->GetAdj_FileName();
  
  /*--- Set the surface filename --- */
  
  SurfaceFilename = config->GetSurfAdjCoeff_FileName();
  
  /*--- Set the restart filename --- */
  
  RestartFilename = config->GetRestart_AdjFileName();
  
  /*--- Add the obj. function extension --- */
  
  RestartFilename = config->GetObjFunc_Extension(RestartFilename);

  /*--- Set the default convergence field --- */

  if (Conv_Field.size() == 0 ) Conv_Field = "RMS_ADJ_PRESSURE";

}

CAdjFlowIncOutput::~CAdjFlowIncOutput(void) {

  if (rank == MASTER_NODE){
    HistFile.close();
  }

}

void CAdjFlowIncOutput::SetHistoryOutputFields(CConfig *config){
  
  /// BEGIN_GROUP: RMS_RES, DESCRIPTION: The root-mean-square residuals of the SOLUTION variables. 
  /// DESCRIPTION: Root-mean square residual of the adjoint Pressure.
  AddHistoryOutput("RMS_ADJ_PRESSURE",    "rms[A_P]",  FORMAT_FIXED, "RMS_RES", TYPE_RESIDUAL); 
  /// DESCRIPTION: Root-mean square residual of the adjoint Velocity x-component.
  AddHistoryOutput("RMS_ADJ_VELOCITY-X", "rms[A_U]", FORMAT_FIXED, "RMS_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the adjoint Velocity y-component.
  AddHistoryOutput("RMS_ADJ_VELOCITY-Y", "rms[A_V]", FORMAT_FIXED, "RMS_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the adjoint Velocity z-component.
  AddHistoryOutput("RMS_ADJ_VELOCITY-Z", "rms[A_W]", FORMAT_FIXED, "RMS_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the temperature.
  AddHistoryOutput("RMS_ADJ_HEAT", "rms[A_T]", FORMAT_FIXED, "RMS_RES", TYPE_RESIDUAL);
  if ((!config->GetFrozen_Visc_Disc() && !cont_adj) || (!config->GetFrozen_Visc_Cont() && cont_adj)){
    switch(turb_model){
    case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
      /// DESCRIPTION: Root-mean square residual of the adjoint nu tilde.
      AddHistoryOutput("RMS_ADJ_NU_TILDE", "rms[A_nu]", FORMAT_FIXED, "RMS_RES", TYPE_RESIDUAL);      
      break;  
    case SST:
      /// DESCRIPTION: Root-mean square residual of the adjoint kinetic energy.
      AddHistoryOutput("RMS_ADJ_TKE", "rms[A_k]", FORMAT_FIXED, "RMS_RES", TYPE_RESIDUAL); 
      /// DESCRIPTION: Root-mean square residual of the adjoint dissipation.
      AddHistoryOutput("RMS_ADJ_DISSIPATION",    "rms[A_w]", FORMAT_FIXED, "RMS_RES", TYPE_RESIDUAL);   
      break;
    default: break;
    }
  }
  /// END_GROUP
  
  /// BEGIN_GROUP: MAX_RES, DESCRIPTION: The maximum residuals of the SOLUTION variables. 
  /// DESCRIPTION: Maximum residual of the adjoint Pressure.
  AddHistoryOutput("MAX_ADJ_PRESSURE",    "max[A_Rho]",  FORMAT_FIXED, "MAX_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the adjoint Velocity x-component
  AddHistoryOutput("MAX_ADJ_VELOCITY-X", "max[A_RhoU]", FORMAT_FIXED, "MAX_RES", TYPE_RESIDUAL); 
  /// DESCRIPTION: Maximum residual of the adjoint Velocity y-component
  AddHistoryOutput("MAX_ADJ_VELOCITY-Y", "max[A_RhoV]", FORMAT_FIXED, "MAX_RES", TYPE_RESIDUAL); 
  /// DESCRIPTION: Maximum residual of the adjoint Velocity z-component
  AddHistoryOutput("MAX_ADJ_VELOCITY-Z", "max[A_RhoW]", FORMAT_FIXED, "MAX_RES", TYPE_RESIDUAL); 
  /// DESCRIPTION: Maximum residual of the temperature.
  AddHistoryOutput("MAX_ADJ_HEAT", "max[A_T]", FORMAT_FIXED, "MAX_RES", TYPE_RESIDUAL);
  if ((!config->GetFrozen_Visc_Disc() && !cont_adj) || (!config->GetFrozen_Visc_Cont() && cont_adj)){
    switch(turb_model){
    case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
      /// DESCRIPTION: Maximum residual of the adjoint nu tilde.
      AddHistoryOutput("MAX_ADJ_NU_TILDE", "max[A_nu]", FORMAT_FIXED, "MAX_RES", TYPE_RESIDUAL);      
      break;  
    case SST:
      /// DESCRIPTION: Maximum residual of the adjoint kinetic energy.
      AddHistoryOutput("MAX_ADJ_TKE", "max[A_k]", FORMAT_FIXED, "MAX_RES", TYPE_RESIDUAL);   
      /// DESCRIPTION: Maximum residual of the adjoint dissipation.
      AddHistoryOutput("MAX_ADJ_DISSIPATION",    "max[A_w]", FORMAT_FIXED, "MAX_RES", TYPE_RESIDUAL); 
      break;
    default: break;
    }
  }
  /// END_GROUP
  
  /// BEGIN_GROUP: BGS_RES, DESCRIPTION: The block Gauss Seidel residuals of the SOLUTION variables. 
  /// DESCRIPTION: BGSimum residual of the adjoint Pressure.
  AddHistoryOutput("BGS_ADJ_PRESSURE",    "bgs[A_Rho]",  FORMAT_FIXED, "BGS_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: BGSimum residual of the adjoint Velocity x-component
  AddHistoryOutput("BGS_ADJ_VELOCITY-X", "bsg[A_RhoU]", FORMAT_FIXED, "BGS_RES", TYPE_RESIDUAL); 
  /// DESCRIPTION: BGSimum residual of the adjoint Velocity y-component
  AddHistoryOutput("BGS_ADJ_VELOCITY-Y", "bgs[A_RhoV]", FORMAT_FIXED, "BGS_RES", TYPE_RESIDUAL); 
  /// DESCRIPTION: BGSimum residual of the adjoint Velocity z-component
  AddHistoryOutput("BGS_ADJ_VELOCITY-Z", "bgs[A_RhoW]", FORMAT_FIXED, "BGS_RES", TYPE_RESIDUAL); 
  /// DESCRIPTION: BGSimum residual of the temperature.
  AddHistoryOutput("BGS_ADJ_HEAT", "bgs[A_T]", FORMAT_FIXED, "BGS_RES", TYPE_RESIDUAL);
  if ((!config->GetFrozen_Visc_Disc() && !cont_adj) || (!config->GetFrozen_Visc_Cont() && cont_adj)){
    switch(turb_model){
    case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
      /// DESCRIPTION: BGSimum residual of the adjoint nu tilde.
      AddHistoryOutput("BGS_ADJ_NU_TILDE", "bsg[A_nu]", FORMAT_FIXED, "BGS_RES", TYPE_RESIDUAL);      
      break;  
    case SST:
      /// DESCRIPTION: BGSimum residual of the adjoint kinetic energy.
      AddHistoryOutput("BGS_ADJ_TKE", "bgs[A_k]", FORMAT_FIXED, "BGS_RES", TYPE_RESIDUAL);   
      /// DESCRIPTION: BGSimum residual of the adjoint dissipation.
      AddHistoryOutput("BGS_ADJ_DISSIPATION",    "bgs[A_w]", FORMAT_FIXED, "BGS_RES", TYPE_RESIDUAL); 
      break;
    default: break;
    }
  }
  /// END_GROUP
  
  
  /// BEGIN_GROUP: SENSITIVITY, DESCRIPTION: Sensitivities of different geometrical or boundary values.   
  /// DESCRIPTION: Sum of the geometrical sensitivities on all markers set in MARKER_MONITORING.
  AddHistoryOutput("SENS_GEO",   "Sens_Geo",   FORMAT_SCIENTIFIC, "SENSITIVITY", TYPE_COEFFICIENT); 
  /// DESCRIPTION: Sensitivity of the objective function with respect to the angle of attack (only for compressible solver).
  AddHistoryOutput("SENS_AOA",   "Sens_AoA",   FORMAT_SCIENTIFIC, "SENSITIVITY", TYPE_COEFFICIENT); 
  /// DESCRIPTION: Sensitivity of the objective function with respect to the Mach number (only of compressible solver).
  AddHistoryOutput("SENS_MACH",  "Sens_Mach",  FORMAT_SCIENTIFIC, "SENSITIVITY", TYPE_COEFFICIENT); 
  /// DESCRIPTION: Sensitivity of the objective function with respect to the far-field pressure.
  AddHistoryOutput("SENS_PRESS", "Sens_Press", FORMAT_SCIENTIFIC, "SENSITIVITY", TYPE_COEFFICIENT); 
  /// DESCRIPTION: Sensitivity of the objective function with respect to the far-field temperature.
  AddHistoryOutput("SENS_TEMP",  "Sens_Temp",  FORMAT_SCIENTIFIC, "SENSITIVITY", TYPE_COEFFICIENT); 
  /// DESCRIPTION: Sensitivity of the objective function with respect to the inlet velocity.
  AddHistoryOutput("SENS_VEL_IN", "Sens_Vin", FORMAT_SCIENTIFIC, "SENSITIVITY", TYPE_COEFFICIENT); 
  /// DESCRIPTION: Sensitivity of the objective function with respect to the outlet pressure.
  AddHistoryOutput("SENS_PRESS_OUT",  "Sens_Pout",  FORMAT_SCIENTIFIC, "SENSITIVITY", TYPE_COEFFICIENT); 
  /// END_GROUP
  
}

void CAdjFlowIncOutput::LoadHistoryData(CConfig *config, CGeometry *geometry, CSolver **solver) { 
  
  CSolver* adjflow_solver = solver[ADJFLOW_SOL];
  CSolver* adjturb_solver = solver[ADJTURB_SOL];  
  CSolver* adjheat_solver = solver[ADJHEAT_SOL];
  
  SetHistoryOutputValue("TIME_ITER",  curr_TimeIter);  
  SetHistoryOutputValue("INNER_ITER", curr_InnerIter);
  SetHistoryOutputValue("OUTER_ITER", curr_OuterIter); 
  
  SetHistoryOutputValue("RMS_ADJ_PRESSURE", log10(adjflow_solver->GetRes_RMS(0)));
  SetHistoryOutputValue("RMS_ADJ_VELOCITY-X", log10(adjflow_solver->GetRes_RMS(1)));
  SetHistoryOutputValue("RMS_ADJ_VELOCITY-Y", log10(adjflow_solver->GetRes_RMS(2)));
  if (nDim == 3) {
    SetHistoryOutputValue("RMS_ADJ_VELOCITY-Z", log10(adjflow_solver->GetRes_RMS(3)));
  }
  if (weakly_coupled_heat){
    SetHistoryOutputValue("RMS_ADJ_HEAT",         log10(adjheat_solver->GetRes_RMS(0)));
  }
  if (heat){
    if (nDim == 3) SetHistoryOutputValue("RMS_ADJ_HEAT",         log10(adjflow_solver->GetRes_RMS(4)));
    else           SetHistoryOutputValue("RMS_ADJ_HEAT",         log10(adjflow_solver->GetRes_RMS(3)));
  }
  if ((!config->GetFrozen_Visc_Disc() && !cont_adj) || (!config->GetFrozen_Visc_Cont() && cont_adj)){
    switch(turb_model){
    case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
      SetHistoryOutputValue("RMS_ADJ_NU_TILDE", log10(adjturb_solver->GetRes_RMS(0)));
      break;  
    case SST:
      SetHistoryOutputValue("RMS_ADJ_TKE", log10(adjturb_solver->GetRes_RMS(0)));
      SetHistoryOutputValue("RMS_ADJ_DISSIPATION",    log10(adjturb_solver->GetRes_RMS(1)));
      break;
    default: break;
    }
  }
  SetHistoryOutputValue("MAX_ADJ_PRESSURE", log10(adjflow_solver->GetRes_Max(0)));
  SetHistoryOutputValue("MAX_ADJ_VELOCITY-X", log10(adjflow_solver->GetRes_Max(1)));
  SetHistoryOutputValue("MAX_ADJ_VELOCITY-Y", log10(adjflow_solver->GetRes_Max(2)));
  if (nDim == 3) {
    SetHistoryOutputValue("MAX_ADJ_VELOCITY-Z", log10(adjflow_solver->GetRes_Max(3)));
  }
  if (weakly_coupled_heat){
    SetHistoryOutputValue("MAX_ADJ_HEAT",         log10(adjheat_solver->GetRes_Max(0)));
  }
  if (heat){
    if (nDim == 3) SetHistoryOutputValue("MAX_ADJ_HEAT",         log10(adjflow_solver->GetRes_Max(4)));
    else           SetHistoryOutputValue("MAX_ADJ_HEAT",         log10(adjflow_solver->GetRes_Max(3)));
  }
  if ((!config->GetFrozen_Visc_Disc() && !cont_adj) || (!config->GetFrozen_Visc_Cont() && cont_adj)){
    switch(turb_model){
    case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
      SetHistoryOutputValue("MAX_ADJ_NU_TILDE", log10(adjturb_solver->GetRes_Max(0)));
      break;  
    case SST:
      SetHistoryOutputValue("MAX_ADJ_TKE", log10(adjturb_solver->GetRes_Max(0)));
      SetHistoryOutputValue("MAX_ADJ_DISSIPATION",    log10(adjturb_solver->GetRes_Max(1)));
      break;
    default: break;
    }
  }
  
  if (multizone){
    SetHistoryOutputValue("BGS_ADJ_PRESSURE", log10(adjflow_solver->GetRes_BGS(0)));
    SetHistoryOutputValue("BGS_ADJ_VELOCITY-X", log10(adjflow_solver->GetRes_BGS(1)));
    SetHistoryOutputValue("BGS_ADJ_VELOCITY-Y", log10(adjflow_solver->GetRes_BGS(2)));
    if (nDim == 3) {
      SetHistoryOutputValue("BGS_ADJ_VELOCITY-Z", log10(adjflow_solver->GetRes_BGS(3)));
    }
    if (weakly_coupled_heat){
      SetHistoryOutputValue("BGS_ADJ_HEAT",         log10(adjheat_solver->GetRes_BGS(0)));
    }
    if (heat){
      if (nDim == 3) SetHistoryOutputValue("BGS_ADJ_HEAT",         log10(adjflow_solver->GetRes_BGS(4)));
      else           SetHistoryOutputValue("BGS_ADJ_HEAT",         log10(adjflow_solver->GetRes_BGS(3)));
    }
    if ((!config->GetFrozen_Visc_Disc() && !cont_adj) || (!config->GetFrozen_Visc_Cont() && cont_adj)){
      switch(turb_model){
      case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
        SetHistoryOutputValue("BGS_ADJ_NU_TILDE", log10(adjturb_solver->GetRes_BGS(0)));
        break;  
      case SST:
        SetHistoryOutputValue("BGS_ADJ_TKE", log10(adjturb_solver->GetRes_BGS(0)));
        SetHistoryOutputValue("BGS_ADJOINT_DISSIPATION",    log10(adjturb_solver->GetRes_BGS(1)));
        break;
      default: break;
      }
    }
  }
  
  SetHistoryOutputValue("SENS_GEO", adjflow_solver->GetTotal_Sens_Geo());
  SetHistoryOutputValue("SENS_AOA", adjflow_solver->GetTotal_Sens_AoA());
  SetHistoryOutputValue("SENS_MACH", adjflow_solver->GetTotal_Sens_Mach());
  SetHistoryOutputValue("SENS_PRESS", adjflow_solver->GetTotal_Sens_Press());
  SetHistoryOutputValue("SENS_TEMP", adjflow_solver->GetTotal_Sens_Temp());
  SetHistoryOutputValue("SENS_VEL_IN", adjflow_solver->GetTotal_Sens_ModVel());
  SetHistoryOutputValue("SENS_PRESS_OUT", adjflow_solver->GetTotal_Sens_BPress());

}

void CAdjFlowIncOutput::SetVolumeOutputFields(CConfig *config){
  
  /// BEGIN_GROUP: COORDINATES, DESCRIPTION: Coordinates of the mesh nodes.
  /// DESCRIPTION: x coordinates of the mesh nodes.
  AddVolumeOutput("COORD-X", "x", "COORDINATES"); 
  /// DESCRIPTION: y coordinates of the mesh nodes.
  AddVolumeOutput("COORD-Y", "y", "COORDINATES");
  if (nDim == 3)
    /// DESCRIPTION: z coordinates of the mesh nodes.
    AddVolumeOutput("COORD-Z", "z", "COORDINATES");
  /// END_GROUP
  
  /// BEGIN_GROUP: SOLUTION, DESCRIPTION: The SOLUTION variables of the adjoint solver.
  /// DESCRIPTION: Adjoint Pressure.
  AddVolumeOutput("ADJ_PRESSURE",    "Adjoint_Pressure",    "SOLUTION"); 
  /// DESCRIPTION: Adjoint Velocity x-component.
  AddVolumeOutput("ADJ_VELOCITY-X", "Adjoint_Velocity_x", "SOLUTION"); 
  /// DESCRIPTION: Adjoint Velocity y-component.
  AddVolumeOutput("ADJ_VELOCITY-Y", "Adjoint_Velocity_y", "SOLUTION"); 
  if (nDim == 3)
    /// DESCRIPTION: Adjoint Velocity z-component.
    AddVolumeOutput("ADJ_VELOCITY-Z", "Adjoint_Velocity_z", "SOLUTION"); 
 
  if (weakly_coupled_heat || heat){
    AddVolumeOutput("ADJ_HEAT", "Adjoint_Heat", "SOLUTION");
  }

  if (!config->GetFrozen_Visc_Disc()){    
    switch(turb_model){
    case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
      /// DESCRIPTION: Adjoint nu tilde.
      AddVolumeOutput("ADJ_NU_TILDE", "Adjoint_Nu_Tilde", "SOLUTION"); 
      break;  
    case SST:
      /// DESCRIPTION: Adjoint kinetic energy.
      AddVolumeOutput("ADJ_TKE", "Adjoint_TKE", "SOLUTION"); 
      /// DESCRIPTION: Adjoint dissipation.
      AddVolumeOutput("ADJ_DISSIPATION", "Adjoint_Omega", "SOLUTION");  
      break;
    default: break;
    }
  }
  /// END_GROUP
  
  /// BEGIN_GROUP: GRID_VELOCITY, DESCRIPTION: The grid velocity in case of a moving grid.  
  if (config->GetGrid_Movement()){
    /// DESCRIPTION: Grid velocity x-component.
    AddVolumeOutput("GRID_VELOCITY-X", "Grid_Velocity_x", "GRID_VELOCITY"); 
    /// DESCRIPTION: Grid velocity y-component.
    AddVolumeOutput("GRID_VELOCITY-Y", "Grid_Velocity_y", "GRID_VELOCITY"); 
    if (nDim == 3)    
      /// DESCRIPTION: Grid velocity z-component.
      AddVolumeOutput("GRID_VELOCITY-Z", "Grid_Velocity_z", "GRID_VELOCITY"); 
  }
  /// END_GROUP
  
  /// BEGIN_GROUP: RESIDUAL, DESCRIPTION: Residuals of the SOLUTION variables. 
  /// DESCRIPTION: Residual of the adjoint Pressure.
  AddVolumeOutput("RES_ADJ_PRESSURE",    "Residual_Adjoint_Pressure",    "RESIDUAL");  
  /// DESCRIPTION: Residual of the adjoint Velocity x-component.
  AddVolumeOutput("RES_ADJ_VELOCITY-X", "Residual_Adjoint_Velocity_x", "RESIDUAL");  
  /// DESCRIPTION: Residual of the adjoint Velocity y-component.
  AddVolumeOutput("RES_ADJ_VELOCITY-Y", "Residual_Adjoint_Velocity_y", "RESIDUAL");  
  if (nDim == 3)
    /// DESCRIPTION: Residual of the adjoint Velocity z-component.
    AddVolumeOutput("RES_ADJ_Velocity-Z", "Residual_Adjoint_Velocity_z", "RESIDUAL"); 
  /// DESCRIPTION: Residual of the adjoint energy. 
  AddVolumeOutput("RES_ADJ_ENERGY", "Residual_Adjoint_Energy", "RESIDUAL");            
  if (!config->GetFrozen_Visc_Disc()){      
    switch(turb_model){
    case SA: case SA_NEG: case SA_E: case SA_COMP: case SA_E_COMP:
      /// DESCRIPTION: Residual of the nu tilde. 
      AddVolumeOutput("RES_ADJ_NU_TILDE", "Residual_Adjoint_Nu_Tilde", "RESIDUAL"); 
      break;  
    case SST:
      /// DESCRIPTION: Residual of the adjoint kinetic energy. 
      AddVolumeOutput("RES_ADJ_TKE", "Residual_Adjoint_TKE", "RESIDUAL");    
      /// DESCRIPTION: Residual of the adjoint dissipation.
      AddVolumeOutput("RES_ADJ_DISSIPATION", "Residual_Adjoint_Omega", "RESIDUAL");
      break;
    default: break;
    }
  }
  /// END_GROUP
  
  /// BEGIN_GROUP: SENSITIVITY, DESCRIPTION: Geometrical sensitivities of the current objective function.
  /// DESCRIPTION: Sensitivity x-component.
  AddVolumeOutput("SENSITIVITY_X", "Sensitivity_x", "SENSITIVITY"); 
  /// DESCRIPTION: Sensitivity y-component.
  AddVolumeOutput("SENSITIVITY_Y", "Sensitivity_y", "SENSITIVITY");
  if (nDim == 3)
    /// DESCRIPTION: Sensitivity z-component.
    AddVolumeOutput("SENSITIVITY_Z", "Sensitivity_z", "SENSITIVITY");   
  /// DESCRIPTION: Sensitivity in normal direction.
  AddVolumeOutput("SENSITIVITY", "Surface_Sensitivity", "SENSITIVITY"); 
  /// END_GROUP
 
}

void CAdjFlowIncOutput::LoadVolumeData(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned long iPoint){
  
  CVariable* Node_AdjFlow = solver[ADJFLOW_SOL]->node[iPoint]; 
  CVariable* Node_AdjHeat = NULL;
  CVariable* Node_AdjTurb = NULL;
  CPoint*    Node_Geo     = geometry->node[iPoint];
  
  if (config->GetKind_Turb_Model() != NONE && !config->GetFrozen_Visc_Disc()){
    Node_AdjTurb = solver[ADJTURB_SOL]->node[iPoint]; 
  }
  if (weakly_coupled_heat){
    Node_AdjHeat = solver[ADJHEAT_SOL]->node[iPoint];
  }
  
  SetVolumeOutputValue("COORD-X", iPoint,  Node_Geo->GetCoord(0));  
  SetVolumeOutputValue("COORD-Y", iPoint,  Node_Geo->GetCoord(1));
  if (nDim == 3)
    SetVolumeOutputValue("COORD-Z", iPoint, Node_Geo->GetCoord(2));
  
  SetVolumeOutputValue("ADJ_PRESSURE",    iPoint, Node_AdjFlow->GetSolution(0));
  SetVolumeOutputValue("ADJ_VELOCITY-X", iPoint, Node_AdjFlow->GetSolution(1));
  SetVolumeOutputValue("ADJ_VELOCITY-Y", iPoint, Node_AdjFlow->GetSolution(2));
  if (nDim == 3){
    SetVolumeOutputValue("ADJ_VELOCITY-Z", iPoint, Node_AdjFlow->GetSolution(3));
  }

  if (weakly_coupled_heat){
    SetVolumeOutputValue("ADJ_HEAT", iPoint, Node_AdjHeat->GetSolution(0));
  }
  if (heat){
    if (nDim == 3) SetVolumeOutputValue("ADJ_HEAT", iPoint, Node_AdjFlow->GetSolution(4));
    else           SetVolumeOutputValue("ADJ_HEAT", iPoint, Node_AdjFlow->GetSolution(3));
  }
  // Turbulent 
  if (!config->GetFrozen_Visc_Disc()){         
    switch(turb_model){
    case SST:
      SetVolumeOutputValue("ADJ_TKE", iPoint, Node_AdjTurb->GetSolution(0));
      SetVolumeOutputValue("ADJ_DISSIPATION", iPoint, Node_AdjTurb->GetSolution(1));
      break;
    case SA: case SA_COMP: case SA_E: 
    case SA_E_COMP: case SA_NEG: 
      SetVolumeOutputValue("ADJ_NU_TILDE", iPoint, Node_AdjTurb->GetSolution(0));
      break;
    case NONE:
      break;
    }
  }
  
  // Residuals
  SetVolumeOutputValue("RES_ADJ_PRESSURE", iPoint, Node_AdjFlow->GetSolution(0) - Node_AdjFlow->GetSolution_Old(0));
  SetVolumeOutputValue("RES_ADJ_VELOCITY-X", iPoint, Node_AdjFlow->GetSolution(1) - Node_AdjFlow->GetSolution_Old(1));
  SetVolumeOutputValue("RES_ADJ_VELOCITY-Y", iPoint, Node_AdjFlow->GetSolution(2) - Node_AdjFlow->GetSolution_Old(2));
  if (nDim == 3){
    SetVolumeOutputValue("RES_ADJ_VELOCITY-Z", iPoint, Node_AdjFlow->GetSolution(3) - Node_AdjFlow->GetSolution_Old(3));
  } 
  if (!config->GetFrozen_Visc_Disc()){            
    switch(config->GetKind_Turb_Model()){
    case SST:
      SetVolumeOutputValue("RES_ADJ_TKE", iPoint, Node_AdjTurb->GetSolution(0) - Node_AdjTurb->GetSolution_Old(0));
      SetVolumeOutputValue("RES_ADJ_DISSIPATION", iPoint, Node_AdjTurb->GetSolution(1) - Node_AdjTurb->GetSolution_Old(1));
      break;
    case SA: case SA_COMP: case SA_E: 
    case SA_E_COMP: case SA_NEG: 
      SetVolumeOutputValue("RES_ADJ_NU_TILDE", iPoint, Node_AdjTurb->GetSolution(0) - Node_AdjTurb->GetSolution_Old(0));
      break;
    case NONE:
      break;
    }
  }
  
  SetVolumeOutputValue("SENSITIVITY_X", iPoint, Node_AdjFlow->GetSensitivity(0));
  SetVolumeOutputValue("SENSITIVITY_Y", iPoint, Node_AdjFlow->GetSensitivity(1));
  if (nDim == 3)
    SetVolumeOutputValue("SENSITIVITY_Z", iPoint, Node_AdjFlow->GetSensitivity(2));
  
}

void CAdjFlowIncOutput::LoadSurfaceData(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned long iPoint, unsigned short iMarker, unsigned long iVertex){
  
  SetVolumeOutputValue("SENSITIVITY", iPoint, solver[ADJFLOW_SOL]->GetCSensitivity(iMarker, iVertex));
  
}


bool CAdjFlowIncOutput::SetInit_Residuals(CConfig *config){
  
  return (config->GetUnsteady_Simulation() != STEADY && (config->GetIntIter() == 0))|| 
        (config->GetUnsteady_Simulation() == STEADY && (config->GetExtIter() < 2)); 
  
}

bool CAdjFlowIncOutput::SetUpdate_Averages(CConfig *config){
  return false;
  
//  return (config->GetUnsteady_Simulation() != STEADY && !dualtime);
      
}
