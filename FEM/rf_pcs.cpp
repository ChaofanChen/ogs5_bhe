/**************************************************************************
ROCKFLOW - Object: Process PCS
Task: 
Programing:
02/2003 OK Implementation
  /2003 WW CRFProcessDeformation
11/2003 OK re-organized
07/2004 OK PCS2
02/2005 WW/OK Element Assemblier and output 
**************************************************************************/

/*--------------------- MPI Parallel  -------------------*/
#ifdef USE_MPI
#include <mpi.h>
#endif
/*--------------------- MPI Parallel  -------------------*/

// MFC
#include "stdafx.h" 
#ifdef MFC
#include "afxpriv.h" // For WM_SETMESSAGESTRING
#endif
#include "makros.h"
// C
#include <malloc.h>
// C++
#include <iostream>
//#include <algorithm> // header of transform. WW
// GEOLib
#include "geo_ply.h"
/*------------------------------------------------------------------------*/
/* MshLib */
#include "msh_nodes_rfi.h" //MB
#include "msh_elem.h"
#include "msh_lib.h"
/*-----------------------------------------------------------------------*/
/* Objects */
#include "rf_pcs.h"
#include "pcs_dm.h"
#include "files.h"    // FIL
#include "file_rfr.h" // FIL
#include "solver.h"    // ConfigRenumberProperties
#include "rf_st_new.h" // ST
#include "rf_bc_new.h" // ST
#include "rf_mmp_new.h" // MAT
#include "rf_ic_new.h"  // IC
#include "rf_tim_new.h"  // IC
#include "rfiter.h"   // ITE
#include "elements.h" // ELE
#include "fem_ele_std.h" // ELE
#include "msh_lib.h" // ELE
#include "nodes.h" 
#include "rf_tim_new.h"
#include "rf_out_new.h"
#include "rfmat_cp.h"
#include "rf_mfp_new.h" // MFP
#include "rf_num_new.h"
#include "gs_project.h"
#include "rf_fct.h"
/*-----------------------------------------------------------------------*/
/* Tools */
#include "matrix.h"
#include "mathlib.h"
#include "geo_strings.h"
#include "par_ddc.h"
#include "tools.h"
#include "rf_pcs.h"
#include "rfstring.h"
/*-------------------- ITPACKV    ---------------------------*/
extern void transM2toM6(void);
/*-------------------- ITPACKV    ---------------------------*/
/*-------------------- JAD    ---------------------------*/
extern void transM2toM5(void);
/*-------------------- JAD    ---------------------------*/
/*-----------------------------------------------------------------------*/
/* LOP */
#include "rf_apl.h" // Loop...
#include "loop_pcs.h"
extern VoidFuncVoid LOPCalcSecondaryVariables_USER;
/*-----------------------------------------------------------------------*/
/* Kernels */
#include "int_asm.h"
#include "cel_mpc.h"
#include "cgs_mpc.h"
#include "cgs_mmp.h"
#include "int_mmp.h"
#include "int_mms.h"
#include "cel_mtm2.h"
#include "cgs_mtm2.h"
#include "cel_agm.h"
#include "cgs_agm.h"
//------------------------------------------------------------------------
// PCS
VoidXFuncVoidX PCSDestroyELEMatrices[PCS_NUMBER_MAX];
void PCSConfigELEMatricesMTM(int);
void PCSConfigELEMatricesMPC(int);
void PCSConfigELEMatricesSM(int);
//void PCSConfigELEMatricesHTM(int);
void PCSConfigELEMatricesMMP(int);
//------------------------------------------------------------------------
// Globals, to be checked
int pcs_no_fluid_phases = 0;
int pcs_no_components = 0;
bool pcs_monolithic_flow = false;
int dm_pcs_number = 0;
int pcs_deformation = 0;
int dm_number_of_primary_nvals = 2; 
bool show_onces_adp = true;
bool show_onces_mod = true;
bool show_onces_mod_flow = true;
bool show_onces_density = true;
int memory_opt = 0;
int problem_2d_plane_dm; 
int anz_nval = 0;
int anz_nval0 = 0; //WW
NvalInfo *nval_data = NULL;
int anz_eval = 0;
EvalInfo *eval_data = NULL;
string project_title("New project"); //OK41

//--------------------------------------------------------
// Coupling Flag. WW
bool T_Process = false;
bool H_Process = false;
bool M_Process = false;
bool RD_Process = false;
bool MH_Process = false; // MH monolithic scheme
bool MASS_TRANSPORT_Process = false;
bool FLUID_MOMENTUM_Process = false;
bool RANDOM_WALK_Process = false;
bool pcs_created = false;

namespace process{class CRFProcessDeformation;}
using process::CRFProcessDeformation;
using Mesh_Group::CNode;
using Mesh_Group::CElem;
using FiniteElement::ElementValue;
using Math_Group::vec;

#define noCHECK_EQS
#define noCHECK_ST_GROUP
#define noCHECK_BC_GROUP

/*************************************************************************
PCS2 - File structure
// Construction/destruction
CRFProcess::CRFProcess(void)
CRFProcess::~CRFProcess(void)
void CRFProcess::Create()
void PCSDestroyAllProcesses(void)
void PCSRead(string file_base_name)
ios::pos_type CRFProcess::Read(ifstream *pcs_file)
// Access to PCS
CRFProcess *CRFProcess::Get(string name)
CRFProcess *CRFProcess::GetProcessByFunctionName(char *name)
CRFProcess *CRFProcess::GetProcessByNumber(int number)
// Configuration 1 - NOD
void CRFProcess::Config(void)
void CRFProcess::ConfigNODValues(void)
void CRFProcess::CreateNODValues(void)
// Configuration 2 - ELE
void CRFProcess::ConfigELEValues(void)
void CRFProcess::CreateELEValues(void)
void CRFProcess::CreateELEGPValues(void)
// Configuration 3 - ELE matrices
void CRFProcess::CreateELEMatricesPointer(void)
??? void PCSConfigELEMatricesXXX(int pcs_type_number)
// Execution
double CRFProcess::Execute()
void CRFProcess::InitEQS()
void CRFProcess::CalculateElementMatrices(void) 
void CRFProcess::DomainDecomposition()
void CRFProcess::AssembleSystemMatrixNew(void) 
void CRFProcess::IncorporateBoundaryConditions(const double Scaling)
void CRFProcess::IncorporateSourceTerms(const double Scaling)
int CRFProcess::ExecuteLinearSolver(void)
// Specials
void PCSRestart()
void RelocateDeformationProcess(CRFProcess *m_pcs)
void CRFProcess::CreateFDMProcess()
void CRFProcess::PCSMoveNOD(void) 
string PCSProblemType()
// ReMove site
*************************************************************************/

//////////////////////////////////////////////////////////////////////////
// PCS vector
//////////////////////////////////////////////////////////////////////////
// It is better to have space between data type and data name. WW
vector<LINEAR_SOLVER *> PCS_Solver; //WW
vector<CRFProcess*> pcs_vector;
vector<double*> ele_val_vector; //PCH
vector<string> ele_val_name_vector; // PCH
template <class T> T *resize(T *array, size_t old_size, size_t new_size);
//////////////////////////////////////////////////////////////////////////
// Construction / destruction
//////////////////////////////////////////////////////////////////////////

/**************************************************************************
FEMLib-Method:
Task:
Programing:
03/2003 OK Implementation
02/2005 WW Local elment assembly (all protected members)
last modified:
**************************************************************************/
CRFProcess::CRFProcess(void) 
           :fem(NULL), Write_Matrix(false), matrix_file(NULL), 
            Memory_Type(0), WriteSourceNBC_RHS(0) //WW  //WW
{
  TempArry = NULL;
//SB:GS4  pcs_component_number=0; //SB: counter for transport components
  pcs_component_number = pcs_no_components -1;
  //----------------------------------------------------------------------
  // NUM
  pcs_num_name[0] = NULL;
  pcs_num_name[1] = NULL;
  pcs_nonlinear_iterations = 1;
  pcs_nonlinear_iteration_tolerance = 1.0e8;
  pcs_coupling_iterations = 1;
  pcs_sol_name = NULL;
  m_num = NULL;
  cpl_type_name = "PARTITIONED"; //OK
  num_type_name = "FEM"; //OK
  eqs = NULL; //WW
  //----------------------------------------------------------------------
  // ELE
  ConfigELEMatrices = NULL;
  pcs_number_of_evals = 0;
  NumDeactivated_SubDomains = 0;
  //----------------------------------------------------------------------
  // 
  mobile_nodes_flag = -1;
  //----------------------------------------------------------------------
  // USER
  PCSSetIC_USER = NULL;
  //----------------------------------------------------------------------
  // TIM
  tim_type_name = "TRANSIENT"; //OK
  time_unit_factor = 1.0;
  //pcs_type_name.empty();
  //----------------------------------------------------------------------
  // CPL
  for(int i=0; i<10; i++)
    Shift[i] = 0;
  selected = true; //OK
  // MSH OK
  m_msh = NULL;
  // Reload solutions
  reload=-1;
  pcs_nval_data = NULL;
  pcs_eval_data = NULL;
  non_linear = false; //OK/CMCD
  cal_integration_point_value = false; //WW
  continuum = 0;
  adaption = false;
  compute_domain_face_normal = false; //WW
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
03/2003 OK Implementation
01/2005 WW Local assemblier as a member
last modified:
**************************************************************************/
CRFProcess::~CRFProcess(void)
{
  long i;
  //----------------------------------------------------------------------
  if(fem) delete fem; //WW
  fem = NULL;
  //----------------------------------------------------------------------
  ElementMatrix *eleMatrix = NULL;
  ElementValue* gp_ele = NULL;
  //WW
  if(Ele_Matrices.size()>0)
  {
     for (i=0;i<(long)Ele_Matrices.size();i++)
     {
         eleMatrix = Ele_Matrices[i];
         delete eleMatrix;
         eleMatrix = NULL;
     }
     Ele_Matrices.clear();
  }
  //WW
  if(ele_gp_value.size()>0)
  {
     for (i=0;i<(long)ele_gp_value.size();i++)
     {
         gp_ele = ele_gp_value[i];
         delete gp_ele;
         gp_ele = NULL;
     }
     ele_gp_value.clear();
  }
  //----------------------------------------------------------------------
  // Matrix output
  if(matrix_file) // WW
  {
      matrix_file->close();
      delete matrix_file;
  }     
  //----------------------------------------------------------------------
  // Release memory of node values. WW
  for(i=0; i<(int)nod_val_vector.size(); i++)
  {
     delete nod_val_vector[i];
     nod_val_vector[i] = NULL;
  }
  nod_val_vector.clear();
  //  WW
  for(i=0; i<(int)st_node_value.size(); i++)
  {
     delete st_node_value[i];
     st_node_value[i] = NULL;    
  }
  st_node_value.clear();
  //----------------------------------------------------------------------
  //pcs_type_name.clear();
  continuum_vector.clear();
}

/**************************************************************************
FEMLib-Method:
Task:    Gauss point values for CFEMSH  
Programing:
08/2005 WW Implementation
**************************************************************************/
void CRFProcess::AllocateMemGPoint()
{
  if(pcs_type_name.find("FLOW")==0) return;
  long i; 
  CElem* elem = NULL;
  ElementValue* ele_GP=NULL;
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
  { 
     elem = m_msh->ele_vector[i];
     ele_GP = new ElementValue(this, elem);
	 ele_gp_value.push_back(ele_GP);
  }

}

/**************************************************************************
FEMLib-Method:
Task:    This function is a part of the monolithic scheme
         and it is used to assign pcs name to IC, ST, BC, TIM and OUT. object 
Programing:
07/2005 WW Implementation
**************************************************************************/
void CRFProcess::SetOBJNames()
{
  int i; 
  //----------------------------------------------------------------------
  // IC
  CInitialCondition *m_ic = NULL;
  for(i=0;i<(long)ic_vector.size();i++){
    m_ic = ic_vector[i];
	m_ic->pcs_type_name = pcs_type_name;
  }
  //----------------------------------------------------------------------
  // BC
  CBoundaryCondition *m_bc = NULL;
  list<CBoundaryCondition*>::const_iterator p_bc = bc_list.begin();
  while(p_bc!=bc_list.end()) {
    m_bc = *p_bc;
	m_bc->pcs_type_name = pcs_type_name;
    ++p_bc;
  }
  //----------------------------------------------------------------------
  // ST
  CSourceTerm* m_st = NULL;
  for(i=0;i<(int)st_vector.size();i++){
    m_st = st_vector[i];
	m_st->pcs_type_name = pcs_type_name;
  }
  //----------------------------------------------------------------------
/*
  // TIM
  CTimeDiscretization *m_tim = NULL; 
  for(i=0;i<(int)time_vector.size();i++){
    m_tim = time_vector[i];
	m_tim->pcs_type_name = pcs_type_name;
  }
*/
  //----------------------------------------------------------------------
  // TIM
  for(i=0;i<(int)time_vector.size();i++){
    Tim = time_vector[i];
	Tim->pcs_type_name = pcs_type_name;
  }
  //----------------------------------------------------------------------
  // OUT
// OK4216
  COutput* m_out = NULL;
  for(i=0;i<(int)out_vector.size();i++){
    m_out = out_vector[i];
	  m_out->pcs_type_name = pcs_type_name;
    //m_out->pcs_pv_name = pcs_primary_function_name[0];//CMCD
    //string temp = pcs_primary_function_name[0];
  }
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
10/2002 OK Implementation
04/2004 WW Modification for 3D problems 
02/2005 WW New fem calculator
04/2005 OK MSHCreateNOD2ELERelations
07/2005 WW Geometry element objects
02/2006 WW Removed memory leaking
01/2006 YD MMP for each PCS
04/2006 WW Unique linear solver for all processes if they share the same mesh
last modified:
CREATE
**************************************************************************/
void CRFProcess::Create()
{
  int i=0;
  int phase;
  int timelevel=0;
  CRFProcess *m_pcs = NULL; //
  //----------------------------------------------------------------------------
  if(m_msh) //OK->MB please shift to Config()
  {
    if(pcs_type_name.compare("GROUNDWATER_FLOW")==0)
      MSHDefineMobile(this);
  }
  //----------------------------------------------------------------------------
  int DOF = GetPrimaryVNumber(); //OK should be PCS member variable
  //----------------------------------------------------------------------------
  // MMP - create mmp groups for each process   //YD
  cout << "->Create MMP" << '\n';
  CMediumPropertiesGroup *m_mmp_group = NULL;
  for(i=0;i<DOF;i++)
  {
    m_mmp_group = MMPGetGroup(pcs_type_name);
    if(!m_mmp_group) {
      m_mmp_group = new CMediumPropertiesGroup();
      m_mmp_group->pcs_type_name = pcs_type_name; 
      m_mmp_group->Set(this);
      mmp_group_list.push_back(m_mmp_group);
    }
  }
  //----------------------------------------------------------------------------
  // NUM_NEW
  cout << "->Create NUM" << '\n';
  int no_numerics = (int)num_vector.size();
  CNumerics* m_num_tmp = NULL;
  if(pcs_type_name.compare("RANDOM_WALK"))	// PCH RWPT does not need this.
  {
	for(i=0;i<no_numerics;i++){
		m_num_tmp = num_vector[i];

		if((pcs_type_name.compare(m_num_tmp->pcs_type_name)==0) 
		||(m_num_tmp->pcs_type_name.compare(pcs_primary_function_name[0])==0))
		{
			m_num = m_num_tmp;
			break;
		}
	}
  }
  if(!m_num){
    cout << "Warning in CRFProcess::Create() - no NUM data" << endl;
    m_num_tmp = new CNumerics(pcs_type_name); //OK
    m_num = m_num_tmp;
  }
  else{
    pcs_nonlinear_iterations = m_num->nls_max_iterations;
    pcs_nonlinear_iteration_tolerance = m_num->nls_error_tolerance;
  }
  //----------------------------------------------------------------------------
  // EQS - create equation system
//WW CreateEQS();
  cout << "->Create EQS" << '\n';
  phase=1;
  // create EQS
  if(type==4)
  {
     eqs = CreateLinearSolverDim(m_num->ls_storage_method,DOF,DOF*Shift[DOF-1]);
     InitializeLinearSolver(eqs,m_num);
     PCS_Solver.push_back(eqs); //WW
  }
  else if(type==41)    
  {
      if(num_type_name.find("EXCAVATION")!=string::npos)
         eqs = CreateLinearSolverDim(m_num->ls_storage_method,DOF-1,Shift[DOF-1]);
      else
         eqs = CreateLinearSolverDim(m_num->ls_storage_method,DOF,Shift[DOF-1]+m_msh->GetNodesNumber(false));  
      InitializeLinearSolver(eqs,m_num);
      PCS_Solver.push_back(eqs); //WW
  }
  else
  {
    /////////////////////////////////////////////////////////////////////
    // If there is a solver exsiting. WW 
    m_pcs = NULL; 
    for(i=0; i<(int)pcs_vector.size(); i++)
	{
       m_pcs = pcs_vector[i];
       if(m_pcs&&m_pcs->eqs)
	   {
          if(m_pcs->pcs_type_name.find("DEFORMATION")==string::npos)
            break;
	   }
	}
    // If unique mesh
	if(m_pcs&&m_pcs->eqs&&(fem_msh_vector.size()==1))
       eqs = m_pcs->eqs;
    /////////////////////////////////////////////////////////////////////
	else
	{
       if(m_msh)
         eqs = CreateLinearSolver(m_num->ls_storage_method,m_msh->GetNodesNumber(false));
       else
         eqs = CreateLinearSolver(m_num->ls_storage_method,NodeListLength);
       InitializeLinearSolver(eqs,m_num);
       PCS_Solver.push_back(eqs); 
	}
  }
     // Set solver properties: EQS<->SOL
     // Internen Speicher allokieren
     // Speicher initialisieren

  //----------------------------------------------------------------------------
  // Time unit factor //WW
  cout << "->Create TIM" << '\n';
  //CTimeDiscretization* Tim = TIMGet(pcs_type_name);
    Tim = TIMGet(pcs_type_name);
  if(Tim){
    if(Tim->time_unit.find("MINUTE")!=string::npos) time_unit_factor=60.0;
    else if(Tim->time_unit.find("HOUR")!=string::npos) time_unit_factor=3600.0;
    else if(Tim->time_unit.find("DAY")!=string::npos) time_unit_factor=86400.0;
    else if(Tim->time_unit.find("MONTH")!=string::npos) time_unit_factor=2592000.0;
    else if(Tim->time_unit.find("YEAR")!=string::npos) time_unit_factor=31536000;
  }
  //----------------------------------------------------------------------------
  // BC - create BC groups for each process
  cout << "->Create BC" << '\n';
  CBoundaryConditionsGroup *m_bc_group = NULL;
  for(i=0;i<DOF;i++){
    //OKm_bc_group = BCGetGroup(pcs_type_name,pcs_primary_function_name[i]);
    //OKif(!m_bc_group){
    BCGroupDelete(pcs_type_name,pcs_primary_function_name[i]);
      m_bc_group = new CBoundaryConditionsGroup();
      m_bc_group->pcs_type_name = pcs_type_name; //OK
      m_bc_group->pcs_pv_name = pcs_primary_function_name[i]; //OK
      m_bc_group->Set(this,Shift[i]);
      bc_group_list.push_back(m_bc_group);
    //OK}
  } 
  //----------------------------------------------------------------------------
  // ST - create ST groups for each process
  cout << "->Create ST" << '\n';
  CSourceTermGroup *m_st_group = NULL;
  // WW
  if(WriteSourceNBC_RHS==2) // Read from file
     ReadRHS_of_ST_NeumannBC();
  else // WW
  {  // Calculate directly
    for(i=0;i<DOF;i++)
    {
      //OK m_st_group = m_st_group->Get(pcs_primary_function_name[i]);
      m_st_group = STGetGroup(pcs_type_name,pcs_primary_function_name[i]);
      if(!m_st_group) {
        m_st_group = new CSourceTermGroup();
        m_st_group->pcs_type_name = pcs_type_name; //OK
        m_st_group->pcs_pv_name = pcs_primary_function_name[i]; //OK
        m_st_group->Set(this,Shift[i]);
        st_group_list.push_back(m_st_group);
      }
    }
    if(WriteSourceNBC_RHS==1)// WW
       WriteRHS_of_ST_NeumannBC();
  }

  //----------------------------------------------------------------------------
  // ELE - config and create element values
  cout << "->Config ELE values" << '\n';
  if(!m_msh)
  {
    ConfigELEValues1();
    ConfigELEValues2();
    CreateELEValues();
    CreateELEGPValues();
  } 
  else AllocateMemGPoint();

  //----------------------------------------------------------------------------
  // ELE - config element matrices
  //-------------------------------------------------------
  // Will be removed is new FEM is ready
  bool OldFEM = true;
  if(num_type_name.find("NEW")!=string::npos)
     OldFEM = false;
  if(pcs_type_name.find("DEFORMATION")!=string::npos) // TEST
     OldFEM = false;
  if(OldFEM) // TEST
  {
    if(ConfigELEMatrices)
      ConfigELEMatrices(pcs_type_number);
    CreateELEMatricesPointer();
  }
  //----------------------------------------------------------------------------
  // NOD - config and create node values
  cout << "->Config NOD values" << '\n';
  double* nod_values = NULL;
  double* ele_values = NULL;    // PCH
  long j;
  size_t size;
  if(m_msh){
    number_of_nvals = 2*DOF + pcs_number_of_secondary_nvals;
    for(i=0;i<pcs_number_of_primary_nvals;i++){
      nod_val_name_vector.push_back(pcs_primary_function_name[i]); // new time
      nod_val_name_vector.push_back(pcs_primary_function_name[i]); // old time //need this MB!
    }
    for(i=0;i<pcs_number_of_secondary_nvals;i++)
      nod_val_name_vector.push_back(pcs_secondary_function_name[i]); // new time
    //
    long m_msh_nod_vector_size = m_msh->NodesNumber_Quadratic;
    for(j=0;j<m_msh_nod_vector_size;j++){
       nod_values =  new double[number_of_nvals];
       for(i=0;i<number_of_nvals;i++) nod_values[i] = 0.0;
          nod_val_vector.push_back(nod_values);
    } 

    // Create element values - PCH
    int number_of_evals = 2*pcs_number_of_evals;  //PCH, increase memory
    if(number_of_evals>0) // WW added this "if" condition
	{
       for(i=0;i<pcs_number_of_evals;i++)
       {
         ele_val_name_vector.push_back(pcs_eval_name[i]); // new time
         ele_val_name_vector.push_back(pcs_eval_name[i]); // old time
       }
       long m_msh_ele_vector_size = (long)m_msh->ele_vector.size();
       if(ele_val_vector.size()==0)
       {
         for(j=0;j<m_msh_ele_vector_size;j++)
         {
           ele_values =  new double[number_of_evals];
           for(i=0;i<number_of_evals;i++) 
             ele_values[i] = 0.0;
             ele_val_vector.push_back(ele_values);
           }
        } 
        else
        {
          for(j=0;j<m_msh_ele_vector_size;j++){
            ele_values = ele_val_vector[j];
#ifndef SX
#ifdef GCC
            size = malloc_usable_size( ele_values )/sizeof(double); 
#else
            size= _msize( ele_values )/sizeof(double);
#endif
#endif
            ele_values = resize(ele_values, size, size+ number_of_evals);
            ele_val_vector[j] = ele_values;
          }
        }
     }
  }
  else
  {
    ConfigNODValues1(); 
    ConfigNODValues2();
    CreateNODValues();
  }


  if(reload==2&&type!=4&&type!=41) 
     ReadSolution(); //WW
  //----------------------------------------------------------------------------
  //SelectData(0);
  //----------------------------------------------------------------------------
  // IC
  cout << "->Assign IC" << '\n';
  CInitialCondition *m_ic = NULL;
  int nidx;
  if(m_msh){
    SetIC();
  }
  else{
    for(i=0;i<DOF;i++){
      timelevel = 0;
      nidx = PCSGetNODValueIndex(pcs_primary_function_name[i],timelevel);
      for(j=0;j<(long)ic_vector.size();j++){
        m_ic = ic_vector[j];
        if((m_ic->pcs_pv_name.compare(pcs_primary_function_name[i])==0)&&\
           (m_ic->pcs_type_name.compare(pcs_type_name)==0)){
          m_ic->m_pcs = this;
          m_ic->Set(nidx);
        }
      }
      timelevel = 1;
      nidx = PCSGetNODValueIndex(pcs_primary_function_name[i],timelevel);
      for(j=0;j<(long)ic_vector.size();j++){
        m_ic = ic_vector[j];
        if((m_ic->pcs_pv_name.compare(pcs_primary_function_name[i])==0)&&\
           (m_ic->pcs_type_name.compare(pcs_type_name)==0)){
          m_ic->m_pcs = this;
          m_ic->Set(nidx);
        }
      }
    }
  }
  if(pcs_type_name.find("DYNAMIC")!=string::npos) //WW
     setIC_danymic_problems();
  if(m_msh)
  {
    if(type==4||type==41) m_msh->SwitchOnQuadraticNodes(true); 
	else  m_msh->SwitchOnQuadraticNodes(false); 
    CheckMarkedElement();
   //    m_msh->RenumberNodesForGlobalAssembly();
  } 


  //----------------------------------------------------------------------------
  if(!OldFEM) //WW This condition will be removed is new FEM is ready
  {
    // Keep all local matrices in the memory
    if(Memory_Type!=0)  
      AllocateLocalMatrixMemory();
    if(type==4||type==41)
    {
      // Set initialization function
      CRFProcessDeformation *dm_pcs = (CRFProcessDeformation *) this;
      dm_pcs->Initialization(); 
    }
    else  // Initialize FEM calculator
    {
         int Axisymm = 1; // ani-axisymmetry
         if(m_msh->isAxisymmetry()) Axisymm = -1; // Axisymmetry is true
         fem = new CFiniteElementStd(this, Axisymm*m_msh->GetCoordinateFlag()); 
    }
  }

  //----------------------------------------------------------------------
  // Initialize the system equations 
  if(PCSSetIC_USER)
    PCSSetIC_USER(pcs_type_number);
//OKRichards  if(pcs_type_name.find("RICHARD")!=string::npos)
//OKRichards    MMPSetICRichards(pcs_type_number);
  if(pcs_type_name.find("RICHARD")!=string::npos)    //YD
      continuum_ic = true;
  int time_level = 0;
  CalcSecondaryVariables(time_level);
  time_level = 1;
  CalcSecondaryVariables(time_level);
  if(pcs_type_name.find("RICHARD")!=string::npos)    //YD
      continuum_ic = false;
  //----------------------------------------------------------------------
  // Element matrix output. WW
  if(Write_Matrix)
  {
    cout << "->Write Matrix" << '\n';
     string m_file_name = FileName +"_"+pcs_type_name+"_element_matrix.txt";
     matrix_file = new fstream(m_file_name.c_str(),ios::trunc|ios::out);
     if (!matrix_file->good())
       cout << "Warning in GlobalAssembly: Matrix files are not found" << endl;
  }

  if(compute_domain_face_normal) //WW
     m_msh->FaceNormal();        
}
/**************************************************************************
FEMLib-Method:
Task: Write the contribution of ST or Neumann BC to RHS to a file after
      integration
Programing:
12/2005 WW 
03/2006 WW Write as acsi
04/2006 WW 
last modified:
**************************************************************************/
inline void CRFProcess::WriteRHS_of_ST_NeumannBC()
{
     string m_file_name = FileName +"_"+pcs_type_name+"_ST_RHS.asc";
     ofstream os(m_file_name.c_str(), ios::trunc|ios::out);     	
     if (!os.good())
     {
         cout << "Failure to open file: "<<m_file_name << endl;
         abort();
     }

	/*
    CSourceTermGroup *m_st_group = NULL;
    list<CSourceTermGroup*>::const_iterator p_st_group = st_group_list.begin();
    while(p_st_group!=st_group_list.end()) {
      m_st_group = *p_st_group;
	  m_st_group->Write(os);
      ++p_st_group;
    }
	*/
    os<<"$PCS_TYPE  "<<endl;
    //string s_buffer;
    //s_buffer = pcs_type_name;
    //transform(s_buffer.begin(), s_buffer.end(), s_buffer.begin(), toupper);
    os<<pcs_type_name.c_str()<<endl;
    os<<"geo_node_number  ";
    os<<"msh_node_number  ";
	os<<"CurveIndex ";
    os<<"node_value ";
    /*
    // This is for river flow
    // This writing will be valid for river flow when some 
    // of its parameters being moved from CSourceTerm to here
    os<<"node_distype ";
    os<<"node_area ";
    os<<"node_parameterA ";
    os<<"node_parameterB ";
    os<<"node_parameterC ";
    os<<"node_parameterD ";
    os<<"node_parameterE ";
    os<<"conditional ";
    */
    os<<endl;
    os.setf(ios::scientific,ios::floatfield);
  	os.precision(14);
    os<<(long)st_node_value.size()<<endl;
    for(long i=0; i<(long)st_node_value.size(); i++)
      st_node_value[i]->Write(os); 
    os.close();
}

/**************************************************************************
FEMLib-Method:
Task: Write the contribution of ST or Neumann BC to RHS to a file after
      integration
Programing:
03/2006 WW 
last modified: 04/2006
**************************************************************************/
inline void CRFProcess::ReadRHS_of_ST_NeumannBC()
{
    long Size;
    string m_file_name = FileName +"_"+pcs_type_name+"_ST_RHS.asc";
    ifstream is(m_file_name.c_str(), ios::in);     
    if (!is.good())
    {
        cout << "File "<<m_file_name<<" is not found" << endl;
        abort();
    }

    string s_buffer;
    getline(is, s_buffer);
	getline(is, s_buffer);
	getline(is, s_buffer);
    is>>Size>>ws;
    st_node_value.clear();
    for(long i=0; i<Size; i++)
	{
       CNodeValue *cnodev = new CNodeValue();
	   cnodev->Read(is);
       st_node_value.push_back(cnodev); 
	}
    is.close();
}
/**************************************************************************
FEMLib-Method:
Task: Write the solution
Programing:
04/2006 WW 
last modified:
**************************************************************************/
void CRFProcess:: WriteSolution()
{
    if(reload!=1) return;	 
    string m_file_name = FileName +"_"+pcs_type_name+"_primary_value.asc";
    ofstream os(m_file_name.c_str(), ios::trunc|ios::out);     	
    if (!os.good())
    {
       cout << "Failure to open file: "<<m_file_name << endl;
       abort();
    }
    //
    long i;
    int j;
    int idx[20];
	for(j=0; j<pcs_number_of_primary_nvals; j++) 
	{
      idx[j] = GetNodeValueIndex(pcs_primary_function_name[j]);
      idx[j+pcs_number_of_primary_nvals] = idx[j]+1;
	}
	for(i=0; i<m_msh->GetNodesNumber(false); i++)
    {
       for(j=0; j<2*pcs_number_of_primary_nvals; j++)
         os<<GetNodeValue(i,idx[j]) <<"  ";
       os<<endl;
    }  
    os.close();
}

/**************************************************************************
FEMLib-Method:
Task: Write the solution
Programing:
04/2006 WW 
last modified:
**************************************************************************/
void CRFProcess:: ReadSolution()
{
    string m_file_name = FileName +"_"+pcs_type_name+"_primary_value.asc";
    ifstream is(m_file_name.c_str(), ios::in);     	
    if (!is.good())
    {
       cout << "Failure to open file: "<<m_file_name << endl;
       abort();
    }
    //
    long i;
    int j;
    int idx[20];
    double val[20];
	for(j=0; j<pcs_number_of_primary_nvals; j++) 
	{
       idx[j] = GetNodeValueIndex(pcs_primary_function_name[j]);
       idx[j+pcs_number_of_primary_nvals] = idx[j]+1;
	}
	for(i=0; i<m_msh->GetNodesNumber(false); i++)
    {
       for(j=0; j<2*pcs_number_of_primary_nvals; j++)
         is>>val[j];
       is>>ws;
       for(j=0; j<2*pcs_number_of_primary_nvals; j++)
         SetNodeValue(i,idx[j], val[j]);
    }  
    is.close();
}


/**************************************************************************
FEMLib-Method:
Task:
Programing:
05/2005 WW Set coupling data
last modified:
**************************************************************************/
void CRFProcess::setIC_danymic_problems()
{
  char *function_name[7];
  int i, j, nv;
  nv = 0; 
  if(max_dim==1) // 2D
  {
    nv = 5;
    function_name[0] = "DISPLACEMENT_X1";
    function_name[1] = "DISPLACEMENT_Y1";
    function_name[2] = "VELOCITY_X1";
    function_name[3] = "VELOCITY_Y1";
    function_name[4] = "PRESSURE1"; 
  }
  else //3D
  {
    nv =7;
    function_name[0] = "DISPLACEMENT_X1";
    function_name[1] = "DISPLACEMENT_Y1";
    function_name[2] = "DISPLACEMENT_Z1";
    function_name[3] = "VELOCITY_X1";
    function_name[4] = "VELOCITY_Y1";
    function_name[5] = "VELOCITY_Z1";
    function_name[6] = "PRESSURE1"; 
  }

  CInitialCondition *m_ic = NULL;
  long no_ics =(long)ic_vector.size();
  int nidx;
  for(i=0;i<nv;i++){
    nidx = PCSGetNODValueIndex(function_name[i],1);
    for(j=0;j<no_ics;j++){
      m_ic = ic_vector[j];
      if(m_ic->pcs_pv_name.compare(function_name[i])==0)
        m_ic->Set(nidx);
    }
  }

}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
05/2005 WW Set coupling data
last modified:
**************************************************************************/
void CRFProcess::setST_danymic_problems()
{
  char *function_name[7];
  int i, nv;
  nv = 0; 
  if(max_dim==1) // 2D
  {
    nv = 5;
    function_name[0] = "DISPLACEMENT_X1";
    function_name[1] = "DISPLACEMENT_Y1";
    function_name[2] = "VELOCITY_X1";
    function_name[3] = "VELOCITY_Y1";
    function_name[4] = "PRESSURE1"; 
  }
  else //3D
  {
    nv =7;
    function_name[0] = "DISPLACEMENT_X1";
    function_name[1] = "DISPLACEMENT_Y1";
    function_name[2] = "DISPLACEMENT_Z1";
    function_name[3] = "VELOCITY_X1";
    function_name[4] = "VELOCITY_Y1";
    function_name[5] = "VELOCITY_Z1";
    function_name[6] = "PRESSURE1"; 
  }

  // ST - create ST groups for each process
  CSourceTermGroup *m_st_group = NULL;
  for(i=0;i<nv;i++)
  {
    m_st_group = m_st_group->Get(function_name[i]);
    if(!m_st_group) {
        m_st_group = new CSourceTermGroup();
        m_st_group->Set(this, Shift[i], function_name[i]);
        st_group_list.push_back(m_st_group);
    }
  }

}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
05/2005 WW Set coupling data
last modified:
**************************************************************************/
void CRFProcess::setBC_danymic_problems()
{
  char *function_name[7];
  int i, nv;
  nv = 0; 
  if(max_dim==1) // 2D
  {
    nv = 5;
    function_name[0] = "DISPLACEMENT_X1";
    function_name[1] = "DISPLACEMENT_Y1";
    function_name[2] = "VELOCITY_X1";
    function_name[3] = "VELOCITY_Y1";
    function_name[4] = "PRESSURE1"; 
  }
  else //3D
  {
    nv =7;
    function_name[0] = "DISPLACEMENT_X1";
    function_name[1] = "DISPLACEMENT_Y1";
    function_name[2] = "DISPLACEMENT_Z1";
    function_name[3] = "VELOCITY_X1";
    function_name[4] = "VELOCITY_Y1";
    function_name[5] = "VELOCITY_Z1";
    function_name[6] = "PRESSURE1"; 
  }

  
  // BC - create BC groups for each process
  CBoundaryConditionsGroup *m_bc_group = NULL;
  for(i=0;i<nv;i++){
    m_bc_group = m_bc_group->Get(function_name[i]);
    if(!m_bc_group){
       m_bc_group = new CBoundaryConditionsGroup();
       m_bc_group->Set(this, Shift[i], function_name[i]);
       bc_group_list.push_back(m_bc_group);
    }
  } 
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
02/2005 WW Set coupling data
last modified:
**************************************************************************/
void CRFProcess::ConfigureCouplingForLocalAssemblier()
{
   bool Dyn = false;
   if(pcs_type_name.find("DYNAMIC")!=string::npos)
     Dyn = true;
   if(fem)
	   fem->ConfigureCoupling(this, Shift, Dyn);
}
/**************************************************************************
FEMLib-Method:
Task:
Programing:
06/2003 OK Implementation
last modified:
**************************************************************************/
void PCSDestroyAllProcesses(void)
{
  CRFProcess *m_process = NULL;
  long i;
  int j;
  LINEAR_SOLVER *eqs;
  int no_processes =(int)pcs_vector.size();
 //WW----------------------------------------
  for(j=0;j<(int)PCS_Solver.size();j++)
  {
    eqs = PCS_Solver[j]; 
    if(eqs->unknown_vector_indeces)
       eqs->unknown_vector_indeces = \
        (int*) Free(eqs->unknown_vector_indeces);
    if(eqs->unknown_node_numbers)
      eqs->unknown_node_numbers = \
        (long*) Free(eqs->unknown_node_numbers);
    if(eqs->unknown_update_methods)
       eqs->unknown_update_methods = \
        (int*) Free(eqs->unknown_update_methods); 
     eqs = DestroyLinearSolver(eqs);
     
  }
  //----------------------------------------

  for(j=0;j<no_processes;j++){
    m_process = pcs_vector[j];
    /* //WW
    if(m_process->eqs->bc_names_dirichlet)
      m_process->eqs->bc_names_dirichlet = \
        (char **) Free(m_process->eqs->bc_names_dirichlet);
    if(m_process->eqs->bc_names_neumann)
      m_process->eqs->bc_names_neumann = \
        (char **) Free(m_process->eqs->bc_names_neumann);
   */
    
/*
    for(j=0;j<m_process->number_of_nvals;j++) 
      if( m_process->pcs_nval_data[j]) 
        m_process->pcs_nval_data[j] = (PCS_NVAL_DATA *) Free(m_process->pcs_nval_data[j]);
*/
    if(m_process->pcs_nval_data)
      m_process->pcs_nval_data = (PCS_NVAL_DATA *) Free(m_process->pcs_nval_data);
/*
	for(j=0;j<m_process->pcs_number_of_evals;j++) 
      if( m_process->pcs_eval_data[j]) 
        m_process->pcs_eval_data[j] = (PCS_EVAL_DATA *) Free(m_process->pcs_eval_data[j]);
*/
    if(m_process->pcs_eval_data)
       m_process->pcs_eval_data = (PCS_EVAL_DATA *) Free(m_process->pcs_eval_data);
#ifdef PCS_NOD
    for (i=0;i<NodeListSize();i++) {
      k = GetNode(i);
      k->values[m_process->pcs_number] = (double *)Free(k->values[m_process->pcs_number]);
	}
#endif
    if(m_process->TempArry)   //MX
      m_process->TempArry = (double *) Free(m_process->TempArry);

	delete(m_process);
  }
  
  // WW
  for(i=0; i<(long)fem_msh_vector.size(); i++)
    delete fem_msh_vector[i];
  fem_msh_vector.clear();
  // WW
  for(i=0; i<(long)ele_val_vector.size(); i++)
    delete ele_val_vector[i];
  ele_val_vector.clear();
  //WW
  for(i=0; i<(long)ic_vector.size(); i++)
    delete ic_vector[i];
  ic_vector.clear();

  //----------------------------------------------------------------------
  MSPDelete(); //WW
  BCDelete();  //WW
  STDelete();  //WW



/*OK for PCS
  // Namen der Knotendaten loeschen
  for (i = 0; i < ModelsNumberNodeValInfoStructures(); i++)
    {
      if (nval_data[i].name)
        nval_data[i].name = (char *) Free(nval_data[i].name);
      if (nval_data[i].einheit)
        nval_data[i].einheit = (char *) Free(nval_data[i].einheit);
    }
  nval_data = (NvalInfo *) Free(nval_data);
  // Namen der Elementdaten loeschen
  for (i = 0; i	< ModelsNumberElementValInfoStructures(); i++)
    {
      if (eval_data[i].name)
        eval_data[i].name = (char *) Free(eval_data[i].name);
      if (eval_data[i].einheit)
        eval_data[i].einheit = (char *) Free(eval_data[i].einheit);
    }
  eval_data = (EvalInfo *) Free(eval_data);
*/
  /* Knoten-Daten  */
  for (i=0;i<NodeListSize();i++) {
    if (GetNode(i) != NULL) {                            
	  DestroyModelNodeData(i);   //SB:todo
    }
  }

  /* Element-Daten */
  for (i=0;i<ElListSize();i++) {
    if (ElGetElement(i) != NULL) {
      DestroyModelElementData(i);
    }
  }
/*OK_MOD
  if(MultiphaseFlow())
    DestroyKernel_MMP();
*/
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
01/2004 OK Implementation
08/2004 WW Read the deformation process
           Check the comment key '//' in .pcs
last modified:
**************************************************************************/
bool PCSRead(string file_base_name)
{
  //----------------------------------------------------------------------
  PCSDelete();  
  CRFProcess *m_pcs = NULL;
  CRFProcessDeformation *dm_pcs = NULL;

  char line[MAX_ZEILE];
  int Inctive_SubDomain[20];
  pcs_deformation = -1;
  int indexCh1a, indexCh2a;
  //basic_string <char>::size_type indexCh1a, indexCh2a;
  //  static const basic_string <char>::size_type npos = -1;
  string CommentK("//");
  string line_string;
  string pcs_file_name;
  ios::pos_type position;
  //========================================================================
  // File handling
  pcs_file_name = file_base_name + PCS_FILE_EXTENSION;
  ifstream pcs_file (pcs_file_name.data(),ios::in);
  if (!pcs_file.good()){
    cout << "Warning: no PCS data *.pcs file is missing" << endl;
    return false;
  }
  // rewind the file
  pcs_file.clear();
  pcs_file.seekg(0,ios::beg); 
  //========================================================================
  // Keyword loop
  cout << "PCSRead" << endl;
  while (!pcs_file.eof()) {
    pcs_file.getline(line,MAX_ZEILE);
    line_string = line;
    if(line_string.find("#STOP")!=string::npos)
      break;
    indexCh1a = (int)line_string.find_first_of(CommentK.c_str()); 
    indexCh2a = (int)line_string.find("#PROCESS");
    //----------------------------------------------------------------------
    if(indexCh2a>indexCh1a&&(indexCh1a==-1)) { // keyword found
      m_pcs = new CRFProcess();
      m_pcs->file_name_base = file_base_name; //OK
      position = m_pcs->Read(&pcs_file);
      //..................................................................
      // pcs_type_name_vector
      int pcs_type_name_vector_size = (int)m_pcs->pcs_type_name_vector.size();
      if(pcs_type_name_vector_size>1){
        if((m_pcs->pcs_type_name_vector[0].find("FLOW")!=string::npos)&&\
           (m_pcs->pcs_type_name_vector[1].find("DEFORMATION")!=string::npos)
		   //WW
		   || (m_pcs->pcs_type_name_vector[1].find("FLOW")!=string::npos)&&\
              (m_pcs->pcs_type_name_vector[0].find("DEFORMATION")!=string::npos) )
		{
            m_pcs->pcs_type_name = "DEFORMATION_FLOW";
			MH_Process = true; // MH monolithic scheme

		}
      }
      m_pcs->pcs_type_name_vector.clear();
      //..................................................................
      //RelocateDeformationProcess(m_pcs);
	  if(m_pcs->pcs_type_name.find("DEFORMATION")!=string::npos){
//...
         string pcs_name_dm = m_pcs->pcs_type_name; 
         string num_type_name_dm = m_pcs->num_type_name;
         bool m_output = false;
		 int rhs_out = m_pcs->WriteSourceNBC_RHS;
		 int r_load = m_pcs->reload;
         int m_memory = 0;
         int i = 0;
         int m_inactive = m_pcs->NumDeactivated_SubDomains;
         for(i=0; i<m_inactive; i++)
            Inctive_SubDomain[i] = m_pcs->Deactivated_SubDomain[i];

         m_output = m_pcs->Write_Matrix;
         m_memory = m_pcs->Memory_Type;
         // Numerics
         if(m_pcs->num_type_name.compare("STRONG_DISCONTINUITY")==0) 
            enhanced_strain_dm=1;
         delete m_pcs;

         dm_pcs = new CRFProcessDeformation();
         m_pcs = dynamic_cast<CRFProcess *> (dm_pcs);

         m_pcs->pcs_type_name = pcs_name_dm;
         m_pcs->Write_Matrix=m_output;
		 m_pcs->WriteSourceNBC_RHS = rhs_out;
         m_pcs->num_type_name = num_type_name_dm;
		 m_pcs->Memory_Type = m_memory;
         m_pcs->NumDeactivated_SubDomains = m_inactive;
         m_pcs->reload = r_load;
         for(i=0; i<m_inactive; i++)
            m_pcs->Deactivated_SubDomain[i] = Inctive_SubDomain[i];
         pcs_deformation = 1;
//...
      }
      //..................................................................
      m_pcs->pcs_number =(int)pcs_vector.size();
      pcs_vector.push_back(m_pcs);

      pcs_file.seekg(position,ios::beg);
    } // keyword found
  } // eof
  ///OK LOPPreTimeLoop_PCS();
  return true;
}

/**************************************************************************
FEMLib-Method: 
Task: PCS read function
Programing:
06/2004 OK Implementation
08/2004 WW Read deformation process
11/2004 OK file streaming
12/2005 OK MSH_TYPE
01/2006 OK GEO_TYPE
**************************************************************************/
ios::pos_type CRFProcess::Read(ifstream *pcs_file)
{
  char line[MAX_ZEILE];
  string line_string;
  string CommentK("//");
  string hash("#");
  bool new_keyword = false;
  bool new_subkeyword = false;
  ios::pos_type position;
  ios::pos_type position_subkeyword;
  std::stringstream line_stream;
  //----------------------------------------------------------------------
  while (!new_keyword) {
    position = pcs_file->tellg();
    pcs_file->getline(line,MAX_ZEILE);
    line_string = line;
    if(line_string.find(hash)!=string::npos) {
      new_keyword = true;
      break;
    }
    //....................................................................
    // WW Comment line
    if(line_string.find_first_of(CommentK.c_str())!=string::npos) return position;
    //....................................................................
    if(line_string.find("$PCS_TYPE")!=string::npos) { // subkeyword found
      while((!new_keyword)||(!new_subkeyword)||(!pcs_file->eof())){
        position = pcs_file->tellg();
        line_string = GetLineFromFile1(pcs_file);
        if(line_string.find("#")!=string::npos){
          return position;
        }
        if(line_string.find("$")!=string::npos){
          new_subkeyword = true;
          break;
        }
        line_stream.str(line_string);
        line_stream >> pcs_type_name;
        line_stream.clear();
        if(pcs_type_name.find("FLOW")!=string::npos){ 
          pcs_no_fluid_phases++;
          H_Process = true;
        }
        if(pcs_type_name.compare("FLUID_FLOW")==0){ 
          pcs_type_name = "LIQUID_FLOW";
        }
  	    if(pcs_type_name.find("DEFORMATION")!=string::npos){
          M_Process = true;
	      //if only "DEFORMATION", do not create solver for FLUID process. WW
          if(pcs_no_fluid_phases<1) 
            pcs_no_fluid_phases = 1;
	    }
	    if(pcs_type_name.compare("MASS_TRANSPORT")==0)
        { 
           H_Process = true;
		   MASS_TRANSPORT_Process = true;
           pcs_no_components++;
        }
        if(pcs_type_name.find("HEAT")!=string::npos)
           T_Process = true;
        pcs_type_name_vector.push_back(pcs_type_name);
		if(pcs_type_name.compare("FLUID_MOMENTUM")==0) {
			FLUID_MOMENTUM_Process = true;
			pcs_type_name = "FLUID_MOMENTUM";
		}
        if(pcs_type_name.compare("RANDOM_WALK")==0) {
			RANDOM_WALK_Process = true;
			pcs_type_name = "RANDOM_WALK";
		}
      }
/*
     *pcs_file >> pcs_type_name;
      if(pcs_type_name.find("FLOW")!=string::npos){ 
        pcs_no_fluid_phases++;
      }
      if(pcs_type_name.compare("FLUID_FLOW")==0){ 
        pcs_type_name = "LIQUID_FLOW";
      }
	  if(pcs_type_name.find("DEFORMATION")!=string::npos)
	  {
	    //if only "DEFORMATION", do not create solver for FLUID process. WW
        if(pcs_no_fluid_phases<1) 
          pcs_no_fluid_phases = 1;
	  }
	  if(pcs_type_name.compare("MASS_TRANSPORT")==0) 
        pcs_no_components++;
      pcs_file->ignore(MAX_ZEILE,'\n');
      continue;
*/
    }
    //....................................................................
    if(line_string.find("$NUM_TYPE")!=string::npos) { // subkeyword found
     *pcs_file >> num_type_name;
      pcs_file->ignore(MAX_ZEILE,'\n');
      continue;
    }
    //....................................................................
    if(line_string.find("$CPL_TYPE")!=string::npos) { // subkeyword found
     *pcs_file >> cpl_type_name;
      if(cpl_type_name.compare("MONOLITHIC")==0){
        pcs_monolithic_flow = true;
        pcs_deformation = 11;
      }
      pcs_file->ignore(MAX_ZEILE,'\n');
      continue;
    }
    //....................................................................
    if(line_string.find("$TIM_TYPE")!=string::npos) { // subkeyword found
     *pcs_file >> tim_type_name;
      pcs_file->ignore(MAX_ZEILE,'\n');
      continue;
    }
    //....................................................................
    if(line_string.find("$PRIMARY_VARIABLE")!=string::npos) { // subkeyword found
     *pcs_file >> primary_variable_name;
      pcs_file->ignore(MAX_ZEILE,'\n');
      continue;
    }
    //....................................................................
    if(line_string.find("$ELEMENT_MATRIX_OUTPUT")!=string::npos) { // subkeyword found
      *pcs_file >> Write_Matrix; //WW
      pcs_file->ignore(MAX_ZEILE,'\n');
      continue;
    }
    //....................................................................
    if(line_string.find("$ST_RHS")!=string::npos) { // subkeyword found
		*pcs_file >> WriteSourceNBC_RHS; //WW
        pcs_file->ignore(MAX_ZEILE,'\n');
        continue;
    }
    //....................................................................
    if(line_string.find("$MEMORY_TYPE")!=string::npos) { // subkeyword found
       *pcs_file >> Memory_Type;  //WW
       pcs_file->ignore(MAX_ZEILE,'\n');
       continue;
    }
    //....................................................................
    if(line_string.find("$RELOAD")!=string::npos) { // subkeyword found
       *pcs_file >> reload;  //WW
       pcs_file->ignore(MAX_ZEILE,'\n');
       continue;
    }
    if(line_string.find("$DEACTIVATED_SUBDOMAIN")!=string::npos) { // subkeyword found
        *pcs_file >> NumDeactivated_SubDomains>>ws; //WW
        for(int i=0; i<NumDeactivated_SubDomains; i++)
           *pcs_file >> Deactivated_SubDomain[i]>>ws;
        continue;
    }
    //....................................................................
    if(line_string.find("$MSH_TYPE")!=string::npos) { // subkeyword found
        *pcs_file >> msh_type_name >> ws;
      continue;
    }
    //....................................................................
    if(line_string.find("$GEO_TYPE")!=string::npos) { //OK
      *pcs_file >> geo_type >> geo_type_name >> ws;
      continue;
    }
    //....................................................................
    if(line_string.find("$MEDIUM_TYPE")!=string::npos) { // subkeyword found
      while ((!new_keyword)||(!new_subkeyword)||(!pcs_file->eof())) {
        position_subkeyword = pcs_file->tellg();
       *pcs_file >> line_string;
        if(line_string.size()==0) 
          break;
        if(line_string.find("#")!=string::npos) {
          new_keyword = true;
          break;
        }
        if(line_string.find("$")!=string::npos) {
          new_subkeyword = true;
          break;
        }
        if(line_string.find("CONTINUUM")!=string::npos) {
        *pcs_file >> line_string;
          continuum_vector.push_back(strtod(line_string.data(),NULL));
        }
        pcs_file->ignore(MAX_ZEILE,'\n');
      }
      continue;
    }
    //....................................................................
  }
  //----------------------------------------------------------------------
  return position;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
01/2004 OK Implementation
08/2004 WW Read the deformation process
           Check the comment key '//' in .pcs
last modified:
**************************************************************************/
void PCSWrite(string file_base_name)
{
  //----------------------------------------------------------------------
  // File handling
  string pcs_file_name = file_base_name + PCS_FILE_EXTENSION;
  fstream pcs_file (pcs_file_name.data(),ios::trunc|ios::out);
  // rewind the file
  pcs_file.clear();
  //----------------------------------------------------------------------
  // PCS loop
  cout << "PCSWrite" << endl;
  int no_processes = (int)pcs_vector.size();
  CRFProcess* m_pcs = NULL;
  int i;
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    pcs_file << "#PROCESS" << endl;
    m_pcs->Write(&pcs_file);
  }
  //----------------------------------------------------------------------
  pcs_file << "#STOP" << endl;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
01/2004 OK Implementation
12/2005 OK MSH_TYPE
last modified:
**************************************************************************/
void CRFProcess::Write(fstream *pcs_file)
{
  //......................................................................
 *pcs_file << " $PCS_TYPE" << endl;
 *pcs_file << "  " << pcs_type_name << endl;
  //......................................................................
 *pcs_file << " $NUM_TYPE" << endl;
 *pcs_file << "  " << num_type_name << endl;
  //......................................................................
 *pcs_file << " $CPL_TYPE" << endl;
 *pcs_file << "  " << cpl_type_name << endl;
  //......................................................................
 *pcs_file << " $TIM_TYPE" << endl;
 *pcs_file << "  " << tim_type_name << endl;
  //......................................................................
  if(msh_type_name.size()>0){
   *pcs_file << " $MSH_TYPE" << endl;
   *pcs_file << "  " << msh_type_name << endl;
  }
  //......................................................................
 //*pcs_file << " $PRIMARY_VARIABLE" << endl;
 //*pcs_file << "  " << primary_variable_name << endl;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
12/2004 OK Implementation
last modified:
**************************************************************************/
CRFProcess* PCSGet(string pcs_type_name)
{
  CRFProcess *m_pcs = NULL;
  for(int i=0;i<(int)pcs_vector.size();i++){
    m_pcs = pcs_vector[i];
    if(m_pcs->pcs_type_name.compare(pcs_type_name)==0){
      return m_pcs;
    }
  }
  return NULL;
}
//////////////////////////////////////////////////////////////////////////
// Access
//////////////////////////////////////////////////////////////////////////


// OK->SB please try Get function
CRFProcess *CRFProcess::GetProcessByFunctionName(char *name)
{
  CRFProcess *m_process = NULL;
  /* Tests */
  if (!name) return m_process;
  int i;
  int no_processes =(int)pcs_vector.size();
  for(i=0;i<no_processes;i++){
    m_process = pcs_vector[i];
    if ( strcmp(StrUp(m_process->pcs_primary_function_name[0]),StrUp(name))==0 ) {
      break;
    }
  }
  return m_process;
}

//SB: new 3912
CRFProcess *CRFProcess::GetProcessByNumber(int number)
{
  CRFProcess *m_process = NULL;
  /* Tests */
  if (number < 1) return m_process;
  int i;
  int no_processes =(int)pcs_vector.size();
  for(i=0;i<no_processes;i++){
    m_process = pcs_vector[i];
    if ( m_process->pcs_number == number ) {
      break;
    }
  }
  return m_process;
}

//////////////////////////////////////////////////////////////////////////
// Configuration
//////////////////////////////////////////////////////////////////////////

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
02/2003 OK Implementation
08/2003 WW Modified to fit monolithic scheme 
02/2005 OK Unsaturated flow (Richards model)
02/2005 MB string
05/2005 WW/DL Dymanic problem
01/2006 YD Dual Richards
OKToDo switch to char
get rid of type
**************************************************************************/
void CRFProcess::Config(void)
{
  //----------------------------------------------------------------------
  // Set mesh pointer to corresponding mesh
  m_msh = FEMGet(pcs_type_name);
  if((int)continuum_vector.size()== 0)  // YD
    continuum_vector.push_back(1.0);
  if(!m_msh)
    cout << "Error in CRFProcess::Config - no MSH data" << endl;
  CRFProcessDeformation *dm_pcs;
  dm_pcs = NULL;
  if(pcs_type_name.compare("LIQUID_FLOW")==0)  {
    type = 1; 
    ConfigLiquidFlow();
  }
  if(pcs_type_name.compare("GROUNDWATER_FLOW")==0)  {
    type = 1; 
    ConfigGroundwaterFlow();
  }
  if(pcs_type_name.compare("RICHARDS_FLOW")==0)  {
	  if((int)continuum_vector.size()>1){
        RD_Process = true;
		type = 22;
      }
	  else
          type = 14;
    ConfigUnsaturatedFlow();
  }
  if(pcs_type_name.compare("OVERLAND_FLOW")==0)  {
    type = 66;
    max_dim = 1;
    ConfigGroundwaterFlow();
  }
  if(pcs_type_name.compare("AIR_FLOW")==0)  { //OK
    type = 5;
    ConfigGasFlow();
  }
  if(pcs_type_name.compare("TWO_PHASE_FLOW")==0)  {
    type = 12;
    ConfigMultiphaseFlow();
  }
  if(pcs_type_name.compare("COMPONENTAL_FLOW")==0)  {
    type = 11;
    ConfigNonIsothermalFlow();
  }
  if(pcs_type_name.compare("HEAT_TRANSPORT")==0)  {
    type = 3;
    ConfigHeatTransport();
  }
  if(pcs_type_name.compare("MASS_TRANSPORT")==0)  {
    type = 2;
    ConfigMassTransport();
  }
  if(pcs_type_name.find("DEFORMATION")!=string::npos)
    ConfigDeformation();
  if(pcs_type_name.find("FLUID_MOMENTUM")!=string::npos) 
	ConfigFluidMomentum();
  
  if(pcs_type_name.find("RANDOM_WALK")!=string::npos) {
	ConfigRandomWalk();
  }
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
03/2003 OK Implementation
        WW Splitted for processes
last modified:
02/2005 MB Pressure version for LIQUID Flow
**************************************************************************/
void CRFProcess::ConfigLiquidFlow()
{
  pcs_num_name[0] = "PRESSURE0";
  pcs_sol_name    = "LINEAR_SOLVER_PROPERTIES_PRESSURE1";
  // NOD values
  pcs_number_of_primary_nvals = 1;
  pcs_number_of_secondary_nvals = 0;
  pcs_primary_function_name[0] = "PRESSURE1";
  pcs_primary_function_unit[0] = "Pa";
  // ELE values
  ConfigELEMatrices = PCSConfigELEMatricesSM;
  pcs_number_of_evals = 5;
  pcs_eval_name[0] = "VOLUME";
  pcs_eval_unit[0] = "m3";
  pcs_eval_name[1] = "VELOCITY1_X";
  pcs_eval_unit[1] = "m/s";
  pcs_eval_name[2] = "VELOCITY1_Y";
  pcs_eval_unit[2] = "m/s";
  pcs_eval_name[3] = "VELOCITY1_Z";
  pcs_eval_unit[3] = "m/s";
    pcs_eval_name[4] = "POROSITY";  //MX, test for n=n(c), 04.2005
  pcs_eval_unit[4] = "-";
  //----------------------------------------------------------------------
  // Secondary variables
  pcs_number_of_secondary_nvals = 1;
  pcs_secondary_function_name[0] = "HEAD";
  pcs_secondary_function_unit[0] = "m";
  pcs_secondary_function_timelevel[0] = 1;
}


/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
03/2003 OK Implementation
        WW Splitted for processes
last modified:
02/2005 MB head version for GroundwaterFlow
**************************************************************************/
void CRFProcess::ConfigGroundwaterFlow()
{
  pcs_num_name[0] = "HEAD";
  pcs_sol_name    = "LINEAR_SOLVER_PROPERTIES_HEAD";
  // NOD values
  pcs_number_of_primary_nvals = 1;
  pcs_primary_function_name[0] = "HEAD";
  pcs_primary_function_unit[0] = "m";
  // ELE values
  ConfigELEMatrices = PCSConfigELEMatricesSM;
  PCSDestroyELEMatrices[pcs_type_number] = NULL; //OK
  pcs_number_of_evals = 6;
  pcs_eval_name[0] = "VOLUME";
  pcs_eval_unit[0] = "m3";
  pcs_eval_name[1] = "VELOCITY1_X";
  pcs_eval_unit[1] = "m/s";
  pcs_eval_name[2] = "VELOCITY1_Y";
  pcs_eval_unit[2] = "m/s";
  pcs_eval_name[3] = "VELOCITY1_Z";
  pcs_eval_unit[3] = "m/s";
  pcs_eval_name[4] = "PERMEABILITY";
  pcs_eval_unit[4] = "m^2";
  pcs_eval_name[5] = "POROSITY";
  pcs_eval_unit[5] = "-";
  //----------------------------------------------------------------------
  // Secondary variables
  pcs_number_of_secondary_nvals = 2;
  pcs_secondary_function_name[0] = "FLUX";
  pcs_secondary_function_unit[0] = "m3/s";
  pcs_secondary_function_timelevel[0] = 1;
  pcs_secondary_function_name[1] = "WDEPTH";
  pcs_secondary_function_unit[1] = "m";
  pcs_secondary_function_timelevel[1] = 1;
  //----------------------------------------------------------------------
  if(m_msh)
    m_msh->DefineMobileNodes(this);
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
10/2004 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::ConfigGasFlow()
{
  //----------------------------------------------------------------------
  // Primary variables - NOD values
  pcs_number_of_primary_nvals = 1;
  pcs_number_of_secondary_nvals = 0;
  pcs_primary_function_name[0] = "PRESSURE1";
  pcs_primary_function_unit[0] = "Pa";
  //----------------------------------------------------------------------
  // Secondary variables - NOD values
  pcs_number_of_secondary_nvals = 1;
  pcs_secondary_function_name[0] = "NOD_MASS_FLUX";
  pcs_secondary_function_unit[0] = "kg/s";
  //----------------------------------------------------------------------
  // ELE values
  pcs_number_of_evals = 6;
  pcs_eval_name[0] = "VELOCITY1_X";
  pcs_eval_unit[0] = "m/s";
  pcs_eval_name[1] = "VELOCITY1_Y";
  pcs_eval_unit[1] = "m/s";
  pcs_eval_name[2] = "VELOCITY1_Z";
  pcs_eval_unit[2] = "m/s";
  pcs_eval_name[3] = "MASS_FLUX1_X";
  pcs_eval_unit[3] = "kg/s";
  pcs_eval_name[4] = "MASS_FLUX1_Y";
  pcs_eval_unit[4] = "kg/s";
  pcs_eval_name[5] = "MASS_FLUX1_Z";
  pcs_eval_unit[5] = "kg/s";
  //----------------------------------------------------------------------
  // NUM
  pcs_num_name[0] = "PRESSURE0";
  pcs_sol_name    = "LINEAR_SOLVER_PROPERTIES_PRESSURE1";
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
03/2003 OK Implementation
        WW Splitted for processes
last modified:
**************************************************************************/
void CRFProcess::ConfigMultiphaseFlow()
{           
  switch(pcs_type_number){
    case 0:
      pcs_num_name[0]= "PRESSURE0";
      pcs_sol_name   = "LINEAR_SOLVER_PROPERTIES_PRESSURE1";
      break;
    case 1:
      pcs_num_name[0]= "SATURATION0";
      pcs_sol_name   = "LINEAR_SOLVER_PROPERTIES_SATURATION1";
      break;
  }
  //----------------------------------------------------------------------
  // NOD Primary variables
  pcs_number_of_primary_nvals=1;
  switch(pcs_type_number){
    case 0:
      pcs_primary_function_name[0] = "PRESSURE1";
      pcs_primary_function_unit[0] = "Pa";
      break;
    case 1:
      pcs_primary_function_name[0] = "SATURATION2";
      pcs_primary_function_unit[0] = "m3/m3";
      break;
  }
  //----------------------------------------------------------------------
  // NOD Secondary variables
  pcs_number_of_secondary_nvals = 6;
  switch(pcs_type_number){
    case 0:
      pcs_secondary_function_name[0]      = "PRESSURE2";
      pcs_secondary_function_unit[0]      = "Pa";
      pcs_secondary_function_timelevel[0] = 0;
      pcs_secondary_function_name[1]      = "PRESSURE2";
      pcs_secondary_function_unit[1]      = "Pa";
      pcs_secondary_function_timelevel[1] = 1;
      pcs_secondary_function_name[2]      = "PRESSURE_CAP";
      pcs_secondary_function_unit[2]      = "Pa";
      pcs_secondary_function_timelevel[2] = 0;
      pcs_secondary_function_name[3]      = "FLUX";
      pcs_secondary_function_unit[3]      = "m3/s";
      pcs_secondary_function_timelevel[3] = 0;
      pcs_secondary_function_name[4]      = "DENSITY1";
      pcs_secondary_function_unit[4]      = "kg/m3";
      pcs_secondary_function_timelevel[4] = 1;
      pcs_secondary_function_name[5]      = "VISCOSITY1";
      pcs_secondary_function_unit[5]      = "Pa s";
      pcs_secondary_function_timelevel[5] = 1;
      break;
    case 1:
      pcs_secondary_function_name[0]      = "SATURATION1";
      pcs_secondary_function_timelevel[0] = 0;
      pcs_secondary_function_unit[0]      = "m3/m3";
      pcs_secondary_function_name[1]      = "SATURATION1";
      pcs_secondary_function_timelevel[1] = 1;
      pcs_secondary_function_unit[1]      = "m3/m3";
      pcs_secondary_function_name[2]      = "PRESSURE_CAP";
      pcs_secondary_function_unit[2]      = "Pa";
      pcs_secondary_function_timelevel[2] = 1;
      pcs_secondary_function_name[3]      = "FLUX";
      pcs_secondary_function_unit[3]      = "m3/s";
      pcs_secondary_function_timelevel[3] = 1;
      pcs_secondary_function_name[4]      = "DENSITY2";
      pcs_secondary_function_unit[4]      = "kg/m3";
      pcs_secondary_function_timelevel[4] = 1;
      pcs_secondary_function_name[5]      = "VISCOSITY2";
      pcs_secondary_function_unit[5]      = "Pa s";
      pcs_secondary_function_timelevel[5] = 1;
      break;
  }
  //----------------------------------------------------------------------
  // ELE values
  pcs_number_of_evals = 7;
  switch(pcs_type_number){
    case 0:
      pcs_eval_name[0] = "VELOCITY1_X";
      pcs_eval_unit[0] = "m/s";
	  pcs_eval_name[1] = "VELOCITY1_Y";
      pcs_eval_unit[1] = "m/s";
	  pcs_eval_name[2] = "VELOCITY1_Z";
      pcs_eval_unit[2] = "m/s";
	  pcs_eval_name[3] = "POROSITY1"; //MX 03.2005
      pcs_eval_unit[3] = "-";
	  pcs_eval_name[4] = "POROSITY1_IL"; //MX 03.2005
      pcs_eval_unit[4] = "-";
	  pcs_eval_name[5] = "PERMEABILITY1"; //MX 03.2005
      pcs_eval_unit[5] = "-";
	  pcs_eval_name[6] = "POROSITY1_SW"; //MX 03.2005
      pcs_eval_unit[6] = "-";
      break;
    case 1:
  	  pcs_eval_name[0] = "VELOCITY2_X";
      pcs_eval_unit[0] = "m/s";
	  pcs_eval_name[1] = "VELOCITY2_Y";
      pcs_eval_unit[1] = "m/s";
	  pcs_eval_name[2] = "VELOCITY2_Z";
      pcs_eval_unit[2] = "m/s";
	  pcs_eval_name[3] = "POROSITY";   //MX 03.2005
      pcs_eval_unit[3] = "-";
	  pcs_eval_name[4] = "POROSITY_IL"; //MX 03.2005
      pcs_eval_unit[4] = "-";
	  pcs_eval_name[5] = "PERMEABILITY"; //MX 03.2005
      pcs_eval_unit[5] = "-";
	  pcs_eval_name[6] = "POROSITY_SW"; //MX 03.2005
      pcs_eval_unit[6] = "-";
      break;
  }
  ConfigELEMatrices = PCSConfigELEMatricesMMP;
  // USER
  LOPCalcSecondaryVariables_USER = MMPCalcSecondaryVariables;
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
03/2003 OK Implementation
        WW Splitted for processes
last modified:
**************************************************************************/
void CRFProcess::ConfigNonIsothermalFlow()
{
  //----------------------------------------------------------------------
  // Primary variables
  pcs_number_of_primary_nvals=1;
  switch(pcs_type_number){
    case 0:
      pcs_num_name[0]= "PRESSURE0";
      pcs_sol_name = "LINEAR_SOLVER_PROPERTIES_PRESSURE1";
      pcs_primary_function_name[0] = "PRESSURE1";
      pcs_primary_function_unit[0] = "Pa";
      break;
    case 1:
      pcs_num_name[0]= "SATURATION0";
      pcs_sol_name = "LINEAR_SOLVER_PROPERTIES_SATURATION1";
      pcs_primary_function_name[0] = "SATURATION2";
      pcs_primary_function_unit[0] = "m3/m3";
      break;
  }
  //----------------------------------------------------------------------
  // Secondary variables
  pcs_number_of_secondary_nvals = 6;
  switch(pcs_type_number){
    case 0:
      pcs_secondary_function_name[0] = "PRESSURE2";
      pcs_secondary_function_timelevel[0] = 0;
      pcs_secondary_function_unit[0] = "Pa";
      pcs_secondary_function_name[1] = "PRESSURE2";
      pcs_secondary_function_timelevel[1] = 1;
      pcs_secondary_function_unit[1] = "Pa";
      pcs_secondary_function_name[2] = "MASS_FRACTION1";
      pcs_secondary_function_timelevel[2] = 0;
      pcs_secondary_function_unit[2] = "kg/kg";
      pcs_secondary_function_name[3] = "MASS_FRACTION1";
      pcs_secondary_function_timelevel[3] = 1;
      pcs_secondary_function_unit[3] = "kg/kg";
      pcs_secondary_function_name[4] = "PRESSURE_CAP";
      pcs_secondary_function_timelevel[4] = 0;
      pcs_secondary_function_unit[4] = "Pa";
      pcs_secondary_function_name[5] = "DENSITY1";
      pcs_secondary_function_timelevel[5] = 1;
      pcs_secondary_function_unit[5] = "kg/m3";
      break;
    case 1:
      pcs_secondary_function_name[0] = "SATURATION1";
      pcs_secondary_function_timelevel[0] = 0;
      pcs_secondary_function_unit[0] = "m3/m3";
      pcs_secondary_function_name[1] = "SATURATION1";
      pcs_secondary_function_timelevel[1] = 1;
      pcs_secondary_function_unit[1] = "m3/m3";
      pcs_secondary_function_name[2] = "MASS_FRACTION2";
      pcs_secondary_function_timelevel[2] = 0;
      pcs_secondary_function_unit[2] = "kg/kg";
      pcs_secondary_function_name[3] = "MASS_FRACTION2";
      pcs_secondary_function_timelevel[3] = 1;
      pcs_secondary_function_unit[3] = "kg/kg";
      pcs_secondary_function_name[4] = "PRESSURE_CAP";
      pcs_secondary_function_timelevel[4] = 1;
      pcs_secondary_function_unit[4] = "Pa";
      pcs_secondary_function_name[5] = "DENSITY2";
      pcs_secondary_function_timelevel[5] = 1;
      pcs_secondary_function_unit[5] = "kg/m3";
      break;
  }
  // Node
  pcs_number_of_primary_nvals=1;
  // ELE values
  pcs_number_of_evals = 14;
  pcs_eval_name[0]  = "COMP_FLUX";
  pcs_eval_name[1]  = "POROSITY";
  pcs_eval_name[2]  = "PERMEABILITY";
  pcs_eval_name[3]  = "VELOCITY1_X";
  pcs_eval_name[4]  = "VELOCITY1_Y";
  pcs_eval_name[5]  = "VELOCITY1_Z";
  pcs_eval_name[6]  = "VELOCITY2_X";
  pcs_eval_name[7]  = "VELOCITY2_Y";
  pcs_eval_name[8]  = "VELOCITY2_Z";
  pcs_eval_name[9]  = "POROSITY_IL";
  pcs_eval_name[10] = "VoidRatio";
  pcs_eval_name[11] = "PorosityChange";
  pcs_eval_name[12] = "n_sw_Rate";
  pcs_eval_name[13]  = "POROSITY_SW";
  pcs_eval_unit[0]  = "kg/s";
  pcs_eval_unit[1]  = "m3/m3";
  pcs_eval_unit[2]  = "m2";
  pcs_eval_unit[3]  = "m/s";
  pcs_eval_unit[4]  = "m/s";
  pcs_eval_unit[5]  = "m/s";
  pcs_eval_unit[6]  = "m/s";
  pcs_eval_unit[7]  = "m/s";
  pcs_eval_unit[8]  = "m/s";
  pcs_eval_unit[9]  = "-";
  pcs_eval_unit[10] = "-";
  pcs_eval_unit[11] = "-";
  pcs_eval_unit[12] = "-";
  pcs_eval_unit[13] = "-";
  ConfigELEMatrices = PCSConfigELEMatricesMPC;
  /* USER */
  PCSSetIC_USER = MPCSetIC;
  LOPCalcSecondaryVariables_USER = MPCCalcSecondaryVariables;
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
03/2003 OK Implementation
        WW Splitted for processes
last modified:
**************************************************************************/
void CRFProcess::ConfigNonIsothermalFlowRichards()
{
  switch(pcs_type_number){
    case 0:
      pcs_num_name[0]= "PRESSURE0";
      pcs_sol_name   = "LINEAR_SOLVER_PROPERTIES_PRESSURE1";
      break;
    case 1:
      pcs_num_name[0]= "SATURATION0";
      pcs_sol_name   = "LINEAR_SOLVER_PROPERTIES_SATURATION1";
      break;
  }
  // 1.1 primary variables
  pcs_number_of_primary_nvals = 1;
  pcs_primary_function_name[0] = "PRESSURE2";
  pcs_primary_function_unit[0] = "Pa";
   // 1.2 secondary variables
  LOPCalcSecondaryVariables_USER = MPCCalcSecondaryVariablesRichards;
  pcs_number_of_secondary_nvals = 14;
  pcs_secondary_function_name[0] = "SATURATION2";
  pcs_secondary_function_unit[0] = "m3/m3";
  pcs_secondary_function_timelevel[0] = 0;
  pcs_secondary_function_name[1] = "SATURATION2";
  pcs_secondary_function_unit[1] = "m3/m3";
  pcs_secondary_function_timelevel[1] = 1;
  pcs_secondary_function_name[2] = "SATURATION1";
  pcs_secondary_function_unit[2] = "m3/m3";
  pcs_secondary_function_timelevel[2] = 0;
  pcs_secondary_function_name[3] = "SATURATION1";
  pcs_secondary_function_unit[3] = "m3/m3";
  pcs_secondary_function_timelevel[3] = 1;
  pcs_secondary_function_name[4] = "PRESSURE_CAP";
  pcs_secondary_function_unit[4] = "Pa";
  pcs_secondary_function_timelevel[4] = 0;
  pcs_secondary_function_name[5] = "PRESSURE_CAP";
  pcs_secondary_function_unit[5] = "Pa";
  pcs_secondary_function_timelevel[5] = 1;
  pcs_secondary_function_name[6] = "MASS_FRACTION1";
  pcs_secondary_function_unit[6] = "kg/m3";
  pcs_secondary_function_timelevel[6] = 0;
  pcs_secondary_function_name[7] = "MASS_FRACTION1";
  pcs_secondary_function_unit[7] = "kg/m3";
  pcs_secondary_function_timelevel[7] = 1;
  pcs_secondary_function_name[8] = "MASS_FRACTION2";
  pcs_secondary_function_unit[8] = "kg/m3";
  pcs_secondary_function_timelevel[8] = 0;
  pcs_secondary_function_name[9] = "MASS_FRACTION2";
  pcs_secondary_function_unit[9] = "kg/m3";
  pcs_secondary_function_timelevel[9] = 1;
  pcs_secondary_function_name[10] = "PRESSURE1";
  pcs_secondary_function_unit[10] = "Pa";
  pcs_secondary_function_timelevel[10] = 0;
  pcs_secondary_function_name[11] = "PRESSURE1";
  pcs_secondary_function_unit[11] = "Pa";
  pcs_secondary_function_timelevel[11] = 1;
//SB:3912
  pcs_secondary_function_name[12] = "DENSITY1";
  pcs_secondary_function_unit[12] = "kg/m3";
  pcs_secondary_function_timelevel[12] = 1;
  pcs_secondary_function_name[13] = "DENSITY2";
  pcs_secondary_function_unit[13] = "kg/m3";
  pcs_secondary_function_timelevel[13] = 1;
  // 2 ELE values
  pcs_number_of_evals = 13; //WW
  pcs_eval_name[0] = "COMP_FLUX";
  pcs_eval_name[1] = "POROSITY";
  pcs_eval_name[2] = "PERMEABILITY";
  pcs_eval_name[3] = "VELOCITY1_X";
  pcs_eval_name[4] = "VELOCITY1_Y";
  pcs_eval_name[5] = "VELOCITY1_Z";
  pcs_eval_name[6] = "VELOCITY2_X";
  pcs_eval_name[7] = "VELOCITY2_Y";
  pcs_eval_name[8] = "VELOCITY2_Z";
  pcs_eval_name[9] = "POROSITY_IL";
  pcs_eval_name[10] = "VoidRatio";
  pcs_eval_name[11] = "PorosityChange";
  pcs_eval_name[12] = "n_sw_Rate";
  pcs_eval_unit[0]  = "kg/s";
  pcs_eval_unit[1]  = "m3/m3";
  pcs_eval_unit[2]  = "m2";
  pcs_eval_unit[3]  = "m/s";
  pcs_eval_unit[4]  = "m/s";
  pcs_eval_unit[5]  = "m/s";
  pcs_eval_unit[6]  = "m/s";
  pcs_eval_unit[7]  = "m/s";
  pcs_eval_unit[8]  = "m/s";
  pcs_eval_unit[9]  = "-";
  pcs_eval_unit[10] = "-";
  pcs_eval_unit[11] = "-";
  pcs_eval_unit[12] = "-";
  // 3 ELE matrices
  ConfigELEMatrices = PCSConfigELEMatricesMPC;
  PCSSetIC_USER = MPCSetICRichards;
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
03/2004 SB Implementation
        WW Splitted for processes
01/2006 OK Tests        
**************************************************************************/
void CRFProcess::ConfigMassTransport()
{
  long comp=1;
  /* count transport processes */
  pcs_component_number++;
  comp = pcs_component_number;
  // 1 NOD values
  // 1.1 primary variables
  pcs_number_of_primary_nvals = 1;
  pcs_primary_function_name[0]= new char[80];
//  sprintf(pcs_primary_function_name[0], "%s%li","CONCENTRATION",comp);
  //----------------------------------------------------------------------
  // Tests
  if((int)cp_vec.size()<pcs_component_number+1){
    cout << "Error in CRFProcess::ConfigMassTransport - not enough MCP data" << endl;
#ifdef MFC
    AfxMessageBox("Error in CRFProcess::ConfigMassTransport - not enough MCP data");
#endif
    return;
  }
  //----------------------------------------------------------------------
  sprintf(pcs_primary_function_name[0], "%s", cp_vec[pcs_component_number]->compname.c_str());
  pcs_primary_function_unit[0] = "kg/m3";  //SB
/* SB: Eintrag component name in Ausgabestruktur */ //SB:todo : just one phase todo:name
/*
    	pcs_primary_function_name[0] = GetTracerCompName(0,this->pcs_component_number-1);
		name_initial_condition_tracer_component = pcs_primary_function_name[0];
		pcs_ic_name_mass = pcs_primary_function_name[0]; 
*/
  // 1.2 secondary variables
  pcs_number_of_secondary_nvals = 2; //SB3909
  pcs_secondary_function_name[0]= new char[80];
  sprintf(pcs_secondary_function_name[0], "%s%li","MASS_FLUX_",comp);
//      pcs_secondary_function_name[0] = "MASS_FLUX1";
  pcs_secondary_function_unit[0] = "kg/m3/s";
  pcs_secondary_function_timelevel[0] = 0;
  pcs_secondary_function_name[1]= new char[80];
  sprintf(pcs_secondary_function_name[1], "%s%li","MASS_FLUX_",comp);
  pcs_secondary_function_unit[1] = "kg/m3/s";
  pcs_secondary_function_timelevel[1] = 1;
  LOPCalcSecondaryVariables_USER = MTM2CalcSecondaryVariables;  //SB:todo
  // 2 ELE values
  pcs_number_of_evals = 0;
//	  pcs_eval_name[0] = "Darcy velocity";
#ifdef REACTION_ELEMENT
  pcs_number_of_evals = 1;
  pcs_eval_name[0] = pcs_primary_function_name[0];
      pcs_eval_unit[0] = "mol/kgH2O";
#endif
  // 3 ELE matrices
  ConfigELEMatrices = PCSConfigELEMatricesMTM;
  // NUM
  pcs_num_name[0] = "CONCENTRATION0";
  /* SB: immer solver properties der ersten Komponente nehmen */
  pcs_sol_name = "LINEAR_SOLVER_PROPERTIES_CONCENTRATION1"; //SB ??


}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
03/2003 OK Implementation
        WW Splitted for processes
last modified:
**************************************************************************/
void CRFProcess::ConfigHeatTransport()
{
  pcs_num_name[0] = "TEMPERATURE0";
  pcs_sol_name    = "LINEAR_SOLVER_PROPERTIES_TEMPERATURE1";
//WW  ConfigELEMatrices = PCSConfigELEMatricesHTM;
  // NOD
  pcs_number_of_primary_nvals = 1;
  pcs_primary_function_name[0] = "TEMPERATURE1";
  pcs_primary_function_unit[0] = "K";
  pcs_number_of_secondary_nvals = 0;
#ifdef REACTION_ELEMENT
  pcs_number_of_evals = 1;  //MX
  pcs_eval_name[0] = "TEMPERATURE1";
  pcs_eval_unit[0] = "K";
#endif
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
03/2003 WW Implementation
last modified:
**************************************************************************/
void CRFProcess::ConfigDeformation()
{
  int i;
  CNumerics* num=NULL;
  type = 4;
  if(  pcs_type_name.find("DEFORMATION")!=string::npos
     &&pcs_type_name.find("FLOW")!=string::npos)
  {
     type = 41;
     cpl_type_name="MONOLITHIC";
     pcs_deformation=11;
  }
  pcs_coupling_iterations = 10;
  
  for(i=0;i<(int)num_vector.size();i++){
    num = num_vector[i];
    if(num->pcs_type_name.find("DEFORMATION")!=string::npos)
    {
       num->pcs_type_name = pcs_type_name;
	   if(num->nls_method==1)  // Newton-Raphson
	   {
           pcs_deformation = 101;
           if(type==41) pcs_deformation = 110;
	   }
       break;
     }
  }

  // Prepare for restart
  //RFConfigRenumber();
 
  // Geometry dimension
  problem_dimension_dm=m_msh->GetCoordinateFlag()/10;
  problem_2d_plane_dm=1;
 
  // NUM        
  pcs_sol_name = "LINEAR_SOLVER_PROPERTIES_DISPLACEMENT1";
  pcs_num_name[0] = "DISPLACEMENT0";
  pcs_num_name[1] = "PRESSURE0";
  if(pcs_type_name.find("DYNAMIC")!=string::npos)
    VariableDynamics();
  else
    VariableStaticProblem();
  
  // Coupling
  for(int i=0; i<GetPrimaryVNumber(); i++)
     Shift[i] = i*m_msh->GetNodesNumber(true);
  // OBJ names are set to PCS name
  if(type==41) SetOBJNames(); //OK->WW please put to Config()
}



/**************************************************************************
FEMLib-Method: Static problems
Task: 
Programing:
05/2005 WW Implementation
last modified:
**************************************************************************/
void CRFProcess::VariableStaticProblem()
{
  //----------------------------------------------------------------------
  // NOD Primary functions
  pcs_number_of_primary_nvals = 2; //OK distinguish 2/3D problems, problem_dimension_dm;
  dm_number_of_primary_nvals = 2; 
  pcs_primary_function_name[0] = "DISPLACEMENT_X1";
  pcs_primary_function_name[1] = "DISPLACEMENT_Y1";
  pcs_primary_function_unit[0] = "m";
  pcs_primary_function_unit[1] = "m";
  if(max_dim==2)
  {
     pcs_number_of_primary_nvals = 3;
     dm_number_of_primary_nvals = 3; 
     pcs_primary_function_name[2] = "DISPLACEMENT_Z1";
     pcs_primary_function_unit[2] = "m";
  }
  if(type==41){ //Monolithic scheme
    pcs_primary_function_name[pcs_number_of_primary_nvals] = "PRESSURE1";
    pcs_primary_function_unit[pcs_number_of_primary_nvals] = "Pa";
    pcs_number_of_primary_nvals++;
  }	  
  //----------------------------------------------------------------------
  // NOD Secondary functions
  pcs_number_of_secondary_nvals = 10;
  pcs_secondary_function_name[0] = "STRESS_XX"; 
  pcs_secondary_function_name[1] = "STRESS_XY"; 
  pcs_secondary_function_name[2] = "STRESS_YY"; 
  pcs_secondary_function_name[3] = "STRESS_ZZ"; 
  pcs_secondary_function_name[4] = "STRAIN_XX"; 
  pcs_secondary_function_name[5] = "STRAIN_XY"; 
  pcs_secondary_function_name[6] = "STRAIN_YY"; 
  pcs_secondary_function_name[7] = "STRAIN_ZZ";
  pcs_secondary_function_name[8] = "STRAIN_PLS"; 
  pcs_secondary_function_name[9] = "POROPRESSURE0"; 
  pcs_secondary_function_unit[0] = "Pa"; 
  pcs_secondary_function_unit[1] = "Pa"; 
  pcs_secondary_function_unit[2] = "Pa"; 
  pcs_secondary_function_unit[3] = "Pa"; 
  pcs_secondary_function_unit[4] = "-"; 
  pcs_secondary_function_unit[5] = "-"; 
  pcs_secondary_function_unit[6] = "-"; 
  pcs_secondary_function_unit[7] = "-"; 
  pcs_secondary_function_unit[8] = "-"; 
  pcs_secondary_function_unit[9] = "Pa"; 
  pcs_secondary_function_timelevel[0] = 1; 
  pcs_secondary_function_timelevel[1] = 1; 
  pcs_secondary_function_timelevel[2] = 1; 
  pcs_secondary_function_timelevel[3] = 1; 
  pcs_secondary_function_timelevel[4] = 1; 
  pcs_secondary_function_timelevel[5] = 1; 
  pcs_secondary_function_timelevel[6] = 1; 
  pcs_secondary_function_timelevel[7] = 1; 
  pcs_secondary_function_timelevel[8] = 1; 
  pcs_secondary_function_timelevel[9] = 1; 
  
  if(max_dim==2) // 3D
  {  
     pcs_number_of_secondary_nvals = 14;
     pcs_secondary_function_name[10] = "STRESS_XZ"; 
     pcs_secondary_function_name[11] = "STRESS_YZ"; 
     pcs_secondary_function_name[12] = "STRAIN_XZ"; 
     pcs_secondary_function_name[13] = "STRAIN_YZ"; 
     pcs_secondary_function_unit[10] = "Pa"; 
     pcs_secondary_function_unit[11] = "Pa"; 
     pcs_secondary_function_unit[12] = "Pa"; 
     pcs_secondary_function_unit[13] = "Pa"; 
     pcs_secondary_function_timelevel[10] = 1; 
     pcs_secondary_function_timelevel[11] = 1; 
     pcs_secondary_function_timelevel[12] = 1; 
     pcs_secondary_function_timelevel[13] = 1; 
  }

}
/**************************************************************************
FEMLib-Method: Dynamic problems
Task: 
Programing:
05/2005 WW/LD Implementation
last modified:
**************************************************************************/
void CRFProcess::VariableDynamics()
{
  //----------------------------------------------------------------------
  // NOD Primary functions
  pcs_number_of_primary_nvals = 2; 
  dm_number_of_primary_nvals = 2; 
  pcs_primary_function_name[0] = "ACCELERATION_X1";
  pcs_primary_function_name[1] = "ACCELERATION_Y1";
  pcs_primary_function_unit[0] = "m/s^2";
  pcs_primary_function_unit[1] = "m/s^2";
  if(max_dim==2)
  {
     pcs_number_of_primary_nvals = 3;
     dm_number_of_primary_nvals = 3; 
     pcs_primary_function_name[2] = "ACCELERATION_Z1";
     pcs_primary_function_unit[2] = "m/s^2";
  }
  pcs_primary_function_name[pcs_number_of_primary_nvals] = "PRESSURE_RATE1";
  pcs_primary_function_unit[pcs_number_of_primary_nvals] = "Pa/s";
  pcs_number_of_primary_nvals++;

  //----------------------------------------------------------------------
  // NOD Secondary functions
  pcs_number_of_secondary_nvals = 15;
  pcs_secondary_function_name[0] = "STRESS_XX"; 
  pcs_secondary_function_name[1] = "STRESS_XY"; 
  pcs_secondary_function_name[2] = "STRESS_YY"; 
  pcs_secondary_function_name[3] = "STRESS_ZZ"; 
  pcs_secondary_function_name[4] = "STRAIN_XX"; 
  pcs_secondary_function_name[5] = "STRAIN_XY"; 
  pcs_secondary_function_name[6] = "STRAIN_YY"; 
  pcs_secondary_function_name[7] = "STRAIN_ZZ";
  pcs_secondary_function_name[8] = "STRAIN_PLS"; 
  pcs_secondary_function_name[9] = "POROPRESSURE0"; 
  pcs_secondary_function_name[10] = "DISPLACEMENT_X1";
  pcs_secondary_function_name[11] = "DISPLACEMENT_Y1";
  pcs_secondary_function_name[12] = "VELOCITY_X1";
  pcs_secondary_function_name[13] = "VELOCITY_Y1";
  pcs_secondary_function_name[14] = "PRESSURE1"; 
  pcs_secondary_function_unit[0] = "Pa"; 
  pcs_secondary_function_unit[1] = "Pa"; 
  pcs_secondary_function_unit[2] = "Pa"; 
  pcs_secondary_function_unit[3] = "Pa"; 
  pcs_secondary_function_unit[4] = "-"; 
  pcs_secondary_function_unit[5] = "-"; 
  pcs_secondary_function_unit[6] = "-"; 
  pcs_secondary_function_unit[7] = "-"; 
  pcs_secondary_function_unit[8] = "-"; 
  pcs_secondary_function_unit[9] = "Pa"; 
  pcs_secondary_function_unit[10] = "m"; 
  pcs_secondary_function_unit[11] = "m"; 
  pcs_secondary_function_unit[12] = "m/s"; 
  pcs_secondary_function_unit[13] = "m/s"; 
  pcs_secondary_function_unit[14] = "Pa"; 
  pcs_secondary_function_timelevel[0] = 1; 
  pcs_secondary_function_timelevel[1] = 1; 
  pcs_secondary_function_timelevel[2] = 1; 
  pcs_secondary_function_timelevel[3] = 1; 
  pcs_secondary_function_timelevel[4] = 1; 
  pcs_secondary_function_timelevel[5] = 1; 
  pcs_secondary_function_timelevel[6] = 1; 
  pcs_secondary_function_timelevel[7] = 1; 
  pcs_secondary_function_timelevel[8] = 1; 
  pcs_secondary_function_timelevel[9] = 1; 
  pcs_secondary_function_timelevel[10] = 1; 
  pcs_secondary_function_timelevel[11] = 1; 
  pcs_secondary_function_timelevel[12] = 1; 
  pcs_secondary_function_timelevel[13] = 1; 
  pcs_secondary_function_timelevel[14] = 1; 

  // 3D
  if(max_dim==2) // 3D
  {  
     pcs_number_of_secondary_nvals = 21;
     pcs_secondary_function_name[15] = "STRESS_XZ"; 
     pcs_secondary_function_name[16] = "STRESS_YZ"; 
     pcs_secondary_function_name[17] = "STRAIN_XZ"; 
     pcs_secondary_function_name[18] = "STRAIN_YZ"; 
     pcs_secondary_function_name[19] = "DISPLACEMENT_Z1";
     pcs_secondary_function_name[20] = "VELOCITY_Z1";
   
     pcs_secondary_function_unit[15] = "Pa"; 
     pcs_secondary_function_unit[16] = "Pa"; 
     pcs_secondary_function_unit[17] = "Pa"; 
     pcs_secondary_function_unit[18] = "Pa"; 
     pcs_secondary_function_unit[19] = "m"; 
     pcs_secondary_function_unit[20] = "m/s"; 
     pcs_secondary_function_timelevel[15] = 1; 
     pcs_secondary_function_timelevel[16] = 1; 
     pcs_secondary_function_timelevel[17] = 1; 
     pcs_secondary_function_timelevel[18] = 1; 
     pcs_secondary_function_timelevel[19] = 1; 
     pcs_secondary_function_timelevel[20] = 1; 
  }
}


/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
02/2005 OK Implementation
02/2006 OK FLUX
**************************************************************************/
void CRFProcess::ConfigUnsaturatedFlow()
{
 if((int)continuum_vector.size() == 1){
  // 1.1 primary variables
  pcs_number_of_primary_nvals = 1;
  pcs_primary_function_name[0] = "PRESSURE1";
  pcs_primary_function_unit[0] = "Pa";
   // 1.2 secondary variables
  //OK LOPCalcSecondaryVariables_USER = MMPCalcSecondaryVariablesRichards; // p_c and S^l
  pcs_number_of_secondary_nvals = 8;
  pcs_secondary_function_name[0] = "SATURATION1";
  pcs_secondary_function_unit[0] = "m3/m3";
  pcs_secondary_function_timelevel[0] = 0;
  pcs_secondary_function_name[1] = "SATURATION1";
  pcs_secondary_function_unit[1] = "m3/m3";
  pcs_secondary_function_timelevel[1] = 1;
  pcs_secondary_function_name[2] = "PRESSURE_CAP";
  pcs_secondary_function_unit[2] = "Pa";
  pcs_secondary_function_timelevel[2] = 0;
  pcs_secondary_function_name[3] = "PRESSURE_CAP";
  pcs_secondary_function_unit[3] = "Pa";
  pcs_secondary_function_timelevel[3] = 1;
  pcs_secondary_function_name[4] = "FLUX"; //MB
  pcs_secondary_function_unit[4] = "m/s";
  pcs_secondary_function_timelevel[4] = 0;
  pcs_secondary_function_name[5] = "FLUX"; //MB
  pcs_secondary_function_unit[5] = "m/s";
  pcs_secondary_function_timelevel[5] = 1;
  pcs_secondary_function_name[6] = "STORAGE_P"; 
  pcs_secondary_function_unit[6] = "Pa";
  pcs_secondary_function_timelevel[6] = 0;
  pcs_secondary_function_name[7] = "STORAGE_P"; 
  pcs_secondary_function_unit[7] = "Pa";
  pcs_secondary_function_timelevel[7] = 0;
  }
  else if((int)continuum_vector.size() == 2){
  // 1.1 primary variables
  pcs_number_of_primary_nvals = 2;     //YD
  pcs_primary_function_name[0] = "PRESSURE1";
  pcs_primary_function_unit[0] = "Pa";
  pcs_primary_function_name[1] = "PRESSURE2";
  pcs_primary_function_unit[1] = "Pa";
   // 1.2 secondary variables
  //OK LOPCalcSecondaryVariables_USER = MMPCalcSecondaryVariablesRichards; // p_c and S^l
  pcs_number_of_secondary_nvals = 10;
  pcs_secondary_function_name[0] = "SATURATION1";
  pcs_secondary_function_unit[0] = "m3/m3";
  pcs_secondary_function_timelevel[0] = 0;
  pcs_secondary_function_name[1] = "SATURATION1";
  pcs_secondary_function_unit[1] = "m3/m3";
  pcs_secondary_function_timelevel[1] = 1;
  pcs_secondary_function_name[2] = "SATURATION2";
  pcs_secondary_function_unit[2] = "m3/m3";
  pcs_secondary_function_timelevel[2] = 1;
  pcs_secondary_function_name[3] = "SATURATION2";
  pcs_secondary_function_unit[3] = "m3/m3";
  pcs_secondary_function_timelevel[3] = 1;
  pcs_secondary_function_name[4] = "PRESSURE_CAP1";
  pcs_secondary_function_unit[4] = "Pa";
  pcs_secondary_function_timelevel[4] = 0;
  pcs_secondary_function_name[5] = "PRESSURE_CAP1";
  pcs_secondary_function_unit[5] = "Pa";
  pcs_secondary_function_timelevel[5] = 1;
  pcs_secondary_function_name[6] = "PRESSURE_CAP2";
  pcs_secondary_function_unit[6] = "Pa";
  pcs_secondary_function_timelevel[6] = 0;
  pcs_secondary_function_name[7] = "PRESSURE_CAP2";
  pcs_secondary_function_unit[7] = "Pa";
  pcs_secondary_function_timelevel[7] = 1;
  pcs_secondary_function_name[8] = "TOTAL_SATURATION";
  pcs_secondary_function_unit[8] = "m3/m3";
  pcs_secondary_function_timelevel[8] = 0;
  pcs_secondary_function_name[9] = "TOTAL_SATURATION";
  pcs_secondary_function_unit[9] = "m3/m3";
  pcs_secondary_function_timelevel[9] = 0;
  }
  // 2 ELE values
  pcs_number_of_evals = 8; 
  pcs_eval_name[0] = "VELOCITY1_X";
  pcs_eval_unit[0]  = "m/s";
  pcs_eval_name[1] = "VELOCITY1_Y";
  pcs_eval_unit[1]  = "m/s";
  pcs_eval_name[2] = "VELOCITY1_Z";
  pcs_eval_unit[2]  = "m/s";
  pcs_eval_name[3] = "POROSITY";   //MX 11.2005
  pcs_eval_unit[3] = "-";
  pcs_eval_name[4] = "POROSITY_IL"; //MX 11.2005
  pcs_eval_unit[4] = "-";
  pcs_eval_name[5] = "PERMEABILITY"; //MX 11.2005
  pcs_eval_unit[5] = "-";
  pcs_eval_name[6] = "n_sw"; //MX 11.2005
  pcs_eval_unit[6] = "-";
  pcs_eval_name[7] = "n_sw_rate"; //MX 11.2005
  pcs_eval_unit[7] = "-";

}
/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
05/2005 PCH Implementation
last modified:
**************************************************************************/
void CRFProcess::ConfigFluidMomentum()
{
  //pcs_num_name[0] = "VELOCITY1_X";
  // Nothing added in terms of matrix solver.
  // Just linear solver is good enough.
  pcs_sol_name    = "LINEAR_SOLVER_PROPERTIES_PRESSURE1"; 
  // NOD values
  pcs_number_of_primary_nvals = 3;
  pcs_primary_function_name[0] = "VELOCITY1_X";
  pcs_primary_function_unit[0] = "m/s";
  pcs_primary_function_name[1] = "VELOCITY1_Y";
  pcs_primary_function_unit[1] = "m/s";
  pcs_primary_function_name[2] = "VELOCITY1_Z";
  pcs_primary_function_unit[2] = "m/s";

  // I'm adding this to initialize for Fluid Momentum process
  pcs_number_of_secondary_nvals = 0;
  pcs_number_of_evals = 3;
 
  pcs_eval_name[0] = "VELOCITY1_X";
  pcs_eval_unit[0]  = "m/s";
  pcs_eval_name[1] = "VELOCITY1_Y";
  pcs_eval_unit[1]  = "m/s";
  pcs_eval_name[2] = "VELOCITY1_Z";
  pcs_eval_unit[2]  = "m/s";
}
/**************************************************************************/
void CRFProcess::ConfigRandomWalk()
{
  // Nothing added in terms of matrix solver.
  // Just linear solver is good enough.
  pcs_sol_name    = "LINEAR_SOLVER_PROPERTIES_PRESSURE1"; 
  
  // NOD values
  pcs_number_of_primary_nvals = 0;
  pcs_number_of_secondary_nvals = 0;

  // 2 ELE values
  pcs_number_of_evals = 1;  
  pcs_eval_name[0] = "CONCENTRATION0";
  pcs_eval_unit[0]  = "kg/m3";
}

//////////////////////////////////////////////////////////////////////////
// Configuration NOD
//////////////////////////////////////////////////////////////////////////

/*************************************************************************
ROCKFLOW - Function: 
Task: Config node values 
Programming: 02/2003 OK Implementation
04/2004   WW   Modification for 3D problems 
last modified:
**************************************************************************/
void CRFProcess::ConfigNODValues1(void)
{ 
  int i;
  int pcs_nval=0;
  const int DOF = GetPrimaryVNumber(); 
  anz_nval0 = anz_nval;
  number_of_nvals = 2*DOF + pcs_number_of_secondary_nvals;
  //NVAL
  pcs_nval_data = (PCS_NVAL_DATA *) \
                  Malloc(number_of_nvals*sizeof(PCS_NVAL_DATA));
  /*----------------------------------------------------------------*/
  for(i=0;i<DOF;i++) {
    /* Primary variable - old time */
    //NVAL pcs_nval_data[pcs_nval] = (PCS_NVAL_DATA *) Malloc(sizeof(PCS_NVAL_DATA));
    strcpy(pcs_nval_data[pcs_nval].name,pcs_primary_function_name[pcs_nval-i]);
    //Change name for the previous time level
    /*
    char *ch = strchr(pcs_nval_data[pcs_nval].name, '1');
    if( ch != NULL )
    {
      int pos = ch-pcs_nval_data[pcs_nval].name; 
      pcs_nval_data[pcs_nval].name[pos]='0'; 
    }
    else
      strcat(pcs_nval_data[pcs_nval].name, "0");   */
    //-------------------------------------------------------------------------------   
    strcpy(pcs_nval_data[pcs_nval].einheit,pcs_primary_function_unit[pcs_nval-i]);
    pcs_nval_data[pcs_nval].timelevel = 0;
    pcs_nval_data[pcs_nval].speichern = 0;  //WW 
    pcs_nval_data[pcs_nval].laden = 0;
    pcs_nval_data[pcs_nval].restart = 1;
    pcs_nval_data[pcs_nval].adapt_interpol = 1;
    pcs_nval_data[pcs_nval].vorgabe = 0.0;
#ifdef PCS_NOD
	pcs_nval_data[pcs_nval].nval_index = pcs_nval;
#else
	pcs_nval_data[pcs_nval].nval_index = anz_nval + pcs_nval;
#endif
    pcs_nval++;
    /* Primary variable - new time */
    //NVAL pcs_nval_data[pcs_nval] = (PCS_NVAL_DATA *) Malloc(sizeof(PCS_NVAL_DATA));
    strcpy(pcs_nval_data[pcs_nval].name,pcs_primary_function_name[pcs_nval-i-1]);
    strcpy(pcs_nval_data[pcs_nval].einheit,pcs_primary_function_unit[pcs_nval-i-1]);
    pcs_nval_data[pcs_nval].timelevel = 1;
    pcs_nval_data[pcs_nval].speichern = 1;


    pcs_nval_data[pcs_nval].laden = 0;
    pcs_nval_data[pcs_nval].restart = 1;
    pcs_nval_data[pcs_nval].adapt_interpol = 1;
    pcs_nval_data[pcs_nval].vorgabe = 0.0;
#ifdef PCS_NOD
	pcs_nval_data[pcs_nval].nval_index = pcs_nval;
#else
	pcs_nval_data[pcs_nval].nval_index = anz_nval + pcs_nval;
#endif
    pcs_nval++;
  }

  /*----------------------------------------------------------------*/
  /* Secondary variables */
 for(i=0;i<pcs_number_of_secondary_nvals;i++) {
  //NVAL pcs_nval_data[pcs_nval] = (PCS_NVAL_DATA *) Malloc(sizeof(PCS_NVAL_DATA));
  strcpy(pcs_nval_data[pcs_nval].name,pcs_secondary_function_name[i]);
  strcpy(pcs_nval_data[pcs_nval].einheit,pcs_secondary_function_unit[i]);
//  pcs_nval_data[i+2]->timelevel = 1; // always at new time level
  pcs_nval_data[pcs_nval].timelevel = pcs_secondary_function_timelevel[i];
if(pcs_nval_data[pcs_nval].timelevel==1)
  pcs_nval_data[pcs_nval].speichern = 1;
else
  pcs_nval_data[pcs_nval].speichern = 0;
  pcs_nval_data[pcs_nval].laden = 0;
  pcs_nval_data[pcs_nval].restart = 1;
  pcs_nval_data[pcs_nval].adapt_interpol = 1;
  pcs_nval_data[pcs_nval].vorgabe = 0.0;
#ifdef PCS_NOD
  pcs_nval_data[pcs_nval].nval_index = pcs_nval;
#else
  pcs_nval_data[pcs_nval].nval_index = anz_nval + pcs_nval;
#endif
  if(type==4||type==41){ 
    if(dm_number_of_primary_nvals==2||(dm_number_of_primary_nvals==3&&this->type==41)){
      //Block:
      //STRESS_ZX, STRESS_YZ, STRAIN_ZX, STRAIN_ZY and LUMPED_STRESS 
      if(i==3||i==4||i==9||i==10||i==13)
        pcs_nval_data[pcs_nval].speichern = 0; 
      if(!problem_2d_plane_dm){
        //  Block STRESS_ZZ and STRAIN_ZZ
        if(i==5||i==11) pcs_nval_data[pcs_nval].speichern = 0;
      } 
       //if(!pcs_plasticity)
       // {   
       //   if(i==12) pcs_nval_data[pcs_nval].speichern = 0;  //STRAIN_PLS 
       // }      
    }
  }
  pcs_nval++;
 }
 pcs_nval_data = pcs_nval_data;
}

/*************************************************************************
ROCKFLOW - Function: CRFProcess::PCSConfigNODValues
Task: Config node values 
Programming: 02/2003 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::ConfigNODValues2(void)
{
  int i;

  number_of_nvals = 2*GetPrimaryVNumber() + pcs_number_of_secondary_nvals;
  for(i=0;i<number_of_nvals;i++) {
    ModelsAddNodeValInfoStructure(pcs_nval_data[i].name,\
                                  pcs_nval_data[i].einheit,\
                                  pcs_nval_data[i].speichern,\
                                  pcs_nval_data[i].laden,\
                                  pcs_nval_data[i].restart,\
                                  pcs_nval_data[i].adapt_interpol,\
                                  pcs_nval_data[i].vorgabe);
  }
}

/*************************************************************************
ROCKFLOW - Function: 
Task: Create
Programming: 03/2003 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::CreateNODValues(void)
{
  long i;
  for (i=0;i<NodeListSize();i++) {
    if (GetNode(i) != NULL) {
      CreateModelNodeData(i);
#ifdef PCS_NOD
      k = GetNode(i);
      k->values[pcs_number] = (double *)Malloc(number_of_nvals*sizeof(double));
#endif
    }
  }
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
07/2004 OK Implementation
**************************************************************************/
int PCSGetNODValueIndex(string name,int timelevel)
{
  // PCS primary variables
  int pcs_vector_size =(int)pcs_vector.size();
  int i,j;
  CRFProcess *m_pcs = NULL;
  if(pcs_vector_size>0){
    for(i=0;i<pcs_vector_size;i++){
      m_pcs = pcs_vector[i];
      for(j=0;j<m_pcs->number_of_nvals;j++){
        if((name.compare(m_pcs->pcs_nval_data[j].name)==0) && \
           (m_pcs->pcs_nval_data[j].timelevel==timelevel))
           return m_pcs->pcs_nval_data[j].nval_index;
      }
    }
  }
  cout << "Error in PCSGetNODValueIndex: " << name << endl;
  return -1;
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
07/2004 OK Implementation
**************************************************************************/
double PCSGetNODValue(long node,char* name,int timelevel)
{
  int nidx;
  nidx = PCSGetNODValueIndex(name,timelevel);
  return GetNodeVal(node,nidx);
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
11/2004 OK Implementation
**************************************************************************/
void PCSSetNODValue(long node,string v_name,double value,int timelevel)
{
  SetNodeVal(node,PCSGetNODValueIndex(v_name,timelevel),value);
}

//////////////////////////////////////////////////////////////////////////
// Configuration ELE
//////////////////////////////////////////////////////////////////////////

/*************************************************************************
ROCKFLOW - Function:
Task: Config element values 
Programming: 02/2003 OK Implementation
last modified:
  06/2004  WW  
**************************************************************************/
void CRFProcess::ConfigELEValues1(void)
{
  int i;
  if(pcs_number_of_evals)
    pcs_eval_data = (PCS_EVAL_DATA *) Malloc(pcs_number_of_evals*sizeof(PCS_EVAL_DATA));
  for(i=0;i<pcs_number_of_evals;i++) {
    //pcs_eval_data[i] = (PCS_EVAL_DATA *) Malloc(sizeof(PCS_EVAL_DATA));
    strcpy(pcs_eval_data[i].name,pcs_eval_name[i]);
    strcpy(pcs_eval_data[i].einheit,pcs_eval_unit[i]);
    pcs_eval_data[i].speichern = 1;
    pcs_eval_data[i].laden = 0;
    pcs_eval_data[i].restart = 1;
    pcs_eval_data[i].adapt_interpol = 1;
    pcs_eval_data[i].vorgabe = 0.0;
    pcs_eval_data[i].index = anz_eval + i;
    pcs_eval_data[i].eval_index = anz_eval + i; //SB
  }
}

/*************************************************************************
ROCKFLOW - Function:
Task: Config element values 
Programming: 04/2003 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::ConfigELEValues2(void)
{
  int i;
  for(i=0;i<pcs_number_of_evals;i++) {
    ModelsAddElementValInfoStructure(pcs_eval_data[i].name,\
                                     pcs_eval_data[i].einheit,\
                                     pcs_eval_data[i].speichern,\
                                     pcs_eval_data[i].laden,\
                                     pcs_eval_data[i].restart,\
                                     pcs_eval_data[i].adapt_interpol,\
                                     pcs_eval_data[i].vorgabe);
  }
}

/*************************************************************************
ROCKFLOW - Function: 
Task: Create
Programming: 03/2003 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::CreateELEValues(void)
{
  long i;
  for (i=0;i<ElListSize();i++) {
    if (ElGetElement(i) != NULL) {
      ELECreateElementData(i);
    }
  }
}

/*************************************************************************
ROCKFLOW - Function:
Task: Create
Programming: 03/2003 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::CreateELEGPValues(void)
{
  long i;
  for (i=0;i<ElListSize();i++) {
    if (ElGetElement(i) != NULL) {
      ELECreateGPValues(i);
    }
  }
}

/*************************************************************************
ROCKFLOW - Function: CRFProcess::PCSGetELEValueIndex
Task: Provide index for element values
Programming: 08/2003 SB Implementation
last modified:
**************************************************************************/
int PCSGetELEValueIndex(char *name)
{
  int i;
  CRFProcess *m_process = NULL;
  int j;
  int no_processes =(int)pcs_vector.size();
  for(j=0;j<no_processes;j++){
    m_process = pcs_vector[j];
    for(i=0;i<m_process->pcs_number_of_evals;i++) {
      if(strcmp(m_process->pcs_eval_data[i].name,name)==0)
           return m_process->pcs_eval_data[i].eval_index;
    }
  }
  printf("PCSGetELEValueIndex Alert\n");
  printf("%s \n",name);
  return -1;
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
11/2004 OK Implementation
**************************************************************************/
double PCSGetELEValue(long index,double*gp,double theta,string nod_fct_name)
{
  int nn;
  int nidx0,nidx1;
  double e_value;
  double nod_fct0,nod_fct1;
  int no_nodes;
  long* element_nodes = NULL;
  //----------------------------------------------------------------------
  nidx0 = PCSGetNODValueIndex(nod_fct_name,0);
  nidx1 = PCSGetNODValueIndex(nod_fct_name,1);
  //----------------------------------------------------------------------
  nod_fct0 = 0.0;
  nod_fct1 = 0.0;
  if(gp==NULL){ // Element average value
    no_nodes = ElNumberOfNodes[ElGetElementType(index)-1];
    element_nodes = ElGetElementNodes(index);
    for(nn=0;nn<no_nodes;nn++){
      nod_fct0 += GetNodeVal(element_nodes[nn],nidx0);
      nod_fct1 += GetNodeVal(element_nodes[nn],nidx1);
    }
    nod_fct0 /= no_nodes;
    nod_fct1 /= no_nodes;
  }
  else{ // Gauss point values
    nod_fct0 = InterpolValue(index,nidx0,gp[0],gp[1],gp[2]);
    nod_fct1 = InterpolValue(index,nidx1,gp[0],gp[1],gp[2]);
  }
  //----------------------------------------------------------------------
  e_value = (1.-theta)*nod_fct0 + theta*nod_fct1;
  return e_value;
}

//////////////////////////////////////////////////////////////////////////
// Configuration ELE matrices
//////////////////////////////////////////////////////////////////////////

/*************************************************************************
ROCKFLOW - Function:
Task: Create
Programming: 03/2003 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::CreateELEMatricesPointer(void)
{
  long i;
  for (i=0;i<ElListSize();i++) {
    if (ElGetElement(i) != NULL) {
      ELECreateElementMatricesPointer(i,pcs_number);
    }
  }
}

/*************************************************************************
ROCKFLOW - Function: PCSConfigELEMatricesSM
Task: Config element matrices: data access functions
Programming: 
05/2003 OK Implementation
08/2004 OK EM pointer removed
last modified:
todo: direct access possible !
**************************************************************************/
void PCSConfigELEMatricesSM(int pcs_type_number)
{
  // Matrices pointer, allgemein  
  InitInternElementData = SMCreateELEMatricesPointer;
  ELEDestroyElementMatrices = SMDestroyELEMatricesPointer;
  PCSDestroyELEMatrices[pcs_type_number] = SMDestroyELEMatricesPointer;
}

/*************************************************************************
ROCKFLOW - Function: PCSConfigELEMatricesMMP
Task: Config element matrices: data access functions
Programming: 05/2003 OK Implementation
last modified:
todo: direct access possible !
**************************************************************************/
void PCSConfigELEMatricesMMP(int pcs_type_number)
{
  // Matrices pointer, allgemein
  if(pcs_type_number==0)
    InitInternElementData = MMPCreateELEMatricesPointer;
  if(pcs_type_number==1)
    InitInternElementData = MMSCreateELEMatricesPointer;
  //ELEDestroyElementMatrices = NULL; //MMPDestroyELEMatricesPointer;
  PCSDestroyELEMatrices[pcs_type_number] = MMPDestroyELEMatricesPointer;
  if(pcs_type_number==1)
    PCSDestroyELEMatrices[pcs_type_number] = MMSDestroyELEMatricesPointer;
  // Matrices access functions
    /* Kernel - MMP */
  MMPGetElementConductivityMatrix    = MMPGetElementConductivityMatrixPhase_MMP;
  MMPSetElementConductivityMatrix    = MMPSetElementConductivityMatrixPhase_MMP;
  MMPGetElementCapacitanceMatrix     = MMPGetElementCapacitanceMatrixPhase_MMP;
  MMPSetElementCapacitanceMatrix     = MMPSetElementCapacitanceMatrixPhase_MMP;
  MMPGetElementGravityVector         = MMPGetElementGravityVectorPhase_MMP;
  MMPSetElementGravityVector         = MMPSetElementGravityVectorPhase_MMP;
  MMPGetElementCapillarityVector     = MMPGetElementCapillarityVectorPhase_MMP;
  MMPSetElementCapillarityVector     = MMPSetElementCapillarityVectorPhase_MMP;
//  MMPGetElementSaturationMassMatrix  = MMPGetElementSaturationMassMatrix_MMP;
    /* Kernel - MMS */
  MMSGetElementConductivityMatrix   = MMSGetElementConductivityMatrixPhase_MMS;
  MMSGetElementCapillarityVector    = MMSGetElementCapillarityVector_MMS;
  MMSGetElementGravityVector        = MMSGetElementGravityVector_MMS;
  MMSGetElementSaturationMassMatrix = MMSGetElementSaturationMassMatrix_MMS;
  MMSSetElementSaturationMassMatrix = MMSSetElementSaturationMassMatrix_MMS;
  MMSGetElementCapacitanceMatrix    = MMSGetElementCapacitanceMatrix_MMS;

  /* Initialisiert die durch ein Modell benutzten Matrizentypen */
  int phase;
    for (phase = 0; phase < GetRFProcessNumPhases(); phase++) {
       InitializeMatrixtype_MMP("MMPCONDUCTIVITY0", phase, -1);
       InitializeMatrixtype_MMP("MMPCAPACITANCE0", phase, -1);
       InitializeMatrixtype_MMP("MMPGRAVITYVECTOR0", phase, -1);
       InitializeMatrixtype_MMP("MMPCAPILLARPRESSURE0", phase, -1); /* war auskommentiert */
       InitializeMatrixtype_MMP("MMPDEFORMATION0", phase, -1);
    }

}

/*************************************************************************
ROCKFLOW - Function: PCSConfigELEMatricesMPC
Task: Config element matrices: data access functions
Programming: 04/2003 OK Implementation
last modified:
todo: direct access possible !
**************************************************************************/
//void CRFProcess::PCSConfigELEMatricesMPC(void)
void PCSConfigELEMatricesMPC(int pcs_type_number)
{
  InitInternElementData = MPCCreateELEMatricesPointer;
  PCSDestroyELEMatrices[pcs_type_number] = MPCDestroyELEMatricesPointer;
}

/*************************************************************************
ROCKFLOW - Function: CRFProcess::PCSConfigELEMatricesMTM
Task: Config elementa matrix pointer
Programming:	02/2003 OK Implementation
last modified:	08/2003 SB  Adapted to MTM2
**************************************************************************/
void PCSConfigELEMatricesMTM(int pcs_type_number)
{
  /* Element matrices */
  InitInternElementData = MTM2CreateELEMatricesPointer; //SB: InitInternElementDataMTM2;
  PCSDestroyELEMatrices[pcs_type_number] = MTM2DestroyELEMatricesPointer; //SB: von OK
}

/*************************************************************************
ROCKFLOW - Function: PCSConfigELEMatricesHTM
Task: Config element matrices: data access functions
Programming: 04/2003 OK Implementation
last modified:
todo: direct access possible !
**************************************************************************/
/*
void PCSConfigELEMatricesHTM(int pcs_type_number)
{
  // Matrices pointer, allgemein
  InitInternElementData = HTMCreateELEMatricesPointer;
  ELEDestroyElementMatrices = HTMDestroyELEMatricesPointer;
  PCSDestroyELEMatrices[pcs_type_number] = HTMDestroyELEMatricesPointer;
}
*/
/**************************************************************************
FEMLib-Method: 
Task:  Activate or deactivate elements specified in .pcs file
Programing:
05/2005 WW Implementation
**************************************************************************/
void CRFProcess::CheckMarkedElement()
{
  int i, j;
  long l;  
  bool done;
  CElem* elem = NULL;
  for (l = 0; l < (long)m_msh->ele_vector.size(); l++)
  {
     elem = m_msh->ele_vector[l];
     done = false;
     for(i=0; i<NumDeactivated_SubDomains; i++)
     {
        if(elem->GetPatchIndex()== Deactivated_SubDomain[i]) 
        {
           elem->MarkingAll(false);
           done = true;
           break;
        } 
     } 
     if(done) continue;
	 else 
        elem->MarkingAll(true);

  }
  for (l = 0; l < (long)m_msh->nod_vector.size(); l++)
  {
    while(m_msh->nod_vector[l]->connected_elements.size())
      m_msh->nod_vector[l]->connected_elements.pop_back();
  }
  for (l = 0; l < (long)m_msh->ele_vector.size(); l++)
  {
     elem = m_msh->ele_vector[l];
	 if(!elem->GetMark()) continue;
  	 for(i=0; i<elem->GetNodesNumber(m_msh->getOrder()); i++)
     {
        done = false;
		for(j=0; j<(int)elem->GetNode(i)->connected_elements.size(); j++)
        {
            if(l==elem->GetNode(i)->connected_elements[j])
			{
               done = true;
               break;
			}
        }
        if(!done)  
           elem->GetNode(i)->connected_elements.push_back(l);
     }

  }  //
}


//////////////////////////////////////////////////////////////////////////
// PCS Execution
//////////////////////////////////////////////////////////////////////////

/*************************************************************************
ROCKFLOW - Function: CRFProcess::
Task:
Programming: 
02/2003 OK Implementation
04/2003 OK Storing time step results
08/2004 OK PCS2
01/2005 WW new ELE concept
03/2005 OK MultiMSH concept 
06/2005 MB NEWTON error calculation
01/2006 YD Initialize mmp for dual porosity
last modified:
**************************************************************************/
double CRFProcess::Execute()
{
  int timelevel;
  int nidx0,nidx1;
  double pcs_error;
  int i;
  long j;
  //----------------------------------------------------------------------
  cout << "    ->Process " << pcs_number << ": " << pcs_type_name << endl;
  //----------------------------------------------------------------------
  // 0 Initializations
  // System matrix
  SetLinearSolverType(eqs, m_num); //WW
  SetZeroLinearSolver(eqs);

  // Solution vector
  //......................................................................
  if(m_msh){ // MSH data
    CheckMarkedElement();
    m_msh->SwitchOnQuadraticNodes(false);
	if(type == 22){
      nidx1 = GetNodeValueIndex(pcs_primary_function_name[continuum]) + 1; //new time
	  for(j=0;j<eqs->dim;j++)
		  eqs->x[j] = GetNodeValue(m_msh->Eqs2Global_NodeIndex[j],nidx1);
	}
	else{
    for(i=0;i<pcs_number_of_primary_nvals;i++){
      nidx1 = GetNodeValueIndex(pcs_primary_function_name[i]) + 1; //new time
	  for(j=0;j<eqs->dim;j++)
		  eqs->x[j] = GetNodeValue(m_msh->Eqs2Global_NodeIndex[j],nidx1);
    }
	}
  }
  //......................................................................
  else{ // RFI data
    for(i=0;i<pcs_number_of_primary_nvals;i++) {
      timelevel=1;
      nidx1 = PCSGetNODValueIndex(pcs_primary_function_name[i],timelevel);
      TransferNodeValuesToVectorLinearSolver(eqs,nidx1);
    }
  }

  /*---------------------------------------------------------------------*/
  /* 1 Calc element matrices */
if((aktueller_zeitschritt==1)||(tim_type_name.compare("TRANSIENT")==0)){
  cout << "      Calculate element matrices" << endl;
#ifdef MFC
  CWnd *pWin = ((CWinApp *) AfxGetApp())->m_pMainWnd;
  CString m_str_time;
  m_str_time.Format("Time step: t=%e sec",aktuelle_zeit);
  CString m_str_pcs = pcs_type_name.c_str();
  CString m_str = m_str_time + ", " + m_str_pcs + ", Calculate element matrices";
  pWin->SendMessage(WM_SETMESSAGESTRING,0,(LPARAM)(LPCSTR)m_str);
#endif
  if(num_type_name.find("NEW")!=0) // Will be removed. WW
     CalculateElementMatrices();
}
  /*---------------------------------------------------------------------*/
  /* 2 Assemble EQS */
  cout << "      Assemble equation system" << endl;
#ifdef MFC
  CWnd *pWin = ((CWinApp *) AfxGetApp())->m_pMainWnd;
  CString m_str_time;
  m_str_time.Format("Time step: t=%e sec",aktuelle_zeit);
  CString m_str_pcs = pcs_type_name.c_str();
  CString m_str = m_str_time + ", " + m_str_pcs + ", Assemble equation system";
  pWin->SendMessage(WM_SETMESSAGESTRING,0,(LPARAM)(LPCSTR)m_str);
#endif
  if(num_type_name.find("NEW")!=string::npos) //WWToDo Will be removed
    GlobalAssembly();
  else
    AssembleSystemMatrixNew();
  //----------------------------------------------------------------------
  // 3 Incorporate ST 
  cout << "      Incorporate source terms" << endl;
//WW#ifdef CHECK_ST_GROUP
//   CheckSTGroup();
//#endif
 //ofstream Dum("rf_pcs.txt", ios::out);
// Write_Matrix_M5(eqs->x, Dum); abort();
//MXDumpGLS("rf_pcs.txt",1,eqs->b,eqs->x); abort();
  IncorporateSourceTerms();
  SetCPL(); //OK2
  //----------------------------------------------------------------------
  // 4 Incorporate BC
  cout << "      Incorporate boundary conditions" << endl;
#ifdef CHECK_BC_GROUP
  CheckBCGroup();
#endif
  IncorporateBoundaryConditions();
//  MXDumpGLS("rf_pcs.txt",1,eqs->b,eqs->x); abort();
  //ofstream Dum("rf_pcs.txt", ios::out); WW
  //Write_Matrix_M5(eqs->b, Dum); abort();
  //----------------------------------------------------------------------
  /* 5 Solve EQS */
  cout << "      Solve equation system" << endl;
#ifdef MFC
  m_str_time.Format("Time step: t=%e sec",aktuelle_zeit);
  m_str_pcs = pcs_type_name.c_str();
  m_str = m_str_time + ", " + m_str_pcs + ", Solve equation system";
  pWin->SendMessage(WM_SETMESSAGESTRING,0,(LPARAM)(LPCSTR)m_str);
#endif
#ifdef CHECK_EQS
  string eqs_name = pcs_type_name + "_EQS.txt";
  MXDumpGLS((char*)eqs_name.c_str(),1,eqs->b,eqs->x);
#endif
  //......................................................................
  // Transformation von M2 zu M5/M6
  if (m_num->ls_storage_method == 5 )
  {
 //   transM2toM5();
  }
  if (m_num->ls_storage_method == 6 )
  {
    transM2toM6();
  }
  //......................................................................
   ExecuteLinearSolver();
//PCSDumpModelNodeValues();
  //----------------------------------------------------------------------
  // Error calculation 
  //----------------------------------------------------------------------
  // NEWTON
  if(m_num->nls_method_name.find("NEWTON")!=string::npos) {
    double Val;
    pcs_error = 0.0;

    if(m_msh){  // MSH data
      nidx1 = GetNodeValueIndex("HEAD")+1;
      //for(long nn=0;nn<NodeListSize(); nn++)  {
        for(long nn=0;nn<(long)m_msh->nod_vector.size();nn++){
        Val = GetNodeValue(nn,nidx1)+ eqs->x[nn];
	       pcs_error = max(pcs_error, fabs(Val - GetNodeValue(nn,nidx1)));
	       SetNodeValue(nn,nidx1, Val);
      }
    }
    else{  // RFI data
      nidx1 = 1;
      for(long nn=0;nn<NodeListSize(); nn++)  {
        Val = GetNodeVal(nn,nidx1)+ eqs->x[nn];
	       pcs_error = max(pcs_error, fabs(Val - GetNodeVal(nn,nidx1)));
	       SetNodeVal(nn,nidx1, Val);
      }
    }
	cout << "      PCS error: " << pcs_error << endl; //OK7
  } // NEWTON
  //----------------------------------------------------------------------
  else{ //PICARD
    pcs_error=0.0;
    //......................................................................
    if(m_msh){ // MSH data
      for(i=0; i<pcs_number_of_primary_nvals; i++){  
		 pcs_error += CalcIterationNODError(1); //OK4105//WW4117
      }
    }
    //......................................................................
    else{ // RFI data
      for(i=0; i<pcs_number_of_primary_nvals; i++){  
        nidx0 = PCSGetNODValueIndex(pcs_primary_function_name[i],0);
        nidx1 = PCSGetNODValueIndex(pcs_primary_function_name[i],1);
        pcs_error += CalcIterationErrorVectorAgainstNodes(eqs->x,nidx1,nidx0,1);
      }
    }
    cout << "      PCS error: " << pcs_error << endl;
    //--------------------------------------------------------------------
    // 7 Store solution vector in model node values table
    //....................................................................
  if(m_msh) // MSH data
  {
	if(type == 22)
    {
      nidx1 = GetNodeValueIndex(pcs_primary_function_name[continuum]) + 1; //new time
	  for(j=0;j<eqs->dim;j++)
           SetNodeValue(m_msh->Eqs2Global_NodeIndex[j],nidx1,eqs->x[j]);
	}
	else
    {
      for(i=0; i<pcs_number_of_primary_nvals; i++)
      {  
        nidx1 = GetNodeValueIndex(pcs_primary_function_name[i])+1;
		for(j=0;j<eqs->dim;j++)
        {
          SetNodeValue(m_msh->Eqs2Global_NodeIndex[j],nidx1,eqs->x[j]);
        }
	  }
	}
  }
    //....................................................................
    else{ // RFI data
      for(i=0; i<pcs_number_of_primary_nvals; i++){  
        nidx1 = PCSGetNODValueIndex(pcs_primary_function_name[i],1);
        TransferNodeValues(eqs,nidx1); 
      }
    }
  } // END PICARD
  //----------------------------------------------------------------------
  // 8 Calculate secondary variables
  CalcSecondaryVariables(1); // timelevel
  //----------------------------------------------------------------------
  return pcs_error;
}

/*************************************************************************
ROCKFLOW - Function:
Task: Initialize the equation system
Programming: 
 05/2003 OK/WW Implementation
 08/2003 WW    Changes for momolithic coupled equations    
last modified: 
**************************************************************************/
void CRFProcess::InitEQS()
{
    int i;
    eqs->unknown_vector_indeces = \
      (int*) Malloc(pcs_number_of_primary_nvals*sizeof(int));
    eqs->unknown_node_numbers = \
      (long*) Malloc(pcs_number_of_primary_nvals*sizeof(long));
    eqs->unknown_update_methods = \
       (int*) Malloc(pcs_number_of_primary_nvals*sizeof(long));
    
    for(i=0;i<pcs_number_of_primary_nvals;i++) {
      eqs->unknown_vector_indeces[i] = GetNodeValueIndex(pcs_primary_function_name[i])+1;
	  eqs->unknown_node_numbers[i] = m_msh->GetNodesNumber(true);
      eqs->unknown_update_methods[i] = 2; //For Newton-Raphson method. WW
      if(type==41)  // Monolithic scheme for p-u coupled equation
      {
         if(i==pcs_number_of_primary_nvals-1)
            eqs->unknown_node_numbers[i] = m_msh->GetNodesNumber(false);
         eqs->unknown_update_methods[i] = 0;
      }
    }
}

/*************************************************************************
ROCKFLOW - Function:
Task: Calculate element matrices
Programming: 
05/2003 OK Implementation
09/2005 OK gas flow removed
last modified:
**************************************************************************/
void CRFProcess::CalculateElementMatrices(void) 
{
  switch(this->type) {
    case 1:
      SMCalcElementMatrices(this);
      break;
    case 2:
	  MakeStatMat_MTM2(this);
      break;
    case 3:
//      HTMCalcElementMatrices(this);
      break;
    case 5: // Gas flow
      break;
    case 11:
      MPCCalculateElementMatrices(this);
      break;
    case 12:
      if (pcs_type_number==0)
        if(aktueller_zeitschritt==0) 
          MMPCalcElementMatrices(this);
        else
          MMPCalcElementMatricesTypes(this);
      if (pcs_type_number==1) {
        MMSCalcElementMatrices(this);
      }
      break;
    case 13:
      //OK MPCCalculateElementMatricesRichards();
      break;
    case 66:
      OFCalcElementMatrices(this);
      break;
    default:
      DisplayMsgLn("CalculateElementMatrices: no CalculateElementMatrices specified");
      abort();
  }
}

/*************************************************************************
GeoSys-Function:
Task: Assemble the global system equation
Programming: 
01/2005 WW/OK Implementation
04/2005 OK MSH
07/2005 WW Change due to the geometry element objects applied
10/2005 OK DDC
11/2005 YD time step control
01/2006 OK/TK Tests
last modified:
**************************************************************************/
void CRFProcess::GlobalAssembly()
{
  //----------------------------------------------------------------------
  // Tests
  if(!Tim)
    Tim = TIMGet(pcs_type_name);
  if(!Tim)
  {
    cout << "Error in CRFProcess::GlobalAssembly() - no TIM data" << endl;
    return;
  }
  //......................................................................
  if(!fem)
  {
    // Which process needs this?
    // Only one instance of CFiniteElementStd is required for each process
    // Use "new" in such way will cause memery problem.
	// Please move this declaration to pcs configuration. 	  WW     
    if(m_msh) 
      fem = new CFiniteElementStd(this,m_msh->GetCoordinateFlag()); 
  }
  //----------------------------------------------------------------------
  long i;
  CElem* elem = NULL;
  bool Check2D3D;
  bool order = false;
  Check2D3D = false;
  if(M_Process) order = true;
  if(type == 66) //Overland flow
    Check2D3D = true;
#ifdef USE_MPI
  if(dom_vector.size()>0){
    cout << "      Domain Decomposition " << myrank  << '\n';
    CPARDomain* m_dom = NULL;
//    for(int j=0;j<(int)dom_vector.size();j++)
 //   {
//orig      m_dom = dom_vector[j];
      m_dom = dom_vector[myrank];
      SetLinearSolver(m_dom->eqs);
      SetZeroLinearSolver(m_dom->eqs);
      for(i=0;i<(long)m_dom->elements.size();i++)
      {
        elem = m_msh->ele_vector[m_dom->elements[i]];
        if(elem->GetMark())
        {
          elem->SetOrder(order);
          fem->ConfigElement(elem,Check2D3D);
          fem->m_dom = m_dom; //OK
          fem->Assembly();
        } 
      }
      // m_dom->WriteMatrix();
//    }
    //....................................................................
    // Assemble global system
    DDCAssembleGlobalMatrix();
//MXDumpGLS("rf_pcs.txt",1,eqs->b,eqs->x);
  }
#else
  //----------------------------------------------------------------------
  // DDC
  if(dom_vector.size()>0){
    cout << "      Domain Decomposition" << '\n';
    CPARDomain* m_dom = NULL;
    for(int j=0;j<(int)dom_vector.size();j++)
    {
      m_dom = dom_vector[j];
      SetLinearSolver(m_dom->eqs);
      SetZeroLinearSolver(m_dom->eqs);
      for(i=0;i<(long)m_dom->elements.size();i++)
      {
        elem = m_msh->ele_vector[m_dom->elements[i]];
        if(elem->GetMark())
        {
          elem->SetOrder(order);
          fem->ConfigElement(elem,Check2D3D);
          fem->m_dom = m_dom; //OK
          fem->Assembly();
        } 
      }
      // m_dom->WriteMatrix();
    }
    //....................................................................
    // Assemble global system
    DDCAssembleGlobalMatrix();
//MXDumpGLS("rf_pcs.txt",1,eqs->b,eqs->x);
  }
  //----------------------------------------------------------------------
  // STD
  else
  {
    for(i=0;i<(long)m_msh->ele_vector.size();i++)
    {
      elem = m_msh->ele_vector[i];
      if (elem->GetMark()) // Marked for use
      {
         elem->SetOrder(order);
        fem->ConfigElement(elem,Check2D3D);
/*
        for(i=0;i<fem->nnodes;i++){
          //eqs_number[i] = MeshElement->domain_nodes[i];
          fem->eqs_number[i] = fem->MeshElement->nodes[i]->GetEquationIndex();
        }
*/
        fem->Assembly();
//-----------------------NEUMANN CONTROL---------
	    if(Tim->time_control_name.compare("NEUMANN")==0)
        {
	      Tim->time_step_length_neumann = MMin(Tim->time_step_length_neumann,timebuffer);
          Tim->time_step_length_neumann *= 0.5*elem->GetVolume()*elem->GetVolume();
	      if(Tim->time_step_length_neumann < MKleinsteZahl)
            Tim->time_step_length_neumann = 1.0e-5;
	    }
//------------------------------   
      } 
    }
  }
  //----------------------------------------------------------------------
#endif
}

/*************************************************************************
GeoSys-Function:
Task: Calculate integration point velocity
Programming: 
08/2005 WW Implementation
last modified:
**************************************************************************/
void CRFProcess::CalIntegrationPointValue()
{
  long i;
  CElem* elem = NULL;
  cal_integration_point_value = true;
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
  {
    elem = m_msh->ele_vector[i];
    if (elem->GetMark()) // Marked for use
    {
       fem->ConfigElement(elem);
	   fem->Cal_Velocity();
    } 
  }
}


/*************************************************************************
ROCKFLOW - Function: AllocateLocalMatrixMemory
Task: As the function name
Programming: 
01/2005 WW/OK Implementation
06/2005 OK MSH
last modified:
**************************************************************************/
void CRFProcess::AllocateLocalMatrixMemory()
{
  long i;
  //----------------------------------------------------------------------
  int up_type = 0;
  if(!M_Process) up_type=0; 
  if(H_Process&&M_Process)
  { 
    if(type!=4&&type!=41)
      up_type=1;
    else
    {
      if(type==4)
        up_type = 3;
      if(type==41)  up_type = 4;
    }
  } 
  if(!H_Process) up_type = 2;
  //----------------------------------------------------------------------
  ElementMatrix *eleMatrix = NULL;
  CElem* elem = NULL;
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
  {
    elem = m_msh->ele_vector[i];
    if (elem->GetMark()) // Marked for use
	{
      eleMatrix = new ElementMatrix();
      eleMatrix->AllocateMemory(elem,up_type);
      Ele_Matrices.push_back(eleMatrix);
    }
  }
}

/*************************************************************************
FEMLib function
Task: Assemble global system matrix
Programming: 
05/2003 OK Implementation
??/???? WW Moved from AssembleSystemMatrixNew
**************************************************************************/
void CRFProcess::DDCAssembleGlobalMatrix()
{
  int k;
  long i,j,ig,jg;
  CPARDomain *m_dom = NULL;
  double a_ij;
  double b_i=0.0;
  int no_domains =(int)dom_vector.size();
  long no_dom_nodes;
#ifndef USE_MPI
  for(k=0;k<no_domains;k++){
    m_dom = dom_vector[k];
#else
    m_dom = dom_vector[myrank];
#endif
    no_dom_nodes = (long)m_dom->nodes.size();
    for(i=0;i<no_dom_nodes;i++){
      for(j=0;j<no_dom_nodes;j++){
        // get domain system matrix
        SetLinearSolver(m_dom->eqs);
        a_ij = MXGet(i,j);
        // set global system matrix
        SetLinearSolver(eqs);
        ig = m_dom->nodes[i];
        jg = m_dom->nodes[j];
        MXInc(ig,jg,a_ij);
      }
      // set global RHS vector //OK
      b_i = m_dom->eqs->b[i];
      eqs->b[ig] += b_i;
    }
#ifndef USE_MPI
  }
#endif
}

/*************************************************************************
ROCKFLOW - Function:
Task: Assemble system matrix
Programming: 05/2003 OK Implementation
ToDo: Prototyp function
last modified:
**************************************************************************/
void CRFProcess::AssembleSystemMatrixNew(void) 
{
  double ddummy=0.0;
  int no_phases = (int)mfp_vector.size();
  switch(type) {
    case 1:
      //MakeGS_ASM_NEW(eqs->b,eqs->x,ddummy);
      SMAssembleMatrix(eqs->b,eqs->x,ddummy,this);
      break;
    case 2:
//		  MakeGS_MTM2_old(this->eqs->b,this->eqs->x,this->pcs_component_number-1);
		MakeGS_MTM2(eqs->b, this);
      break;
    case 3:
//WW      HTMAssembleMatrix(this);
      break;
    case 5: // Gas flow
      break;
    case 11:
      MPCAssembleSystemMatrix(pcs_type_number,eqs->b);
      break;
    case 12:
      if (pcs_type_number==0)
        MMPAssembleMatrices(no_phases,eqs->x,eqs->b);
      if (pcs_type_number==1)
        MakeGS_MMSV1(eqs->b,eqs->x,ddummy);
      break;
    case 13:
      MPCAssembleSystemMatrix(1,eqs->b);
      break;
    case 66:
      //MakeGS_ASM_NEW(eqs->b,eqs->x,ddummy);
      SMAssembleMatrix(eqs->b,eqs->x,ddummy,this);
      break;
    default:
      DisplayMsgLn("CalculateElementMatrices: no CalculateElementMatrices specified");
      abort();
  }
#ifdef PARALLEL
  DDCAssembleGlobalMatrix();
#endif
//SetLinearSolver(eqs);
//MXDumpGLS("global_matrix_dd.txt",1,eqs->b,eqs->x);
}

/**************************************************************************
FEMLib-Method: CRFProcess::IncorporateBoundaryConditions
Task: set PCS boundary conditions
Programing:
02/2004 OK Implementation
????    WW  and time curve
04/2005 OK MSH
05/2005 OK conditional BCs
07/2007 WW Changes due to the geometry object applied
04/2006 OK Conditions by PCS coupling OK
last modification:
**************************************************************************/
void CRFProcess::IncorporateBoundaryConditions(const double Scaling)
{
  static long i, j;
  static double bc_value, fac = 1.0, time_fac = 1.0;
  long bc_msh_node;
  long bc_eqs_index;
  int interp_method=0;
  int curve, valid=0;
  int idx0, idx1; 
  CFunction* m_fct = NULL; //OK
  bool is_valid = false; //OK
  // If(m_msh), it is better to 
  //-----------------------------------------------------------------------
  CBoundaryConditionsGroup *m_bc_group = NULL;
  for(j=0; j<GetPrimaryVNumber(); j++)
  {
    m_bc_group = BCGetGroup(pcs_type_name,(string)pcs_primary_function_name[j]); //OK
    if(!m_bc_group) continue;
    if((type==4||type==41)&&(j<problem_dimension_dm)) fac = Scaling;
	else fac = 1.0;
    //-----------------------------------------------------------------------
    for(i=0;i<(long)m_bc_group->group_vector.size();i++) {
      bc_msh_node = m_bc_group->group_vector[i]->msh_node_number-Shift[j];
      if(m_msh) //OK
	    if(!m_msh->nod_vector[bc_msh_node]->GetMark()) //WW
          continue;
      time_fac = 1.0;
      if(bc_msh_node>=0){
        //................................................................
        // Time dependencies - CURVE
        curve =  m_bc_group->group_vector[i]->CurveIndex;
        if(curve>0){
          time_fac = GetCurveValue(curve,interp_method,aktuelle_zeit,&valid);
          if(!valid) continue;
	    }
	    else
          time_fac = 1.0;
        //................................................................
        // Time dependencies - FCT
        if(m_bc_group->fct_name.length()>0){
          m_fct = FCTGet(m_bc_group->fct_name);
          if(m_fct){
            time_fac = m_fct->GetValue(aktuelle_zeit,&is_valid);
            //if(!valid) continue;
          }
          else{
            cout << "Warning in CRFProcess::IncorporateBoundaryConditions - no FCT data" << endl;
          }
	    }
        //................................................................
        // Conditions
        if(m_bc_group->group_vector[i]->conditional){  
		  bc_value = time_fac*fac* GetNodeValue(m_bc_group->msh_node_number_subst,GetNodeValueIndex(pcs_primary_function_name[0])+1); //  bc_value = time_fac*fac* GetNodeVal(bc_msh_node+1,GetNODValueIndex(pcs_primary_function_name[0])+1); // YD-----TEST---
        }
        else
          bc_value = time_fac*fac*m_bc_group->group_vector[i]->node_value; // time_fac*fac*PCSGetNODValue(bc_msh_node,"PRESSURE1",0);
        //----------------------------------------------------------------
        // MSH
        if(m_msh){//OK             
          bc_eqs_index = m_msh->nod_vector[bc_msh_node]->GetEquationIndex();  //WW
          //..............................................................
          // NEWTON
          if(m_num->nls_method_name.find("NEWTON")!=string::npos
             ||	type==4||type==41	 ) {  //Solution is in the manner of increment !
		    idx0 = GetNodeValueIndex(pcs_primary_function_name[j]);
            if(type==4||type==41)  
            {
              idx1 = idx0+1;
              bc_value -= GetNodeValue(bc_msh_node, idx0) + GetNodeValue(bc_msh_node, idx1);               
		    }
            else
              bc_value = bc_value - GetNodeValue(bc_msh_node, idx0);
		  }
		}
        //----------------------------------------------------------------
        // RFI
        else {
          bc_eqs_index = GetNodeIndex(bc_msh_node);
          //..............................................................
          // NEWTON
          if(m_num->nls_method_name.find("NEWTON")!=string::npos){  //Solution vector is the increment !
            idx0 = PCSGetNODValueIndex(pcs_primary_function_name[j],0);
            if(type==4||type==41)  
            {
              idx1 = PCSGetNODValueIndex(pcs_primary_function_name[j],1);
              bc_value -= GetNodeVal(bc_eqs_index, idx0) + GetNodeVal(bc_eqs_index, idx1);               
		    }
            else
              bc_value = bc_value - GetNodeVal(bc_eqs_index, idx0);
		  }
        }
        //----------------------------------------------------------------
        bc_eqs_index += Shift[j];
		if((int)continuum_vector.size() > 1){
		   if(pcs_primary_function_name[continuum] == pcs_primary_function_name[j])
		      MXRandbed(bc_eqs_index,bc_value,eqs->b);
		}
		else
        MXRandbed(bc_eqs_index,bc_value,eqs->b);
      }
    }
  }
  //-----------------------------------------------------------------------
  /* irreg. Zeilen/Spalten regularisieren */
/*
  else if (GetNodeState(NodeNumber[i]) == -2 || GetNodeState(NodeNumber[i]) == -4) { // irreg.Knoten
    if (GetRFControlGridAdapt())
      if (AdaptGetMethodIrrNodes() == 1) {
        MXSet(i, i, MKleinsteZahl);
        rechts[i] = 0.0;
      }
    }
  }
*/
}

/**************************************************************************
FEMLib-Method:
Task: PCS source terms into EQS
Programing:
04/2004 OK Implementation
08/2004 WW Extension for monolithic PCS and time curve
last modification:
02/2005 MB River Condition and CriticalDepth
05/2005 WW Dynamic problems
07/2005 WW Changes due to the geometry object applied
03/2006 WW Re-arrange
04/2006 OK CPL
**************************************************************************/
void CRFProcess::IncorporateSourceTerms(const double Scaling)
{
  double value = 0, fac = 1.0, time_fac;
  int interp_method=0;
  int curve, valid=0;
  long msh_node, shift;
  long bc_eqs_index=-1;
  int  EleType;
  double q_face=0.0; 
  
  CElem* elem = NULL;
  CElem* face = NULL;
  ElementValue* gp_ele =NULL;
  double vel[3];   

  //-----------------------------------------------------------------------
  long i, group_vector_length;
  fac = 1.0;
  if(type==4) fac = Scaling;

  CNodeValue *cnodev = NULL;
  CSourceTerm *m_st = NULL;
  //
  group_vector_length = (long)st_node_value.size();
  //--------------------------------------------------------------------
  //WW
  for(i=0;i<(long)st_node_value.size();i++) {
    cnodev = st_node_value[i]; 
	msh_node = cnodev->msh_node_number;
	shift = cnodev->msh_node_number-cnodev->geo_node_number;
    msh_node -= shift; 
    value = cnodev->node_value;
    //
    if(msh_node<0) continue; 
    if(st_node.size()>0&&(long)st_node.size()>i)
	{
       m_st = st_node[i]; 
       //--------------------------------------------------------------------
       if (cnodev->node_distype==7){      // system dependent YD
         long no_st_ele = (long)m_st->element_st_vector.size();
         for(long i_st=0;i_st<no_st_ele;i_st++){
           long ele_index = m_st->element_st_vector[i_st];
           elem = m_msh->ele_vector[ele_index];
           if (elem->GetMark()){ 
              fem->ConfigElement(elem);
              fem->Cal_Velocity(); 
           }
           gp_ele = ele_gp_value[ele_index];
           gp_ele->GetEleVelocity(vel);
           EleType = elem->GetElementType();
           if(EleType == 1)   //Line
               cnodev->node_value += vel[0]; 
        //
           if(EleType == 4 || EleType == 2){   //Traingle & Qua
              for(long i_face=0;i_face < (long)m_msh->face_vector.size();i_face++)
              {
                 face = m_msh->face_vector[i_face];
                 if(m_st->element_st_vector[i_st] == face->GetOwner()->GetIndex())
                     q_face = PointProduction(vel,m_msh->face_normal[i_face])*face->GetVolume();   //   
                     //for(i_node) 
              }
              cnodev->node_value =+ q_face/2;
           }
           // cout<<"  value  "<<m_st_group->group_vector[i]->node_value<<endl;
         }
       }
      //--------------------------------------------------------------------
      // MB
       if(m_st->conditional && !m_st->river)
           value = GetConditionalNODValue(m_st, cnodev); //MB
      //--------------------------------------------------------------------
      // CMCD
       else if(m_st->analytical){
         value = GetAnalyticalSolution(i,m_st); //CMCD
       }
      //--------------------------------------------------------------------
      // MB
       if(cnodev->node_distype==5)       // River Condition
           value = GetRiverNODValue(cnodev, m_st, msh_node); //MB
    
       if(cnodev->node_distype==6)         // CriticalDepth Condition
           value = GetCriticalDepthNODValue(cnodev, m_st, msh_node); //MB
    
       if(cnodev->node_distype == 8)      // NormalDepth Condition JOD
           value = GetNormalDepthNODValue(m_st, msh_node); //MB        
	}
      //--------------------------------------------------------------------
      // FCT
    curve = cnodev->CurveIndex;
    if(curve>0) {
       time_fac = GetCurveValue(curve,interp_method,aktuelle_zeit,&valid);
       if (!valid)  {
          cout<<"\n!!! Time dependent curve is not found. Results are not guaranteed "<<endl;
          cout<<" in void CRFProcess::IncorporateSourceTerms(const double Scaling)"<<endl;
          time_fac = 1.0;
       }
    }
    else time_fac = 1.0;
        //--------------------------------------------------------------------
    value *= time_fac* fac;
        //--------------------------------------------------------------------
        // EQS->RHS
    if(m_msh) //WW
        bc_eqs_index = m_msh->nod_vector[msh_node]->GetEquationIndex()+shift;       
    else 
        bc_eqs_index = GetNodeIndex(msh_node)+shift;
     //
   if((int)continuum_vector.size() > 1){
      if(pcs_primary_function_name[continuum] == pcs_primary_function_name[i])
          eqs->b[bc_eqs_index] += value;
   }
   else
    eqs->b[bc_eqs_index] += value;
  }
    //====================================================================
  }

  //-----------------------------------------------------------------------

/* // Commented by WW
void CRFProcess::IncorporateSourceTerms(const double Scaling)
{
  double value = 0, fac = 1.0, time_fac;
  int interp_method=0;
  int curve, valid=0;
  long msh_node, shift;
  long i, j;
  long bc_eqs_index=-1;
//WW  double H = 0.0;
  int  EleType;
  double q_face=0.0; 
  
  CElem* elem = NULL;
  CElem* face = NULL;
  ElementValue* gp_ele =NULL;
  double vel[3];   
  char *function_name[7];
  bool dynamic = false;
  int nv = 0; 
  shift = 0;
  if(pcs_type_name.find("DYNAMIC")!=string::npos){
    dynamic = true;
    if(max_dim==1){ // 2D
      nv = 5;
      function_name[0] = "DISPLACEMENT_X1";
      function_name[1] = "DISPLACEMENT_Y1";
      function_name[2] = "VELOCITY_X1";
      function_name[3] = "VELOCITY_Y1";
      function_name[4] = "PRESSURE1"; 
    }
    else { //3D
      nv =7;
      function_name[0] = "DISPLACEMENT_X1";
      function_name[1] = "DISPLACEMENT_Y1";
      function_name[2] = "DISPLACEMENT_Z1";
      function_name[3] = "VELOCITY_X1";
      function_name[4] = "VELOCITY_Y1";
      function_name[5] = "VELOCITY_Z1";
      function_name[6] = "PRESSURE1"; 
    }
  }
  else{
    nv = GetPrimaryVNumber();
    for(j=0; j<nv; j++)  
      function_name[j] =pcs_primary_function_name[j];        
  }

  //-----------------------------------------------------------------------
  CSourceTermGroup *m_st_group = NULL;
  CSourceTerm *m_st = NULL;
  for(j=0; j<nv; j++)  {
     if((type==4||type==41)&&j<problem_dimension_dm) fac = Scaling;
	 else fac = 1.0;
//WW     m_st_group = m_st_group->Get((string)function_name[j]);
     m_st_group = STGetGroup(pcs_type_name,(string)function_name[j]);
     if(!m_st_group) continue;
     //-----------------------------------------------------------------------
     long group_vector_length = (long)m_st_group->group_vector.size();
     //--------------------------------------------------------------------
	 
     //WW
     if(dynamic) 
     {
        if(j==nv-1) shift = Shift[max_dim+1];
		else shift = Shift[(j+max_dim+1)%(max_dim+1)];
     }
	 else
        shift = Shift[j]; 
    //--------------------------------------------------------------------
    for(i=0;i<group_vector_length;i++) {
      msh_node = m_st_group->group_vector[i]->msh_node_number-shift;
      m_st = m_st_group->st_group_vector[i]; 
    //--------------------------------------------------------------------
          if (m_st_group->group_vector[i]->node_distype==7){      // system dependent YD
           long no_st_ele = (long)m_st->element_st_vector.size();
		   for(long i_st=0;i_st<no_st_ele;i_st++){
              long ele_index = m_st->element_st_vector[i_st];
              elem = m_msh->ele_vector[ele_index];
			  if (elem->GetMark()){ 
                 fem->ConfigElement(elem);
                 fem->Cal_Velocity(); 
				 }
              gp_ele = ele_gp_value[ele_index];
              gp_ele->GetEleVelocity(vel);
              EleType = elem->GetElementType();
              if(EleType == 1){   //Line
              m_st_group->group_vector[i]->node_value += vel[0]; 
              }
              if(EleType == 4 || EleType == 2){   //Traingle & Qua
                 for(long i_face=0;i_face < (long)m_msh->face_vector.size();i_face++){
                   face = m_msh->face_vector[i_face];
                   if(m_st->element_st_vector[i_st] == face->GetOwner()->GetIndex())
                      q_face = PointProduction(vel,m_msh->face_normal[i_face])*face->GetVolume();   //   
                  //for(i_node) 
                 }
			  m_st_group->group_vector[i]->node_value =+ q_face/2;
              }
          // cout<<"  value  "<<m_st_group->group_vector[i]->node_value<<endl;
          }
         }
    //--------------------------------------------------------------------
      if(m_st->conditional && !m_st->river){
        value = m_st_group->GetConditionalNODValue(i,m_st); //MB
      }
      else if(m_st->analytical){
        m_st_group->m_msh = m_msh;
        value = m_st_group->GetAnalyticalSolution(m_st,msh_node,(string)function_name[j]);
      }
      else {
        value = m_st_group->group_vector[i]->node_value;
      } 

      if(m_st_group->group_vector[i]->node_distype==5)  {       // River Condition
        value = m_st_group->GetRiverNODValue(i,m_st, msh_node); //MB
      }
      if(m_st_group->group_vector[i]->node_distype==6)  {       // CriticalDepth Condition
        value = m_st_group->GetCriticalDepthNODValue(i,m_st, msh_node); //MB
      }
      if(m_st_group->group_vector[i]->node_distype == 8)  {     // NormalDepth Condition JOD
        value = m_st_group->GetNormalDepthNODValue(i,m_st, msh_node); //MB        
	  } 

      if(msh_node>=0) {
        curve = m_st_group->group_vector[i]->CurveIndex;
        if(curve>0) {
          time_fac = GetCurveValue(curve,interp_method,aktuelle_zeit,&valid);
        if (!valid)  {
	        cout<<"\n!!! Time dependent curve is not found. Results are not guaranteed "<<endl;
	        cout<<" in void CRFProcess::IncorporateSourceTerms(const double Scaling)"<<endl;
          time_fac = 1.0;
        }
      }
      else time_fac = 1.0;
        
       
        value *= time_fac* fac;
       
        if(m_msh) //WW
           bc_eqs_index = m_msh->nod_vector[msh_node]->GetEquationIndex()+shift;       
		else 
           bc_eqs_index = GetNodeIndex(msh_node)+shift;
   
        eqs->b[bc_eqs_index] += value;
      }
    }
  //.......................................
  }
  //-----------------------------------------------------------------------
}
*/
/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
02/2004 OK Implementation
11/2004 OK NUM
last modification:
**************************************************************************/
int CRFProcess::ExecuteLinearSolver(void)
{
  long iter_count;
  long iter_sum = 0;
  int found = 0;
  //-----------------------------------------------------------------------
  // Set EQS
  SetLinearSolver(eqs);
  //--------------------------------------------------------------------
  // NUM
  found = 1;
  cg_maxiter        = m_num->ls_max_iterations; //OK lsp->maxiter;
  cg_eps            = m_num->ls_error_tolerance; //OK lsp->eps;
  //cg_repeat = lsp->repeat;
  vorkond           = m_num->ls_precond; //OK lsp->precond;
  linear_error_type = m_num->ls_error_method; //OK lsp->criterium;
  iter_count = eqs->LinearSolver(eqs->b,eqs->x,eqs->dim);
  eqs->master_iter = iter_count;
  if(iter_count>=cg_maxiter) {
    cout << "Warning in CRFProcess::ExecuteLinearSolver() - Maximum iteration number reached" << endl;
    return -1;
  }
  iter_sum += iter_count;
  //-----------------------------------------------------------------------
  // Clean results ?
/*
  for (i=0;i<eqs->dim;i++)
    if (fabs(eqs->x[i])<MKleinsteZahl)
      eqs->x[i] = 0.0;
*/
  //-----------------------------------------------------------------------
  return iter_sum;
}
/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
06/2005 PCH Overriding
last modification:
**************************************************************************/
int CRFProcess::ExecuteLinearSolver(LINEAR_SOLVER *eqs)
{
  long iter_count;
  long iter_sum = 0;
  int found = 0;
  //-----------------------------------------------------------------------
  // Set EQS
  SetLinearSolver(eqs);
  //--------------------------------------------------------------------
  // NUM
  found = 1;
  cg_maxiter        = m_num->ls_max_iterations; //OK lsp->maxiter;
  cg_eps            = m_num->ls_error_tolerance; //OK lsp->eps;
  //cg_repeat = lsp->repeat;
  vorkond           = m_num->ls_precond; //OK lsp->precond;
  linear_error_type = m_num->ls_error_method; //OK lsp->criterium;

  iter_count = eqs->LinearSolver(eqs->b,eqs->x,eqs->dim);
  eqs->master_iter = iter_count;
  if(iter_count>=cg_maxiter) {
    cout << "Warning in CRFProcess::ExecuteLinearSolver() - Maximum iteration number reached" << endl;
    return -1;
  }
  iter_sum += iter_count;
  //-----------------------------------------------------------------------
  return iter_sum;
}

//WW
int CRFProcess::GetNODValueIndex(string name,int timelevel)
{
  for(int j=0;j<number_of_nvals;j++){
     if((name.compare(pcs_nval_data[j].name)==0) && \
         (pcs_nval_data[j].timelevel==timelevel))
        return pcs_nval_data[j].nval_index;
   }
  cout << "Error in PCSGetNODValueIndex: " << name << endl;
  return -1;
}


///////////////////////////////////////////////////////////////////////////
// Specials
///////////////////////////////////////////////////////////////////////////

/*-------------------------------------------------------------------------
ROCKFLOW - Function: PCSRestart
Task: Insert process to list
Programming: 
06/2003 OK Implementation
11/2004 OK file_name_base
last modified:
-------------------------------------------------------------------------*/
void PCSRestart()
{
  int j;
  CRFProcess *m_pcs = NULL;
//  int timelevel;
  int nidx0,nidx1;
  int i;
  int no_processes =(int)pcs_vector.size();
  if(no_processes==0)
    return; //OK41
  int ok;
  //----------------------------------------------------------------------
  string file_name_base = pcs_vector[0]->file_name_base;
  ok = ReadRFRRestartData(file_name_base);
  if(ok==0){
    cout << "RFR: no restart data" << endl;
    return;
  }
  //----------------------------------------------------------------------
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    for(j=0;j<m_pcs->GetPrimaryVNumber();j++) {
      // timelevel=0;
      nidx0 = m_pcs->GetNodeValueIndex(m_pcs->GetPrimaryVName(j));
      // timelevel= 1;
      nidx1 = nidx0+1;
      CopyNodeVals(nidx1,nidx0);
    }
  }
}

/**************************************************************************
FEMLib-Method: 
Task: Relocate Deformation process
Programing:
09/2004 WW Implementation
**************************************************************************/
void RelocateDeformationProcess(CRFProcess *m_pcs)
{
   string pcs_name_dm = m_pcs->pcs_type_name; 
   string num_type_name_dm; 
   // Numerics
   if(m_pcs->num_type_name.compare("STRONG_DISCONTINUITY")==0) 
   {
       num_type_name_dm = m_pcs->num_type_name;
       enhanced_strain_dm=1;
   }
   delete m_pcs;
   m_pcs = dynamic_cast<CRFProcess *> (new CRFProcessDeformation());
   m_pcs->pcs_type_name = pcs_name_dm;
   if(enhanced_strain_dm==1) m_pcs->num_type_name = num_type_name_dm;
   pcs_deformation = 1;
}

/*************************************************************************
ROCKFLOW - Function: CreateFDMProcess
Task:
Programming: 
06/2004 OK Implementation
06/2004 OK 1-D FDM
last modified:
**************************************************************************/
void CRFProcess::CreateFDMProcess()
{
  long i;
  //----------------------------------------------------------------------
  // create FDM nodes and relationships
  start_new_elems = ElListSize();
  ConstructElemsToNodesList();
  //============================================================================
  CFDMNode *m_FDM_node;
/*
  long *elems1d;
  int num_elems1d;
  long *nod_neighbors;
  long *ele_neighbors;
  Knoten *node = NULL;
  long *element_nodes;
  for(i=0;i<NodeListLength;i++){
    node = GetNode(i);
    if(node->anz1d>0){
      elems1d = GetNode1DElems(i,&num_elems1d);
      nod_neighbors = new long[num_elems1d];
      ele_neighbors = elems1d;
      l=0;
      for(j=0;j<num_elems1d;j++){
        element_nodes = ElGetElementNodes(elems1d[j]);
        for(k=0;k<ElGetElementNodesNumber(elems1d[j]);k++) {
          if(element_nodes[k]!=i){
            nod_neighbors[l]=element_nodes[k];
            l++;
          }
        }
      }
      m_FDM_node = new CFDMNode;
      m_FDM_node->node = i;
      m_FDM_node->nod_neighbors = nod_neighbors;
      m_FDM_node->ele_neighbors = ele_neighbors;
      m_FDM_node->no_neighbors = num_elems1d;
      FDM_node_vector.push_back(m_FDM_node);
    }
  }
*/
  //----------------------------------------------------------------------------
  long *nodes_sorted;
  CGLPolyline *m_polyline = NULL;
  m_polyline = GEOGetPLYByName("RIVER");//CC
if(m_polyline){
  //ToDo nodes_sorted = m_polyline->GetMHSNodesSorted(nodes_unsorted,&no_nodes);
  long no_nodes = 0;
  long *nodes_unsorted = NULL;
   // encapsulate sort function
///OK  nodes_unsorted = m_polyline->MSHGetNodesClose(&no_nodes);
  double pt1[3],pt2[3];
  pt1[0] = m_polyline->point_vector[0]->x;
  pt1[1] = m_polyline->point_vector[0]->y;
  pt1[2] = m_polyline->point_vector[0]->z;
  double *node_distances = NULL;
  node_distances = new double[no_nodes];
  for(i=0;i<no_nodes;i++) {
    pt2[0] = GetNodeX(nodes_unsorted[i]);
    pt2[1] = GetNodeY(nodes_unsorted[i]);
    pt2[2] = GetNodeZ(nodes_unsorted[i]);
    node_distances[i] = MCalcDistancePointToPoint(pt1,pt2);
  }
  nodes_sorted = TOLSortNodes1(nodes_unsorted,node_distances,no_nodes);
  delete [] node_distances;
  //delete [] nodes_unsorted;
  for(i=0;i<no_nodes;i++){
    m_FDM_node = new CFDMNode;
    m_FDM_node->node = nodes_sorted[i];
    FDM_node_vector.push_back(m_FDM_node);
  }
  //delete [] nodes_sorted;

}
  WriteFDMNodes("river");
  //----------------------------------------------------------------------------
  // PCS
  Config();
  //----------------------------------------------------------------------------
  // TIM
  int timelevel;
  //----------------------------------------------------------------------------
  // NUM 
  //----------------------------------------------------------------------------
  // SOL
  //----------------------------------------------------------------------------
  // BC
   // Set BC names to BC list
/*OK
  CBoundaryConditionsGroup *m_bc_group = NULL;
  for(i=0;i<pcs_number_of_primary_nvals;i++) {
    strcpy(c_string,pcs_primary_function_name[i]);
    if(!BCGroupExists(c_string))
       SetBoundaryConditions(c_string);
    m_bc_group = new CBoundaryConditionsGroup();
    m_bc_group->Set(pcs_primary_function_name[i]);
    bc_group_list.push_back(m_bc_group);
  }
*/
  //----------------------------------------------------------------------------
  // ST
  CSourceTermGroup *m_st_group = NULL;
  for(i=0;i<pcs_number_of_primary_nvals;i++){
    m_st_group = m_st_group->Get(pcs_primary_function_name[i]);
    if(!m_st_group) {
      m_st_group = new CSourceTermGroup();
      m_st_group->Set(this, 0); //OK
      st_group_list.push_back(m_st_group);
    }
  }
  //----------------------------------------------------------------------------
  // MAT
  //----------------------------------------------------------------------------
  // EQS
  //----------------------------------------------------------------------------
  // Set source terms: EQS<->ST (eqs->b)
  // Set boundary conditions: EQS<->BC
  //----------------------------------------------------------------------------
  // MOD -> Kernel
  //----------------------------------------------------------------------------
  /* NOD - model node values */
  ConfigNODValues1(); 
  ConfigNODValues2();
  CreateNODValues();
  //----------------------------------------------------------------------------
  /* IC<->NOD */    
  int no_timelevel = 2; // pcs variable
  int nidx;
  timelevel=0;
  for(i=0;i<pcs_number_of_primary_nvals;i++) {
    nidx = pcs_nval_data[no_timelevel*i+timelevel].nval_index;
///OK    ICSetNODValues(pcs_primary_function_name[i],nidx);
    CopyNodeVals(nidx,nidx+1);
  }
///OK  SMSaintVenantHeadFTCS1D(this);
}

void CRFProcess::DestroyFDMProcess()
{
}

/*************************************************************************
ROCKFLOW - Function: CRFProcess::PCSMoveNOD
Task: 
Programming: 
08/2004 MB/OK Implementation
last modified:
**************************************************************************/
void CRFProcess::PCSMoveNOD(void) 
{
  
  switch(this->type) {
    case 1:
      MSHMoveNODUcFlow(this);
      break;
    default:
      DisplayMsgLn("PCSMoveNOD: no valid process");
      abort();
  }
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
09/2004 OK Implementation
10/2004 OK 2nd version
**************************************************************************/
string PCSProblemType()
{
  string pcs_problem_type;
  int i;
  CRFProcess* m_pcs = NULL;
  int no_processes =(int)pcs_vector.size();
  //----------------------------------------------------------------------
  // H process
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    switch(m_pcs->pcs_type_name[0]){
      case 'L':
        pcs_problem_type = "LIQUID_FLOW";
        break;
     // case 'U':
      //  pcs_problem_type = "UNCONFINED_FLOW";
      //  break;
      case 'O':
        pcs_problem_type = "OVERLAND_FLOW";
        break;
      case 'G':
        pcs_problem_type = "GROUNDWATER_FLOW";
        break;
      case 'T':
        pcs_problem_type = "TWO_PHASE_FLOW";
        break;
      case 'C':
        pcs_problem_type = "COMPONENTAL_FLOW";
        break;
	  case 'R':  //MX test 04.2005
        pcs_problem_type = "RICHARDS_FLOW";
        break;
    }
  }
  //----------------------------------------------------------------------
  // M process
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    switch(m_pcs->pcs_type_name[0]){
      case 'D':
        if(pcs_problem_type.empty())
          pcs_problem_type = "DEFORMATION";
        else
          pcs_problem_type += "+DEFORMATION";
        break;
    }
  }
  //----------------------------------------------------------------------
  // T process
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    switch(m_pcs->pcs_type_name[0]){
      case 'H':
        if(pcs_problem_type.empty())
          pcs_problem_type = "HEAT_TRANSPORT";
        else
          pcs_problem_type += "+HEAT_TRANSPORT";
        break;
    }
  }
  //----------------------------------------------------------------------
  // CB process
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    switch(m_pcs->pcs_type_name[0]){
      case 'M':
        if(pcs_problem_type.empty())
          pcs_problem_type = "MASS_TRANSPORT";
        else
          pcs_problem_type += "+MASS_TRANSPORT";
        break;
    }
  }
  //----------------------------------------------------------------------
  //----------------------------------------------------------------------
  // FM process
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    switch(m_pcs->pcs_type_name[0]){
      case 'F':
        if(pcs_problem_type.empty())
          pcs_problem_type = "FLUID_MOMENTUM";
        else
          pcs_problem_type += "+FLUID_MOMENTUM";
        break;
    }
  }
  //----------------------------------------------------------------------
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    switch(m_pcs->pcs_type_name[7]){    // pcs_type_name[7] should be 'W' because 'R' is reserved for Richard Flow.
      case 'W':
        if(pcs_problem_type.empty())
          pcs_problem_type = "RANDOM_WALK";
        else
          pcs_problem_type += "+RANDOM_WALK";
        break;
    }
  }
  return pcs_problem_type;
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
11/2004 OK Implementation
**************************************************************************/
void CRFProcess::CalcELEVelocities(void)
{
  long e;
  double theta = 1.0;
  if(m_msh){
     CElem* elem = NULL;
     for (e = 0; e < (long)m_msh->ele_vector.size(); e++)
     {
        elem = m_msh->ele_vector[e];
        if (elem->GetMark()) // Marked for use
        {
           //ToDo VELCalcElementVelocity(e,theta,pcs_type_number);
        }
	 }
  }
  else{
    for(e=0;e<ElementListLength;e++){
      VELCalcElementVelocity(e,theta,pcs_type_number);
    }
  }
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
11/2004 OK Implementation
**************************************************************************/
void CRFProcess::CalcELEMassFluxes(void)
{
  int i;
  double e_value = -1.0;
  string e_value_name;
  double geo_factor, density;
  double velocity = 0.0;
  int e_idx;
  int phase = 0;
  long e;
  CMediumProperties* m_mmp = NULL;
  CFluidProperties* m_mfp = NULL;
  m_mfp = mfp_vector[phase]; //OK ToDo
  //======================================================================
  for(e=0;e<ElementListLength;e++){
    m_mmp = mmp_vector[ElGetElementGroupNumber(e)];
    geo_factor = m_mmp->geo_area;
    density = m_mfp->Density();
    for(i=0;i<pcs_number_of_evals;i++){
      e_value_name = pcs_eval_data[i].name; 
      e_idx = PCSGetELEValueIndex(pcs_eval_data[i].name);
      if(e_value_name.find("MASS_FLUX1_X")!=string::npos){
        velocity = ElGetElementVal(e,PCSGetELEValueIndex("VELOCITY1_X"));
        e_value = geo_factor * density * velocity;
        ElSetElementVal(e,e_idx,e_value);
      }
      if(e_value_name.find("MASS_FLUX1_Y")!=string::npos){
        velocity = ElGetElementVal(e,PCSGetELEValueIndex("VELOCITY1_Y"));
        e_value = geo_factor * density * velocity;
        ElSetElementVal(e,e_idx,e_value);
      }
      if(e_value_name.find("MASS_FLUX1_Z")!=string::npos){
        velocity = ElGetElementVal(e,PCSGetELEValueIndex("VELOCITY1_Z"));
        e_value = geo_factor * density * velocity;
        ElSetElementVal(e,e_idx,e_value);
      }
    }
  }
  //======================================================================
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
02/2005 OK Implementation
02/2006 YD Dual Richards
**************************************************************************/
void CRFProcess::CalcSecondaryVariables(int time_level)
{
  char pcsT;
  pcsT = pcs_type_name[0];
  switch(pcsT){
    case 'L':
      break;
    case 'U':
      break;
    case 'G':
      break;
    case 'T':
      break;
    case 'C':
      break;
    case 'R': // Richards flow
	  if(pcs_type_name[1] == 'I')	// PCH To make a distinction with RANDOM WALK.
		CalcSecondaryVariablesRichards(time_level,false);
      break;
    case 'D':
      break;
  }
}

//////////////////////////////////////////////////////////////////////////
// ReMove site
//////////////////////////////////////////////////////////////////////////

/*****************************************************/

/*****************************************************/
/*  Function: GetStartPositionInUnknownArray_Pressure
    Means as the name.
    For monolithic scheme of p-u coupled equation
    08/2003  WW 
*/  

long GetStartPositionInUnknownArray_Pressure(void)
{
  return problem_dimension_dm*GetHighOrderNodeNumber(); 
}

/**************************************************************************
 ROCKFLOW - Funktion: DumpModelNodeValues

 Aufgabe:
 Schreiben von Modell-Knoten-Daten  

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   - void -

 Ergebnis:
   - void -

 Programmaenderungen:
   09/2001   OK   Erste Version
   09/2002   OK   Spaltenvorgabe

**************************************************************************/
void DumpModelNodeValues(int columns)
{
  long i;
  int j;
  int nv=columns;

  DisplayMsgLn(" ");

  printf(" %6s ","Nodes");
  for (j=0;j<nv+1;j++) {
    if (nval_data[j].name) 
      printf(" %13s ",nval_data[j].name);
  }
  DisplayMsgLn(" ");

  for (i=0;i<NodeListLength;i++) {
    printf(" %6ld ",i);
    for (j=0;j<nv+1;j++) {
      printf(" %13.5g ",GetNodeVal(i,j));
    }
    DisplayMsgLn(" ");
  }
}

void CRFProcess::PCSDumpModelNodeValues(void)
{
  DumpModelNodeValues(pcs_number_of_primary_nvals+pcs_number_of_secondary_nvals);
}

/*************************************************************************
ROCKFLOW - Function: GetCompNamehelp
Task: Namepatch, until primary function names are finally sorted out //SB:todo
Programming:	08/2003 SB Implementation
last modified:
superseded by GetPFNamebyCPName() but left here, as not all files are already in the new concept
**************************************************************************/
/* SB: namepatch
	Repariert kurzfristig die Ausgabe
	input: datafield_n[j].name
	wenn dar Name "CONCENTRATIONx" ist, wird er durch den enstprechenden Komponentennamen ersetzt, sonst bleibts */

char *GetCompNamehelp(char *inname){
 int comp, phase;
 char * outname, help[MAX_ZEILE];
 CRFProcess *m_pcs=NULL;
 outname = inname;
 phase = 0;
 for(comp=0;comp<GetRFProcessNumComponents();comp++) {
	sprintf(help,"%s%d","CONCENTRATION",comp+1);
	/*  help has to be a part of inname (strstr) and also have the same length (strcmp) */
	if(strstr(inname, help) && (strcmp(inname,help) == 0)){
		m_pcs = m_pcs->GetProcessByFunctionName(help);
		if(m_pcs == NULL) break;
//		outname = GetTracerCompName(phase,m_pcs->GetProcessComponentNumber()-1);
//		outname = cp_vec[m_pcs->GetProcessComponentNumber()-1]->compname;
		outname = (char *) cp_vec[m_pcs->GetProcessComponentNumber()-1]->compname.data();
		return outname;
	};
 };
 return outname;
} //SB:namepatch

/*************************************************************************
ROCKFLOW - Function: GetCPNamebyPFName(string )
Task: Replaces CP Name by Primary function name for output input
Input:	component property name
Output: primary function name
Programming:	10/2004 SB Implementation
**************************************************************************/
string GetPFNamebyCPName(string inname){
 int i, j, k;
 int pcs_vector_size = (int) pcs_vector.size();
 string outname;
 char  help[MAX_ZEILE];
 CRFProcess *m_pcs=NULL;
 outname = "dummy";
 if(pcs_vector_size>0)
 for(i=0;i<pcs_vector_size;i++){
	m_pcs = pcs_vector[i];
	if(m_pcs->pcs_type_name.compare("MASS_TRANSPORT") == 0){ // if this is mass transport
		j = m_pcs->GetProcessComponentNumber();
		k = cp_vec[j]->transport_phase;
		outname = cp_vec[m_pcs->GetProcessComponentNumber()]->compname;
		if(outname == inname){ //right process found
			sprintf(help,"%s%d","CONCENTRATION",j);
			outname = help;
			return outname;}
	}
 }
// for(i=0;comp<GetRFProcessNumComponents();i++) {
//	sprintf(help,"%s%d","CONCENTRATION",i);
	/*  help has to be a part of inname (strstr) and also have the same length (strcmp) */
//	if(strstr(inname, help) && (strcmp(inname,help) == 0)){
//		m_pcs = m_pcs->GetProcessByFunctionName(help);
//		if(m_pcs == NULL) break;
//		outname = cp_vec[m_pcs->GetProcessComponentNumber()-1]->compname;
//		outname = (char *) cp_vec[m_pcs->GetProcessComponentNumber()-1]->compname.data();
//		if(outname.compare(inname) == 0)
//			return outname;
//	};
// };
// Inname is not from a mass transport process, therefore return inname
 return inname;
} //SB:namepatch

/*************************************************************************
ROCKFLOW - Function: CRFProcess::PCSOutputNODValues(FILE *f)
Task: 
Programming: 11/2003 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::PCSOutputNODValues(void)
{
  int i;
  long j;
  //
  FILE *f=NULL;
  f = fopen("pcs_out.rfo","a");
  // Kopf schreiben
  fprintf(f,"%-ld ",(long)1);
  fprintf(f,"%-ld ",(long)danz_sum_n);
  fprintf(f,"%-ld\n",(long)danz_sum_e);
  // loop over all PCS
  CRFProcess *m_process=NULL;
  int no_processes =(int)pcs_vector.size();
  for(i=0;i<no_processes;i++){
    m_process = pcs_vector[i];
	for(i=0;i<m_process->number_of_nvals;i++) {
	  if(m_process->pcs_nval_data[i].speichern) {
	    fprintf(f,"%s, ",GetCompNamehelp(m_process->pcs_nval_data[i].name)); //SB: namepatch
	    fprintf(f,"%s \n",m_process->pcs_nval_data[i].einheit);
	  }
    }
  }
  for (j=0l;j<NodeListSize();j++) {
    if (GetNode(j)!=NULL) {
      fprintf(f,"%-ld ",j);
      // loop over all PCS
      for(i=0;i<no_processes;i++){
        m_process = pcs_vector[i];
	    for(i=0;i<m_process->number_of_nvals;i++) {
	      if(m_process->pcs_nval_data[i].speichern)
  	        fprintf(f,"%-#*.*g ",FPD_GESAMT,FPD_NACHKOMMA,GetNodeVal(j,m_process->pcs_nval_data[i].nval_index));
		}
	  }
    }
    fprintf(f,"\n");
  }
  //fclose(f);
}

/*************************************************************************
ROCKFLOW - Function: MTM2CalcSecondaryVariables
Task: Calculate secondary variables for Transport Process MTM2
Programming: 08/2003 SB Implementation
ToDo: 
last modified:
**************************************************************************/
void MTM2CalcSecondaryVariables(void)
{
	/* SB: todo */
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
??/2004 MX Implementation
**************************************************************************/
 void CRFProcess::PCSSetTempArry(void){    
    int i;
    TempArry = (double *) Malloc(ElListSize() * sizeof(double));

    for (i=0;i<ElListSize();i++) {
      if (ElGetElement(i)!=NULL)
        TempArry[i] = 0.0;
      }
}

/*  SB: eingef?t */
/**************************************************************************
   ROCKFLOW - Funktion: PCSGetNODConcentration

   Aufgabe:
   Returns concentration at node of requested component

   Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
		index: Index des Knotens, dessen Wert geholt werden soll
		component: Komponente des Prozesses
		timelevel: alte/neue Zeitebene
   Ergebnis:
		value: Knotenwert

   Programmaenderungen:
   08/2003     SB         Erste Version

**************************************************************************/
double PCSGetNODConcentration(long index, long component, long timelevel)
{
  char name[MAX_ZEILE];
  sprintf(name,"%s%i","CONCENTRATION", (int)component); //SB:todo global name
  return PCSGetNODValue(index,name,timelevel);
}

void PCSSetNODConcentration(long index, long component, long timelevel, double value)
{
  char name[MAX_ZEILE];
  sprintf(name,"%s%i","CONCENTRATION", (int)component);
  SetNodeVal(index, PCSGetNODValueIndex(name,timelevel),value);
}

//========================================================================
//OK former model functions
int GetRFControlGridAdapt(void)
{
//OK  return (get_rfcp_adaptive_mesh_refinement_flag(rfcp));
  if(show_onces_adp)
    cout << "GetRFControlGridAdapt - to be removed" << endl;
  show_onces_adp = false;
  return 0;
}

int GetRFControlModel(void)
{
  if(show_onces_mod)
    cout << "GetRFControlModel - to be removed" << endl;
  show_onces_mod = false;
  return -1;
}

int GetRFProcessChemicalModel(void)
{
  cout << "GetRFProcessChemicalModel - to be removed" << endl;
  return 0;
}

int GetRFProcessFlowModel(void)
{
  if(show_onces_mod_flow)
    cout << "GetRFProcessFlowModel - to be removed" << endl;
  show_onces_mod_flow = false;
  return 0;
}

int GetRFProcessHeatReactModel(void)
{
  cout << "GetRFProcessHeatReactModel - to be removed" << endl;
  return 0;
}

int GetRFProcessNumPhases(void)
{
  //DisplayMsgLn("GetRFProcessNumPhases - to be removed");
  int no_phases = (int)mfp_vector.size();
  return no_phases;
}

int GetRFProcessProcessing(char* rfpp_type)
{
  //----------------------------------------------------------------------
  int i;
  bool pcs_flow = false;
  bool pcs_deformation = false;
  CRFProcess* m_pcs = NULL;
  int no_processes = (int)pcs_vector.size();
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    if(m_pcs->pcs_type_name.find("DEFORMATION")!=string::npos)
      pcs_deformation = true;
    if(m_pcs->pcs_type_name.find("FLOW")!=string::npos)
      pcs_flow = true;
  }
  //----------------------------------------------------------------------
  if(strcmp(rfpp_type,"SD")==0){
    if(pcs_flow&&pcs_deformation)
      return 1;
  }
  else
    cout << "GetRFProcessProcessing - to be removed" << endl;
  return 0;
}

int GetRFProcessProcessingAndActivation(char*)
{
  cout << "GetRFProcessProcessingAndActivation - to be removed" << endl;
  return 0;
}

long GetRFProcessNumComponents(void)
{
  //DisplayMsgLn("GetRFProcessNumComponents - to be removed");
  int no_components = (int)cp_vec.size();
  return no_components;
}

int GetRFControlModex(void)
{
  cout << "GetRFControlModex - to be removed" << endl;
  return 0;
}

int GetRFProcessDensityFlow(void)
{
  if(show_onces_density)
    cout << "GetRFProcessDensityFlow - to be removed" << endl;
  show_onces_density = false;
  return 0;
}

int GetRFProcessNumContinua(void)
{
  cout << "GetRFProcessNumContinua - to be removed" << endl;
  return 0;
}

int GetRFProcessNumElectricFields(void)
{
  cout << "GetRFProcessNumElectricFields - to be removed" << endl;
  return 0;
}

int GetRFProcessNumTemperatures(void)
{
  cout << "GetRFProcessNumTemperatures - to be removed" << endl;
  return -1;
}

int GetRFProcessSimulation(void)
{
  cout << "GetRFProcessSimulation - to be removed" << endl;
  return -1;
}

/**************************************************************************
   ROCKFLOW - Funktion: ModelsAddNodeValInfoStructure

   Aufgabe:
   Fuellt die Knotendaten-Infostruktur mit den zugehoerigen Modelldaten.
   Wird vom Modell der Reihe nach fuer jede Knotengroesse aufgerufen.

   Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E:char *name         :Name der Knotengroesse fuer Ergebnisdatei
   E:char *einheit      :Name der phys. Einheit fuer Ergebnisdatei
   E:int speichern      :Werte sollen gespeichert werden (0/1)
   E:int laden          :Werte sollen geladen werden falls vorhanden (0/1)
   E:int restart        :Werte sollen bei Restart geladen werden (0/1)
   E:int adapt_interpol :Werte sollen beim verfeinern auf Kinder interpoliert (0/1)
   E:double vorgabe     :Vorgabe falls keine Restartdaten oder Anfangsbedingungen vorhanden sind

   Ergebnis:
   Knotenindex der gerade vergeben wurde

   Programmaenderungen:
   09/2000   CT    Erste Version
   
**************************************************************************/
int ModelsAddNodeValInfoStructure(char *name, char *einheit, int speichern, int laden, int restart, int adapt_interpol, double vorgabe)
{
  anz_nval++;
  nval_data = (NvalInfo *) Realloc(nval_data, anz_nval * sizeof(NvalInfo));

  nval_data[anz_nval - 1].name = NULL;
  nval_data[anz_nval - 1].einheit = NULL;

  if (name)
    {
      nval_data[anz_nval - 1].name = (char *) Malloc(((int)strlen(name) + 1) * sizeof(char));
      strcpy(nval_data[anz_nval - 1].name, name);
    }
  if (einheit)
    {
      nval_data[anz_nval - 1].einheit = (char *) Malloc(((int)strlen(einheit) + 1) * sizeof(char));
      strcpy(nval_data[anz_nval - 1].einheit, einheit);
    }

  nval_data[anz_nval - 1].speichern = speichern;
  nval_data[anz_nval - 1].laden = laden;
  nval_data[anz_nval - 1].restart = restart;
  nval_data[anz_nval - 1].adapt_interpol = adapt_interpol;
  nval_data[anz_nval - 1].vorgabe = vorgabe;

  return anz_nval - 1;
}
/**************************************************************************
   ROCKFLOW - Funktion: ModelsAddElementValInfoStructure

   Aufgabe:
   Fuellt die Elementdaten-Infostruktur mit den zugehoerigen Modelldaten.
   Wird vom Modell der Reihe nach fuer jede Elementgroesse aufgerufen.

   Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E:char *name         :Name der Elementgroesse fuer Ergebnisdatei
   E:char *einheit      :Name der phys. Einheit fuer Ergebnisdatei
   E:int speichern      :Werte sollen gespeichert werden (0/1)
   E:int laden          :Werte sollen geladen werden falls vorhanden (0/1)
   E:int restart        :Werte sollen bei Restart geladen werden (0/1)
   E:int adapt_interpol :Werte sollen beim verfeinern auf Kinder interpoliert (0/1)
   E:double vorgabe     :Vorgabe falls keine Restartdaten oder Anfangsbedingungen vorhanden sind

   Ergebnis:
   Elementindex der gerade vergeben wurde

   Programmaenderungen:
   09/2000   CT    Erste Version
   
**************************************************************************/
int ModelsAddElementValInfoStructure(char *name, char *einheit, int speichern, int laden, int restart, int adapt_interpol, double vorgabe)
{
  anz_eval++;
  eval_data = (EvalInfo *) Realloc(eval_data, anz_eval * sizeof(EvalInfo));

  eval_data[anz_eval - 1].name = NULL;
  eval_data[anz_eval - 1].einheit = NULL;

  if (name)
    {
      eval_data[anz_eval - 1].name = (char *) Malloc(((int)strlen(name) + 1) * sizeof(char));
      strcpy(eval_data[anz_eval - 1].name, name);
    }
  if (einheit)
    {
      eval_data[anz_eval - 1].einheit = (char *) Malloc(((int)strlen(einheit) + 1) * sizeof(char));
      strcpy(eval_data[anz_eval - 1].einheit, einheit);
    }

  eval_data[anz_eval - 1].speichern = speichern;
  eval_data[anz_eval - 1].laden = laden;
  eval_data[anz_eval - 1].restart = restart;
  eval_data[anz_eval - 1].adapt_interpol = adapt_interpol;
  eval_data[anz_eval - 1].vorgabe = vorgabe;

  return anz_eval - 1;
}


/**************************************************************************
FEMLib-Method:
Task:
Programing:
01/2005 OK Implementation
last modified:
**************************************************************************/
void PCSDelete()
{
  for(int i=0;i<(int)pcs_vector.size();i++){
    delete pcs_vector[i];
  }
  pcs_vector.clear();
  pcs_no_components = 0;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
03/2005 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::SetNodeValue(long n,int nidx,double value)
{
  nod_val_vector[n][nidx] = value;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
09/2005 PCH Implementation
last modified:
**************************************************************************/
void CRFProcess::SetElementValue(long n,int nidx,double value)
{
  ele_val_vector[n][nidx] = value;
}
/**************************************************************************
FEMLib-Method:
Task:
Programing:
03/2005 OK Implementation
last modified:
**************************************************************************/
double CRFProcess::GetNodeValue(long n,int nidx)
{
  double value;
  value = nod_val_vector[n][nidx];
  return value;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
09/2005 PCH Implementation
last modified:
**************************************************************************/
double CRFProcess::GetElementValue(long n,int nidx)
{
  double value;
  value = ele_val_vector[n][nidx];
  return value;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
03/2005 OK Implementation
01/2006 OK Test
**************************************************************************/
int CRFProcess::GetNodeValueIndex(string var_name)
{
  int i;
  int nidx = -2;
  string help;
  for(i=0;i<(int)nod_val_name_vector.size();i++){
//    if(nod_val_name_vector[i].compare(var_name)==0){
    help = nod_val_name_vector[i];
    if(help.compare(var_name)==0){
      nidx = i;
      return nidx;
    }
  }

  // Suppress the following error message when Fluid Momentum process is on.
  CRFProcess* m_pcs = PCSGet("FLUID_MOMENTUM");
  if(m_pcs) ;	// Don't print any error message.
  else
	cout << "Error in CRFProcess::GetNodeValueIndex - " << pcs_type_name << ", " << var_name << ", NIDX = " << nidx << endl;
  //abort();
  return nidx;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
09/2005 PCH Implementation
last modified:
**************************************************************************/
int CRFProcess::GetElementValueIndex(string var_name)
{
  int i;
  int nidx = -1;
  for(i=0;i<(int)ele_val_name_vector.size();i++){
    if(ele_val_name_vector[i].compare(var_name)==0){
      nidx = i;
      return nidx;
    }
  }
  return nidx;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
03/2005 OK Implementation
05/2005 OK pcs_pv_name, 
12/2005 OK RESTART
07/2006 OK/MX MSH
**************************************************************************/
void CRFProcess::SetIC()
{
  int nidx,nidx1;
  CInitialCondition* m_ic = NULL;
  for(int i=0;i<pcs_number_of_primary_nvals;i++){
    nidx = GetNodeValueIndex(pcs_primary_function_name[i]);
    nidx1 = nidx+1; //WW
    for(int j=0;j<(int)ic_vector.size();j++)
    {
      m_ic = ic_vector[j];
      m_ic->m_msh = m_msh; //OK/MX
      if(!(m_ic->pcs_type_name.compare(pcs_type_name)==0))
        continue;
      m_ic->m_pcs = this;
      if(m_ic->pcs_pv_name.compare(pcs_primary_function_name[i])==0){
        m_ic->Set(nidx);
        m_ic->Set(nidx+1);
      }
    }
  }
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
03/2005 OK Implementation
last modified:
**************************************************************************/
void CRFProcess::SetNODValues()
{
  for(long i=0;i<(long)m_msh->nod_vector.size();i++){
//    SetNODValue(i,GetNODValueIndex(pcs_type_name),eqs->x[i]);
//    SetNODValue(i,GetNODValueIndex(pcs_type_name)+1,eqs->x[i]);
    SetNodeValue(m_msh->Eqs2Global_NodeIndex[i],0,eqs->x[i]); //WW
    SetNodeValue(m_msh->Eqs2Global_NodeIndex[i],1,eqs->x[i]); //WW
  }
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
05/2005 OK Implementation
last modification:
**************************************************************************/
void CRFProcess::CheckBCGroup()
{
  long j;
  CBoundaryConditionNode* m_node = NULL;
  //----------------------------------------------------------------------
  // File handling
  //OK CGSProject* m_gsp = NULL;
  //OK m_gsp = GSPGetMember("pcs");
  //OK string pcs_bc_group_file_name = m_gsp->path + "_CheckBCGroup" + ".txt";
  string pcs_bc_group_file_name = pcs_type_name + "_BCGroup.txt";
  fstream pcs_bc_group_file (pcs_bc_group_file_name.data(),ios::trunc|ios::out);
  pcs_bc_group_file.setf(ios::scientific,ios::floatfield);
  if (!pcs_bc_group_file.good()) 
    return;
  pcs_bc_group_file.seekg(0L,ios::beg);
  //----------------------------------------------------------------------
  CBoundaryConditionsGroup *m_bc_group = NULL;
  list<CBoundaryConditionsGroup*>::const_iterator p_bc_group = bc_group_list.begin();
  while(p_bc_group!=bc_group_list.end()) {
    m_bc_group = *p_bc_group;
    for(int i=0;i<pcs_number_of_primary_nvals;i++){
      if((m_bc_group->pcs_type_name.compare(pcs_type_name)==0)\
       &&(m_bc_group->pcs_pv_name.compare(pcs_primary_function_name[i])==0)){
        //m_bc_group->Check();
        pcs_bc_group_file << m_bc_group->pcs_type_name << endl;
        pcs_bc_group_file << m_bc_group->pcs_pv_name << endl;
        pcs_bc_group_file << "geo_node_number" << "," << "msh_node_number" << "," << "node_value" << endl;
        for(j=0;j<(long)m_bc_group->group_vector.size();j++){
          m_node = m_bc_group->group_vector[j];
          pcs_bc_group_file << " " << m_node->geo_node_number << " " << m_node->msh_node_number << " ";
          pcs_bc_group_file << m_node->node_value << endl;
        }
      }
    }
    ++p_bc_group;
  }
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
05/2005 OK Implementation
last modification:
**************************************************************************/
/*
void CRFProcess::CheckSTGroup()
{
  long j;
  CNodeValue* m_node = NULL;
  //----------------------------------------------------------------------
  // File handling
  //OK CGSProject* m_gsp = NULL;
  //OK m_gsp = GSPGetMember("pcs");
  //OK string pcs_st_group_file_name = m_gsp->path + "_CheckSTGroup" + ".txt";
  string pcs_st_group_file_name = pcs_type_name + "_STGroup.txt";
  fstream pcs_st_group_file (pcs_st_group_file_name.data(),ios::trunc|ios::out);
  pcs_st_group_file.setf(ios::scientific,ios::floatfield);
  if (!pcs_st_group_file.good()) 
    return;
  pcs_st_group_file.seekg(0L,ios::beg);
  //----------------------------------------------------------------------
  CSourceTermGroup *m_st_group = NULL;
  list<CSourceTermGroup*>::const_iterator p_st_group = st_group_list.begin();
  while(p_st_group!=st_group_list.end()) {
    m_st_group = *p_st_group;
    for(int i=0;i<pcs_number_of_primary_nvals;i++){
      if((m_st_group->pcs_type_name.compare(pcs_type_name)==0)\
       &&(m_st_group->pcs_pv_name.compare(pcs_primary_function_name[i])==0)){
        //m_bc_group->Check();
        pcs_st_group_file << m_st_group->pcs_type_name << endl;
        pcs_st_group_file << m_st_group->pcs_pv_name << endl;
        pcs_st_group_file << "geo_node_number" << "," << "msh_node_number" << "," << "node_value" << endl;
        for(j=0;j<(long)m_st_group->group_vector.size();j++){
          m_node = m_st_group->group_vector[j];
          pcs_st_group_file << " " << m_node->geo_node_number << " " << m_node->msh_node_number << " ";
          pcs_st_group_file << m_node->node_value << endl;
        }
      }
    }
    ++p_st_group;
  }
}
*/

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
05/2005 OK Implementation
07/2005 WW Change for geometry element object
**************************************************************************/
/*
double CRFProcess::GetELEValue(long index,double*gp,double theta,string nod_fct_name)
{
  int nidx0,nidx1;
  double unit[3];
  double e_value;
  double nod_fct0,nod_fct1;
  unit[0] = unit[1] = unit[2] = 0.0;
  //----------------------------------------------------------------------
  nidx0 = GetNODValueIndex(nod_fct_name); // PCSGetNODValueIndex(nod_fct_name,0);
  nidx1 = GetNODValueIndex(nod_fct_name)+1; // PCSGetNODValueIndex(nod_fct_name,1);
  //----------------------------------------------------------------------
  nod_fct0 = 0.0;
  nod_fct1 = 0.0;
  if(gp==NULL){ // Element average value . WW
    if(m_msh->ele_vector[index]->GetElementType()==4)
      unit[0] = unit[1] = 1.0/3.0;
    else if(m_msh->ele_vector[index]->GetElementType()==5)
      unit[0] = unit[1] = unit[2] = 0.25;
  }
  else
  {
     for(int i=0; i<3; i++) unit[i] = gp[i];
  }

  nod_fct0 = InterpolateNODValue(index,nidx0,unit);
  nod_fct1 = InterpolateNODValue(index,nidx1,unit);
  //----------------------------------------------------------------------
  e_value = (1.-theta)*nod_fct0 + theta*nod_fct1;
  return e_value;
}
*/
/**************************************************************************
FEMLib-Method:
Task:
Programing:
05/2005 OK Implementation
07/2005 WW Re-Implementation
last modification:
**************************************************************************/
/*
double CRFProcess::InterpolateNODValue(long number,int ndx,double*gp)
{
  int i;
  double nval[20];
  CElem* elem = NULL;
  elem = m_msh->ele_vector[number];
  fem->ConfigElement(elem);
  fem->setUnitCoordinates(gp);
  fem->ComputeShapefct(1); // Linear
  for(i=0;i<elem->GetVertexNumber();i++)
     nval[i] = GetNodeValueIndex(elem->nodes_index[i],ndx); // GetNodeVal(nodes[i], ndx);
  return fem->interpolate(nval, 1);
}
*/

/**************************************************************************
FEMLib-Method:
Task: Ermittelt den Fehler bei Iterationen
 new_iteration     : Vektor des neuen Iterationsschritts
 old_iteration_ndx : Knotenindex fuer Werte des alten Iterationsschritts
 reference_ndx     : Knotenindex fuer Werte des alten Zeitschritts (als Referenz)
 method            : Methode der Fehlerermittlung
Programing:
01/2005 OK NUM implementation
05/2005 OK MSH
08/2005 WW Re-implememtation based on NUMCalcIterationError
last modification:
**************************************************************************/
double CRFProcess::CalcIterationNODError(int method)
{

    static long i, k;
    static double error, change, max_c, min_c;
    int ndx0 = GetNodeValueIndex(pcs_primary_function_name[continuum]);
    int ndx1 = GetNodeValueIndex(pcs_primary_function_name[continuum])+1;

    error = 0.;
    change = 0.;

    max_c = 0.;
    min_c = 1.e99;

    switch (method) {
       default:
       case 0:
          return 0.;
       // Maximum error
       case 1:
	     for (i = 0l; i < eqs->dim; i++)
		 {
			 k = m_msh->Eqs2Global_NodeIndex[i];
             error = max(error, fabs(GetNodeValue(k, ndx1) - eqs->x[i]));
		 }
         return error;
       case 2:
	     for (i = 0l; i < eqs->dim; i++)
		 {
			 k = m_msh->Eqs2Global_NodeIndex[i];
             error = max(error, fabs(eqs->x[i] - GetNodeValue(k, ndx1))
				 /(fabs(eqs->x[i]) + fabs(GetNodeValue(k, ndx1))+ MKleinsteZahl) );
		 }
		 return error;
       case 3:
         for (i = 0l; i < eqs->dim; i++) {
            k = m_msh->Eqs2Global_NodeIndex[i];
            error = max(error, fabs(eqs->x[i] - GetNodeValue(k, ndx1)));
            max_c = max(max(max_c, fabs(fabs(eqs->x[i]))),fabs(GetNodeValue(k, ndx1)));
         }
         return error / (max_c + MKleinsteZahl);
       case 4:
         for (i = 0l; i < eqs->dim; i++) {
            k = m_msh->Eqs2Global_NodeIndex[i];
            error = max(error, fabs(eqs->x[i] - GetNodeValue(k, ndx1)));
            min_c = min(min_c, fabs(eqs->x[i]));
            max_c = max(max_c, fabs(eqs->x[i]));
         }
         return error / (max_c - min_c + MKleinsteZahl);
       case 5:
         for (i = 0l; i < eqs->dim; i++) {
	        k = m_msh->Eqs2Global_NodeIndex[i];
            error = max(error, fabs(eqs->x[i] -  GetNodeValue(k, ndx1)) 
  				/ (fabs(eqs->x[i] - GetNodeValue(k, ndx0)) + MKleinsteZahl));
		 }
         return error;
       case 6:
         for (i = 0l; i < eqs->dim; i++) {
            k = m_msh->Eqs2Global_NodeIndex[i];
            error = max(error, fabs(eqs->x[i] -  GetNodeValue(k, ndx1)));
            change = max(change, fabs(eqs->x[i] - GetNodeValue(k, ndx0)));
         }
         return error / (change + MKleinsteZahl);
	}
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
05/2005 OK Implementation
04/2006 YD Add contiuum coupling OK???Why ere
**************************************************************************/
double CRFProcess::ExecuteNonLinear()
{
  int i, j, iterations_continuum_couple = 1;
  double nonlinear_iteration_error=0.0;
  if((int)continuum_vector.size()>1) iterations_continuum_couple = 3;       // m_num->cpl_iterations;
  //----------------------------------------------------------------------
  // Multi-Continua
  for(j=0;j<iterations_continuum_couple;j++) //OK???YD two processes with exchange
  {
    for(i=0;i<(int)continuum_vector.size();i++) //OK???YD
    {
      cout << " PCS continuum type: " << i << endl;
      continuum = i;
      //..................................................................
      for(iter=0;iter<pcs_nonlinear_iterations;iter++)
      {
        cout << "    PCS non-linear iteration: " << iter << "/"   
             << pcs_nonlinear_iterations << endl;
        nonlinear_iteration_error = Execute();
        if(mobile_nodes_flag ==1)
          PCSMoveNOD();
        if(nonlinear_iteration_error < pcs_nonlinear_iteration_tolerance)
        {
          Tim->repeat = false; //OK/YD
          Tim->nonlinear_iteration_error = nonlinear_iteration_error; //OK/YD
          break;
        }
       else
          Tim->repeat = true; //OK/YD
      }
      //..................................................................
/*
//OK???YD why here, should be in TIM
      if(Tim->time_control_name.find("ADAPTIVE")!=string::npos)
      { 
        if(nonlinear_iteration_error > pcs_nonlinear_iteration_tolerance)
        {
          cout << "   PCS repeat " << endl;  
if(aktueller_zeitschritt==2&&pcs_number==167)
{
value = GetNodeValue(0,1);
}
          PrimaryVariableReload();
          Recalculate = true; 
        }
      }
      //..................................................................
*/
    }
  //----------------------------------------------------------------------
  }
  return nonlinear_iteration_error;
}


/**************************************************************************
FEMLib-Method: 
Task:
Programing:
04/2005 OK Implementation
**************************************************************************/
void PCSDelete(string m_pcs_type_name)
{
  CRFProcess* m_pcs = NULL;
  for(int i=0;i<(int)pcs_vector.size();i++){
    m_pcs = pcs_vector[i];
    if(m_pcs->pcs_type_name.compare(m_pcs_type_name)==0){
      delete m_pcs;
      pcs_vector.erase(pcs_vector.begin()+i);
    }
  }
}

/**************************************************************************
   GeoSys - Function: Reallocation

   Aufgabe:
        Reallocte memory by new operator
   09/2005   WW    Erste Version
   
**************************************************************************/
template <class T> T *resize(T *array, size_t old_size, size_t new_size)
{
   T *temp = new T[new_size];
   for(size_t i=0; i<old_size; i++) temp[i] = array[i];
   for(size_t i=old_size; i<new_size; i++) temp[i] = 0;
   delete [] array;
   array = temp; 
   return temp;
} 
/**************************************************************************
   GeoSys - Function: Reload temperature and pressure. Will be changed
    01/2005   WW    
**************************************************************************/
void ReloadTandP(const int type, CRFProcess *pcs) // For DECOVALEX test
{
    long i;
    string deli = " ";
    string StressFileName = FileName+".pat";
    fstream file_stress (StressFileName.data(), ios::in);   
    int idxT0, idxT1, idxP0, idxP1;
	idxT0=idxT1=idxP0=idxP1=-1;
	if(type==0)
	{
       idxT0 = pcs->GetNodeValueIndex("TEMPERATURE1");
       idxT1 = idxT0+1;
	}
	else
	{
       idxP0 = pcs->GetNodeValueIndex("PRESSURE1");
       idxP1 = idxP0+1;
	}

      double val1, val2;
	  for(i=0; i<pcs->m_msh->NodesInUsage(); i++)
      {
        file_stress>>val1>>val2>>ws;
		if(type==0)
		{
           pcs->SetNodeValue(i,idxT0, val1);
           pcs->SetNodeValue(i,idxT1, val2);
		}

        file_stress>>val1>>val2>>ws;
		if(type==1)
		{
           pcs->SetNodeValue(i,idxP0,  val1);  // negative value for Richards flow
           pcs->SetNodeValue(i,idxP1,  val2);
		}
      }
  
   file_stress.close();
   
}
/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
11/2005 MB Implementation
**************************************************************************/
void CRFProcess::CalcFluxesForCoupling(void)
{
  int i,j;
  double flux;
  long n_index;
  long NodeIndex_GW;
  long NodeIndex_OLF;
  int no_nodes = m_msh->no_msh_layer +1;
  long no_richards_problems = (long)(m_msh->ele_vector.size()/m_msh->no_msh_layer);
  long IndexBottomNode;
  long IndexTopNode;
  int NoOfGWNodes = 0;
  double AverageZ_GW = 0.0;
  double AverageZ_OLF =  0.0;
  double AverageH_GW = 0.0;
  double AverageH_OLF =  0.0;
  double dh;
  int idxFLUX;
  int idxHead_GW;
  int idxHead_OLF;
  Mesh_Group::CElem* m_ele_GW = NULL;
  Mesh_Group::CElem* m_ele_OLF = NULL;
  CRFProcess*m_pcs_GW = NULL;
  CRFProcess*m_pcs_OLF = NULL;
  //Get processes
  m_pcs_GW = PCSGet("GROUNDWATER_FLOW");
  if(!m_pcs_GW) //OK
  {
    cout << "Fatal error: no GROUNDWATER_FLOW process" << endl;
    return;
  }
  m_pcs_OLF  = PCSGet("OVERLAND_FLOW");
  if(!m_pcs_OLF) //OK
  {
    cout << "Fatal error: no OVERLAND_FLOW process" << endl;
    return;
  }
  //Get meshes
  CFEMesh* m_msh_GW = m_pcs_GW->m_msh;
  CFEMesh* m_msh_OLF = m_pcs_OLF->m_msh;
  //Get indeces
  idxHead_GW  = m_pcs_GW->GetNodeValueIndex("HEAD")+1;
  idxHead_OLF  = m_pcs_OLF->GetNodeValueIndex("HEAD")+1;
  idxFLUX  = GetNodeValueIndex("FLUX")+1;

  for(i=0;i<no_richards_problems;i++){
    IndexBottomNode = ((i+1) * no_nodes)-1;

    // ToDo safe somewhere else so that this has to be done only once
    //-----------------------------------------------------------------
    // Get Nearest GW and OLF Element
    CGLPoint* m_pnt = NULL;
    long EleNumber;
    m_pnt = new CGLPoint;
	
    m_pnt->x = m_msh->nod_vector[IndexBottomNode]->X();
    m_pnt->y = m_msh->nod_vector[IndexBottomNode]->Y();
    m_pnt->z = m_msh->nod_vector[IndexBottomNode]->Z();

    EleNumber = m_msh_GW->GetNearestELEOnPNT(m_pnt);
    delete m_pnt;
    //GW and OLF use the same Numbering !!!
    m_ele_GW = m_msh_GW->ele_vector[EleNumber];
    m_ele_OLF = m_msh_OLF->ele_vector[EleNumber];

    //-----------------------------------------------------------------
    // Get Average values for element //ToDo encapsulate //WW: CElement::elemnt_averag??e         
    NoOfGWNodes = m_ele_OLF->GetNodesNumber(m_msh_GW->getOrder());
    for(j=0; j<NoOfGWNodes; j++){
      NodeIndex_GW = m_ele_GW->GetNodeIndex(j);
      NodeIndex_OLF = m_ele_OLF->GetNodeIndex(j);

      AverageZ_GW += m_pcs_GW->GetNodeValue(NodeIndex_GW,idxHead_GW);
      AverageZ_OLF += m_msh_OLF->nod_vector[NodeIndex_OLF]->Z();
    }
    AverageZ_GW = AverageZ_GW / NoOfGWNodes;
    AverageZ_OLF = AverageZ_OLF / NoOfGWNodes;

    //-----------------------------------------------------------------
    // UsatZone exists -> Flux from this        
    if(AverageZ_GW < AverageZ_OLF){
      n_index = m_msh->Eqs2Global_NodeIndex[IndexBottomNode];
      if(m_msh->nod_vector[IndexBottomNode]->GetMark()){
        flux = eqs->b[IndexBottomNode];
        //FLUXES IN NEW VERSION WITH VELOCITIES !!!!!
        //WAIT FOR SEBASTIANS MASS TRANSPORT IN USAT ZONE !!!!!
        //TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST 
        flux = 0.00001;
        //flux = 1;
        SetNodeValue(n_index, idxFLUX, flux); 
      }
    }

    //-----------------------------------------------------------------
    // No UsatZone -> Calculate Flux from leakage terms
    if(AverageZ_GW >= AverageZ_OLF){
      //SetRichardsNodesToFullySaturated??
      IndexTopNode = i * no_nodes;              // Top Node of Richard Column -> Flux for OLF
      IndexBottomNode = ((i+1) * no_nodes)-1;   // Bottom Node of Richards Column -> Flux for GW
      //-----------------------------------------------------------------
      // Get Average values for element //ToDo encapsulate       
      for(j=0; j<NoOfGWNodes; j++){
        NodeIndex_GW = m_ele_GW->GetNodeIndex(j);
        NodeIndex_OLF = m_ele_OLF->GetNodeIndex(j);
        AverageH_GW += m_pcs_GW->GetNodeValue(NodeIndex_GW,idxHead_GW);
        AverageH_OLF += m_pcs_OLF->GetNodeValue(NodeIndex_OLF,idxHead_OLF);
      }
      AverageH_GW = AverageH_GW / NoOfGWNodes;
      AverageH_OLF = AverageH_OLF / NoOfGWNodes;
      //Calculate the vertical leakage
      dh = AverageH_GW - AverageH_OLF;
      // get kf fully saturated of uppermost element ?
      // or user defined value: entry resistance / leakage factor ?
      //flux = dh * 0.001;
      flux = dh * 1.;

      //1. Add reacharge value to GW flow -> Add to flux off IndexBottomNode
      //Achtung nur zum Testen Source für GW flow durchgehend !!!!!!
      //SetNodeValue(IndexBottomNode, idxFLUX, -flux);  //H_OLF  > H_GW -> + flux_GW
      SetNodeValue(IndexBottomNode, idxFLUX, 0.00001);
      
      //2. Add reacharge value to OLF -> Add to flux off IndexTopNode
      SetNodeValue(IndexTopNode, idxFLUX, flux);      //H_OLF  > H_GW -> - flux_OLF
      //3. Set flag to set reacharge to Usat to zero ???

    }
  }
}
/**************************************************************************
FEMLib-Method:
Task: Ermittelt den Fehler bei Kopplungs Iterationen
Programing:
11/2005 MB implementation
**************************************************************************/
double CRFProcess::CalcCouplingNODError()
{
  static long i, k;
  static double error, change, max_c, min_c;
  int ndx0 = GetNodeValueIndex(m_num->cpl_variable);
  int ndx1 = GetNodeValueIndex(m_num->cpl_variable)+1;

  error = 0.;
  change = 0.;

  max_c = 0.;
  min_c = 1.e99;

  for (i = 0l; i < eqs->dim; i++) {
    k = m_msh->Eqs2Global_NodeIndex[i];
    if(m_msh->nod_vector[k]->GetMark())
      error = max(error, fabs(GetNodeValue(k, ndx1) - GetNodeValue(k, ndx0)));
	}
  cout <<  "  Coupling error: " << error << endl;
        
  return error;

}

/**************************************************************************
FEMLib-Method:
Task: 
Programing:
11/2005 MB implementation
**************************************************************************/
void CRFProcess::CopyCouplingNODValues()
{
  int nidx0 = -1;
  int nidx1 = -1;
  long l;
  
  //Carefull if cpl_variable = primary variable -> need extra coulumn in NodeValueTable !      
  nidx0 = GetNodeValueIndex(m_num->cpl_variable);
  nidx1 = nidx0+1;
  
  for(l=0;l<(long)m_msh->GetNodesNumber(false);l++){
    SetNodeValue(l,nidx0,GetNodeValue(l,nidx1));
  }
  if(pcs_type_name.find("RICHARDS")!=string::npos) //WW
  {
      nidx0 = GetNodeValueIndex("SATURATION1");
      nidx1 = nidx0+1;
     for(l=0;l<(long)m_msh->GetNodesNumber(false);l++)
        SetNodeValue(l,nidx0,GetNodeValue(l,nidx1));
   } 
}

/**************************************************************************
FEMLib-Method:
Task: 
Programing:
11/2005 MB implementation
02/2006 WW Modified for the cases of high order element and saturation
**************************************************************************/
void CRFProcess::CopyTimestepNODValues()
{
  int nidx0 = -1;
  int nidx1 = -1;
  int j;
  long l;
  
  bool Quadr = false;  //WW
  if(type==4||type==41) Quadr = true;

  for(j=0;j<pcs_number_of_primary_nvals;j++){
    nidx0 = GetNodeValueIndex(pcs_primary_function_name[j]);
    nidx1 = GetNodeValueIndex(pcs_primary_function_name[j])+1;
    for(l=0;l<(long)m_msh->GetNodesNumber(Quadr);l++)
      SetNodeValue(l,nidx0,GetNodeValue(l,nidx1));
    }
   if(pcs_type_name.compare("RICHARDS_FLOW")==0)
	  for(j=0;j<pcs_number_of_secondary_nvals;j++){
		 nidx0 = GetNodeValueIndex(pcs_secondary_function_name[j]);
         nidx1 = GetNodeValueIndex(pcs_secondary_function_name[j])+1;
         for(l=0;l<(long)m_msh->GetNodesNumber(false);l++)
              SetNodeValue(l,nidx0,GetNodeValue(l,nidx1));
	 }
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
12/2005 SB Implementation
last modified:
**************************************************************************/
CRFProcess* PCSGet(string pcs_type_name, string comp_name)
{
  CRFProcess *m_pcs = NULL;
  int i;
  int no_processes = (int)pcs_vector.size();
  string testname;
  for(i=0;i<no_processes;i++){
    m_pcs = pcs_vector[i];
    if(m_pcs->pcs_type_name.compare(pcs_type_name)==0){
      testname = m_pcs->pcs_primary_function_name[0];
      if(testname.compare(comp_name)==0){
//        cout << " Found in PCSGetbyTypeandCompName for PCSType/Compname " << pcs_type_name << ", " << comp_name;
//        cout << " Process number " << m_pcs->pcs_number << ", compnumber " << m_pcs->pcs_component_number << endl;
        return m_pcs;
      }
    }
  }
  return NULL;
}

/**************************************************************************
PCSLib-Method:
12/2005 OK Implementation
**************************************************************************/
CRFProcess* PCSGet(string var_name,bool bdummy)
{
  int j;
  string pcs_var_name;
  CRFProcess *m_pcs = NULL;
  bdummy = bdummy; //WW
  for(int i=0;i<(int)pcs_vector.size();i++){
    m_pcs = pcs_vector[i];
    for(j=0;j<m_pcs->GetPrimaryVNumber();j++){
      pcs_var_name = m_pcs->pcs_primary_function_name[j];
      if(pcs_var_name.compare(var_name)==0){
        return m_pcs;
      }
    }
    for(j=0;j<m_pcs->GetSecondaryVNumber();j++){
      pcs_var_name = m_pcs->pcs_secondary_function_name[j];
      if(pcs_var_name.compare(var_name)==0){
        return m_pcs;
      }
    }
  }
  return NULL;
}
/**************************************************************************
PCSLib-Method:
05/2006 CMCD Implementation
**************************************************************************/
CRFProcess* PCSGetFluxProcess()
{
  CRFProcess *m_pcs;
  int i;
  bool found = false;
  string help;
  int no_processes = (int) pcs_vector.size();
  for (i=0; i<no_processes; i++){
    help = pcs_vector[i]->pcs_type_name;
    if (pcs_vector[i]->pcs_type_name=="LIQUID_FLOW") {
       m_pcs = pcs_vector[i];
       found = true;
       }
    if (pcs_vector[i]->pcs_type_name=="GROUNDWATER_FLOW"){
       m_pcs = pcs_vector[i];
       found = true;
       }
    if (found) return m_pcs;
  }
  return NULL;
}
/*************************************************************************
GeoSys-FEM Function:
Task: 
Programming: 
02/2005 OK/YD Implementation
05/2005 WW  Generalization for all element types
05/2005 OK MSH
08/2005 WW Changes for new mesh
04/2006 YD Changes for Dual Richards
last modified:
**************************************************************************/
//using FiniteElement::CFiniteElementStd;
void CRFProcess::CalcSecondaryVariablesRichards(int timelevel, bool update)
{
  int j, EleType;
  long i, enode;
  long group,multi_mmp;
  double p_cap;
  double saturation,saturation_sum = 0.0;
  double GP[3];
  static double Node_Cap[8];
  int idxp,idxcp,idxS,idx_tS=-1; 
  int number_continuum;
  double total_S;
  int i_pv,i_s,i_e;

  CMediumProperties* m_mmp = NULL;
  CElem* elem =NULL;
  CFiniteElementStd* fem = GetAssembler();
  number_continuum = (int)continuum_vector.size();
//----------------------------------------------------------------------
  if(continuum_ic){  //Create IC: for both continua
     i_s = 0;
     i_e = number_continuum;
  }
  else{
	 i_s = continuum;
	 i_e = continuum+1;
  }
//----------------------------------------------------------------------
  for(i_pv=i_s;i_pv<i_e;i_pv++)
  {
     idxp = GetNodeValueIndex(pcs_primary_function_name[i_pv])+ timelevel;
     idxS = GetNodeValueIndex(pcs_secondary_function_name[i_pv*number_continuum])+ timelevel;
	 idxcp = GetNodeValueIndex(pcs_secondary_function_name[i_pv*number_continuum+number_continuum*2])+ timelevel;
	 if((int)continuum_vector.size()>1)
		 idx_tS = GetNodeValueIndex("TOTAL_SATURATION")+ timelevel;
  //----------------------------------------------------------------------
  // Capillary pressure
     for(i=0;i<(long)m_msh->GetNodesNumber(false);i++){
        p_cap = -GetNodeValue(i,idxp);
        if(timelevel==1&&update)  SetNodeValue(i,idxcp-1,GetNodeValue(i,idxcp));
        SetNodeValue(i,idxcp,p_cap);
	    if(timelevel==1&&update) SetNodeValue(i,idxS-1,GetNodeValue(i,idxS));
     }
  //----------------------------------------------------------------------
 // Liquid saturation
	 for (i = 0; i < m_msh->GetNodesNumber(false); i++){
      SetNodeValue(i,idxS, 0.0);
	 if((int)continuum_vector.size()>1)
      SetNodeValue(i,idx_tS, 0.0);  
     }
  // 
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)  
  {
     elem = m_msh->ele_vector[i];
     if (elem->GetMark())     // Element selected
     {
         if((int)continuum_vector.size() == 1) 
		  group = elem->GetPatchIndex();
		 else{
		  multi_mmp = (long)mmp_vector.size()/(long)continuum_vector.size();
		  group = elem->GetPatchIndex()+multi_mmp*(long)i_pv;   //YD
		 }
          m_mmp = mmp_vector[group];
          m_mmp->m_pcs = this;

          // Activated Element 
          EleType = elem->GetElementType();
          if(EleType==4) // Traingle
          {
             GP[0] = GP[1] = 0.1/0.3; 
             GP[2] = 0.0;
          }
          else if(EleType==5) 
		     GP[0] = GP[1] = GP[2] = 0.25;
          else
		     GP[0] = GP[1] = GP[2] = 0.0;  

          fem->ConfigElement(elem);
		  fem->setUnitCoordinates(GP);
          fem->ComputeShapefct(1); // Linear
		  for(j=0; j<elem->GetVertexNumber(); j++)
		  {
             enode = elem->GetNodeIndex(j);
             Node_Cap[j] =  GetNodeValue(enode,idxcp);
		  }
		  p_cap = fem->interpolate(Node_Cap);
          saturation = m_mmp->SaturationCapillaryPressureFunction(p_cap,(int)mfp_vector.size()-1);  //YD
		  for(j=0; j<elem->GetVertexNumber(); j++)
          {
			  enode = elem->GetNodeIndex(j);
              saturation_sum = GetNodeValue(enode, idxS);
			  SetNodeValue(enode,idxS, saturation_sum+saturation);

          }

      }
  }
  // Average 
  for (i = 0; i <(long)m_msh->GetNodesNumber(false); i++)
  {       	  
	  saturation_sum = GetNodeValue(i, idxS);
      p_cap = m_msh->nod_vector[i]->connected_elements.size();
      if(p_cap==0) p_cap =1;
	  saturation_sum /= (double)p_cap;
      SetNodeValue(i,idxS, saturation_sum);
  }
  }
  //----------

  if(continuum > 0){
	for (i = 0; i <(long)m_msh->GetNodesNumber(false); i++){
      total_S = 0;
	  for(j = 0; j< (int)continuum_vector.size(); j++){
        idxS = GetNodeValueIndex(pcs_secondary_function_name[j*number_continuum])+ timelevel;
	    total_S += GetNodeValue(i, idxS)*continuum_vector[j];
      }
      SetNodeValue(i,idx_tS, total_S);
	  }
  }

  //WW
  /*
  idxp  = GetNodeValueIndex("PRESSURE1") + timelevel;
  idxcp = GetNodeValueIndex("PRESSURE_CAP") + timelevel;
  idxS  = GetNodeValueIndex("SATURATION1") + timelevel;
  //idxf = GetNodeValueIndex("FLUX") + timelevel;
  //WW
  //----------------------------------------------------------------------
  // Capillary pressure
  for(i=0;i<(long)m_msh->GetNodesNumber(false);i++){
     p_cap = -GetNodeValue(i,idxp);
     if(timelevel==1&&update)  SetNodeValue(i,idxcp-1,GetNodeValue(i,idxcp));
     SetNodeValue(i,idxcp,p_cap);
	  if(timelevel==1&&update) SetNodeValue(i,idxS-1,GetNodeValue(i,idxS));
  }
  //----------------------------------------------------------------------
  // Liquid saturation
  //WW
  for (i = 0; i < m_msh->GetNodesNumber(false); i++)
      SetNodeValue(i,idxS, 0.0);
  // 
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)  
  {
     elem = m_msh->ele_vector[i];
     if (elem->GetMark())     // Element selected
     {
          // Activated Element 
          group = elem->GetPatchIndex();
          m_mmp = mmp_vector[group];
          m_mmp->m_pcs = this;
          EleType = elem->GetElementType();
          if(EleType==4) // Traingle
          {
             GP[0] = GP[1] = 0.1/0.3; 
             GP[2] = 0.0;
          }
          else if(EleType==5) 
		     GP[0] = GP[1] = GP[2] = 0.25;
          else
		     GP[0] = GP[1] = GP[2] = 0.0;  

          fem->ConfigElement(elem);
		  fem->setUnitCoordinates(GP);
          fem->ComputeShapefct(1); // Linear

          // fem->Cal_Velocity(true);     //YD
		  for(j=0; j<elem->GetVertexNumber(); j++)
		  {
             enode = elem->GetNodeIndex(j);
             Node_Cap[j] =  GetNodeValue(enode,idxcp);
		  }
		  p_cap = fem->interpolate(Node_Cap);
          saturation = m_mmp->SaturationCapillaryPressureFunction(p_cap,(int)mfp_vector.size()-1);  //YD
		  for(j=0; j<elem->GetVertexNumber(); j++)
          {
			  enode = elem->GetNodeIndex(j);
              saturation_sum = GetNodeValue(enode, idxS);
			  SetNodeValue(enode,idxS, saturation_sum+saturation);
          }
      }
  }
  // Average 
  for (i = 0; i <(long)m_msh->GetNodesNumber(false); i++)
  {       	  
	  saturation_sum = GetNodeValue(i, idxS);
      p_cap = m_msh->nod_vector[i]->connected_elements.size();
      if(p_cap==0) p_cap =1;
	  saturation_sum /= (double)p_cap;
      SetNodeValue(i,idxS, saturation_sum);

 //     cout<<i<<"  "<<saturation_sum<<"  "<<-GetNodeValue(i,idxp)<<endl;   //YD TEST
  }
  */

}

/**************************************************************************
   GeoSys - Function: Get mean element value for element index from secondary node values
				      of process pcs_name and for variable var_name; old and new timelevel
    01/2006   SB    Implementation
**************************************************************************/
double PCSGetEleMeanNodeSecondary(long index, string pcs_name, string var_name, int timelevel){

double val = 1.0; // As this returns saturation, default is fully saturated = 1.0;
int idx, j;
long enode;
CRFProcess *m_pcs = NULL;

// Get process by process name
m_pcs = PCSGet(pcs_name); 
if(m_pcs){
    // Get index of secondary node value
    idx = m_pcs->GetNodeValueIndex(var_name)+timelevel; 
    // Get element with index index
	CElem* elem =NULL; 
	elem = m_pcs->m_msh->ele_vector[index];
	val = 0.0;
	for(j=0; j<elem->GetVertexNumber(); j++){ // average all adjoining nodes
		enode = elem->GetNodeIndex(j);
        val +=  m_pcs->GetNodeValue(enode,idx);
	}
	val = val/((double)elem->GetVertexNumber());
}
return val;
}

/*************************************************************************
GeoSys-FEM Function:
01/2006 OK Implementation
**************************************************************************/
void CRFProcess::SetNODFlux()
{
  long i;
  //----------------------------------------------------------------------
  int nidx;
  nidx = GetNodeValueIndex("FLUX");
  if(nidx<0)
    return;
  double m_val;
  for(i=0;i<(long)m_msh->nod_vector.size();i++)
  {
    m_val = eqs->b[i]; //? m_nod->eqs_index
    SetNodeValue(i,nidx,m_val);
  }
  //----------------------------------------------------------------------
}

/*************************************************************************
GeoSys-FEM Function:
01/2006 OK Implementation
**************************************************************************/
void CRFProcess::AssembleParabolicEquationRHSVector()
{
  long i;
  //----------------------------------------------------------------------
  // Init
  for(i=0;i<(long)m_msh->nod_vector.size();i++)
  {
    eqs->b[i] = 0.0;
  }
  //----------------------------------------------------------------------
  CElem* m_ele = NULL;
  for(i=0;i<(long)m_msh->ele_vector.size();i++)
  {
    m_ele = m_msh->ele_vector[i];
    if(m_ele->GetMark()) // Marked for use
    {
      fem->ConfigElement(m_ele,false);
      fem->AssembleParabolicEquationRHSVector();
      //fem->AssembleParabolicEquationLHSMatrix();
    } 
  }
  //----------------------------------------------------------------------
}
/*************************************************************************
GeoSys-FEM Function:
06/2006 YD Implementation
Reload primary variable
**************************************************************************/
void CRFProcess::PrimaryVariableReload()
{
  char pcsT;
  pcsT = pcs_type_name[0];
  switch(pcsT){
    case 'L':
      break;
    case 'U':
      break;
    case 'G':
      break;
    case 'T':
      break;
    case 'C':
      break;
    case 'R': // Richards flow
      PrimaryVariableReloadRichards();
      break;
  }
}
/*************************************************************************
GeoSys-FEM Function:
06/2006 YD Implementation
Reload primary variable of Richards Flow
**************************************************************************/
void CRFProcess::PrimaryVariableReloadRichards()
{
  int i;
  int idxp,idx_storage;
  double storage_p;

  idxp = GetNodeValueIndex(pcs_primary_function_name[0]);
  idx_storage = GetNodeValueIndex("STORAGE_P");
  for(i=0;i<(long)m_msh->GetNodesNumber(false);i++){
    storage_p = GetNodeValue(i,idx_storage);
    SetNodeValue(i,idxp,storage_p);
    SetNodeValue(i,idxp+1,storage_p);
  }     
  CalcSecondaryVariables(0);
  CalcSecondaryVariables(1);
}

/*************************************************************************
GeoSys-FEM Function:
06/2006 YD Implementation
Reload primary variable of Richards Flow
**************************************************************************/
void CRFProcess::PrimaryVariableStorageRichards()
{
  int i;
  int idxp,idx_storage;
  double pressure;

  idxp = GetNodeValueIndex(pcs_primary_function_name[0])+1;
  idx_storage = GetNodeValueIndex("STORAGE_P");
  for(i=0;i<(long)m_msh->GetNodesNumber(false);i++){
    pressure = GetNodeValue(i,idxp);
    SetNodeValue(i,idx_storage,pressure);
    SetNodeValue(i,idx_storage+1,pressure);
  }     
}

/**************************************************************************
FEMLib-Method: 
11/2005 MB Implementation
03/2006 OK 2nd version (binary coupling)
**************************************************************************/
void CRFProcess::SetCPL()
{
  int i;
  double value;
  //----------------------------------------------------------------------
  // Nothing to do
  if((cpl_type_name.size()==0)||(cpl_type_name.compare("PARTITIONED")==0))
    return;
  //----------------------------------------------------------------------
  // PCS CPL
  CRFProcess* m_pcs_cpl = PCSGet(cpl_type_name);
  if(!m_pcs_cpl)
  {
    cout << "Fatal error in CRFProcess::SetCPL: no PCS data" << endl;
    return;
  }
  //----------------------------------------------------------------------
  // MSH data for PCS CPL
  CFEMesh* m_msh_cpl = m_pcs_cpl->m_msh;
  if(!m_msh_cpl)
  {
    cout << "Fatal error in CRFProcess::SetCPL: no MSH data" << endl;
    return;
  }
  //----------------------------------------------------------------------
  // GEO data for PCS CPL
  Surface* m_sfc = GEOGetSFCByName(m_msh_cpl->geo_name);
  if(!m_sfc)
  {
    cout << "Fatal error in CRFProcess::SetCPL: no GEO data" << endl;
    return;
  }
  //----------------------------------------------------------------------
  //......................................................................
  // MSH nodes of PCS CPL
  cout << "      ->CPL: " << cpl_type_name << ": ";
  vector<long>cpl_msh_nodes_vector;
  m_msh_cpl->GetNODOnSFC(m_sfc,cpl_msh_nodes_vector);
  if((int)cpl_msh_nodes_vector.size()==0)
  {
    cout << "Warning in CRFProcess::SetCPL: no MSH nodes found" << endl;
  }
  cout << "CPL nodes = " << (int)cpl_msh_nodes_vector.size() << endl;
  //.....................................................................-
  // MSH nodes of PCS
  cout << "      ->CPL: " << pcs_type_name << ": ";
  vector<long>msh_nodes_vector;
  m_msh->GetNODOnSFC(m_sfc,msh_nodes_vector);
  if((int)msh_nodes_vector.size()==0)
  {
    cout << "Warning in CRFProcess::SetCPL: no MSH nodes found" << endl;
  }
  cout << "CPL nodes = " << (int)msh_nodes_vector.size() << endl;
  //----------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////
if(m_msh_cpl->pcs_name.compare("RICHARDS_FLOW")==0)
{
  m_msh->SetNODPatchAreas();
  int nidx = GetNodeValueIndex("WDEPTH");
  long st_node_number;
  double st_node_value = 0.0;
  CNode* m_nod = NULL;
  for(i=0;i<(int)msh_nodes_vector.size();i++)
  {
    value = -2.314e-02;
    st_node_number = msh_nodes_vector[i];
    m_nod = m_msh->nod_vector[st_node_number];
    st_node_value = GetNodeValue(st_node_number,nidx);
    st_node_value /= m_nod->patch_area;
    value *= st_node_value;
    //cout << "CPL value = " << value << endl;
    eqs->b[st_node_number] += value;
  }
}
//////////////////////////////////////////////////////////////////////////
if(pcs_type_name.compare("RICHARDS_FLOW")==0&&m_msh_cpl->pcs_name.compare("OVERLAND_FLOW")==0) // ToDo
{
  long msh_node_number;
  long cpl_msh_nod_number;
  long cpl_msh_ele_number;
  value = 0.0;
  cout << "CPL value = " << value << endl;
/*
  CGLPoint* m_pnt = GEOGetPointByName("POINT18");
  msh_node_number = m_msh->GetNODOnPNT(m_pnt);
  eqs->b[msh_node_number] += value;
*/
  // PCS-CON
  CRFProcess* m_pcs_cond = PCSGet(cpl_type_name);
  //int nidx = m_pcs_cond->GetNodeValueIndex(m_pcs_cond->pcs_primary_function_name[0]);
  int cpl_nidx = m_pcs_cond->GetNodeValueIndex("WDEPTH");
  //----------------------------------------------------------------------
  // ELE of PCS_CPL related to NOD of PCS
  CFEMesh* m_msh_this = MSHGet("RICHARDS_FLOW_LOCAL");
  CElem* m_ele_cnd = NULL;
  //----------------------------------------------------------------------
  CSourceTermGroup *m_st_group = NULL;
  CSourceTerm *m_st = NULL;
//  m_st_group = STGetGroup(pcs_type_name,pcs_primary_function_name[0]);
  //----------------------------------------------------------------------
  double cpl_ele_val = 0.0;
  int j;
  CNodeValue *cnodev = NULL;
//  for(i=0;i<(int)m_st_group->group_vector.size();i++)
//  ofstream st_out_file("st_out_file.txt",ios::app);
  for(i=0;i<(int)st_node_value.size();i++)
  {
    cnodev = st_node_value[i]; 
    // MSH-PCS
    //m_nod = m_msh_this->nod_vector[m_st_group->group_vector[i]->msh_node_number];
    msh_node_number = cnodev->msh_node_number; //m_st_group->group_vector[i]->msh_node_number; //0
    // MSH-PCS-CPL
    cpl_msh_nod_number = msh_node_number;
    cpl_msh_ele_number = pcs_number; //OK:TODO
    m_ele_cnd = m_pcs_cond->m_msh->ele_vector[cpl_msh_ele_number];
    for(j=0;j<m_ele_cnd->GetNodesNumber(false);j++)
    {
      cpl_ele_val += m_pcs_cond->GetNodeValue(m_ele_cnd->nodes_index[j],cpl_nidx);
    }
    cpl_ele_val /= m_ele_cnd->GetNodesNumber(false);
    // VAL-CON
    value = 2.314e-02 * cpl_ele_val * 1e-2;
//    st_out_file << value << endl;
    // EQS-RHS
    eqs->b[msh_node_number] += value;
  }
  //----------------------------------------------------------------------
/*
  CNodeValue* m_node_value = NULL;
  m_node_value = new CNodeValue();
  m_node_value->msh_node_number = msh_node_number;
  m_node_value->geo_node_number = m_pnt->id;
  m_node_value->node_value = value;
  CSourceTermGroup *m_st_group = NULL;
  m_st_group = STGetGroup(pcs_type_name,pcs_primary_function_name[0]);
  m_st_group->group_vector.push_back(m_node_value);
  m_st_group->st_group_vector.push_back(m_st); //OK
*/
}
//////////////////////////////////////////////////////////////////////////
if(pcs_type_name.compare("GROUNDWATER_FLOW")==0&&m_msh_cpl->pcs_name.compare("OVERLAND_FLOW")==0) // ToDo
{
  long ie = (long)msh_nodes_vector.size()/(m_msh->no_msh_layer+1);
  long of_node_number,gf_node_number;
  double of_node_value,gf_node_value;
  CNode* m_nod = NULL;
  int of_nidx = GetNodeValueIndex("WDEPTH");

  for(i=0;i<ie;i++)
  {
    of_node_number = msh_nodes_vector[i]; //ToDo
    of_node_value = m_pcs_cpl->GetNodeValue(of_node_number,of_nidx);
    //m_nod = m_msh->nod_vector[gf_node_number];
    //st_node_value /= m_nod->patch_area;
    gf_node_value = of_node_value*2e-11;
if(gf_node_value>1e-13)
    cout << "CPL value = " << gf_node_value << endl;
    gf_node_number = msh_nodes_vector[i];
    eqs->b[gf_node_number] += gf_node_value;
  }
}
//////////////////////////////////////////////////////////////////////////
  int idx = GetNodeValueIndex("FLUX")+1;
  //for(i=0;i<(int)msh_nodes_vector.size();i++)
  for(i=0;i<1;i++)
  {
    SetNodeValue(msh_nodes_vector[i],idx,value);
  }
}

/**************************************************************************
PCSLib-Method: 
04/2006 OK Implementation
**************************************************************************/
void CRFProcess::CreateBCGroup()
{
  int DOF = GetPrimaryVNumber(); //OK should be PCS member variable
  cout << "->Create BC" << '\n';
  CBoundaryConditionsGroup *m_bc_group = NULL;
  for(int i=0;i<DOF;i++){
    BCGroupDelete(pcs_type_name,pcs_primary_function_name[i]);
    m_bc_group = new CBoundaryConditionsGroup();
    m_bc_group->pcs_type_name = pcs_type_name; //OK
    m_bc_group->pcs_pv_name = pcs_primary_function_name[i]; //OK
    m_bc_group->Set(this,Shift[i]);
    bc_group_list.push_back(m_bc_group);
  } 
}

/**************************************************************************
PCSLib-Method: 
04/2006 OK Implementation
**************************************************************************/
void CRFProcess::CreateSTGroup()
{
  int DOF = GetPrimaryVNumber(); //OK should be PCS member variable

  cout << "->Create ST" << '\n';
  CSourceTermGroup *m_st_group = NULL;
  // WW
  ifstream *iSourceNBC_RHS_file = NULL;
  ofstream *oSourceNBC_RHS_file = NULL;
  if(WriteSourceNBC_RHS==1)
  {
     string m_file_name = FileName +"_"+pcs_type_name+"_source_Neumann_RHS.bin";
     oSourceNBC_RHS_file = new ofstream(m_file_name.c_str(), ios::binary);     
  }
  else if(WriteSourceNBC_RHS==2)
  {
     string m_file_name = FileName +"_"+pcs_type_name+"_source_Neumann_RHS.bin";
     iSourceNBC_RHS_file = new ifstream(m_file_name.c_str(), ios::binary);     
     if (!iSourceNBC_RHS_file->good())
       cout << "_source_Neumann_RHS file is not found" << endl;
  }

  for(int i=0;i<DOF;i++)
  {
    //OK m_st_group = m_st_group->Get(pcs_primary_function_name[i]);
    m_st_group = STGetGroup(pcs_type_name,pcs_primary_function_name[i]);
    if(!m_st_group) {
      m_st_group = new CSourceTermGroup();
      m_st_group->pcs_type_name = pcs_type_name; //OK
      m_st_group->pcs_pv_name = pcs_primary_function_name[i]; //OK
//      if(iSourceNBC_RHS_file)  // Read from data. WW
//        m_st_group->Read(*iSourceNBC_RHS_file);
//	  else
        m_st_group->Set(this,Shift[i]);
      st_group_list.push_back(m_st_group);
    }
  }
  if(oSourceNBC_RHS_file)// WW
//    WriteRHS_of_ST_NeumannBC(*oSourceNBC_RHS_file);

  if(iSourceNBC_RHS_file) //WW
  {
    iSourceNBC_RHS_file->close();
     delete iSourceNBC_RHS_file;
     iSourceNBC_RHS_file = NULL; 
  }
  if(oSourceNBC_RHS_file) //WW
  {
     oSourceNBC_RHS_file->close();
     delete oSourceNBC_RHS_file;
     oSourceNBC_RHS_file = NULL; 
  }
}
