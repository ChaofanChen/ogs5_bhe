#include "stdafx.h" //MFC
#include "makros.h"
#ifdef MFC // -> makros.h
#include "afxpriv.h" // For WM_SETMESSAGESTRING
#endif

#include <math.h>
#include <iostream>
#include <iomanip>
#include<fstream>
#include <time.h>

#include "mathlib.h"
//#include "femlib.h"
// Element
#include "fem_ele_std.h"
#include "fem_ele_vec.h"
// BC_Dynamic
#include "tools.h"
#include "rf_bc_new.h"
#include "rf_pcs.h" //OK_MOD"
// 
#include "matrix.h"
#include "pcs_dm.h"
#include "rf_msp_new.h"
#include "fem_ele_vec.h"
#include "rf_tim_new.h"
// Excavation
#include "rf_st_new.h"
#include "rf_out_new.h"
// GEOLib
#include "geo_sfc.h"
// MSHLib
#include "msh_elem.h"

double LoadFactor = 1.0;
double Tolerance_global_Newton = 0.0;
double Tolerance_Local_Newton = 0.0;
int enhanced_strain_dm=0;
int number_of_load_steps = 1;
int problem_2d_type_dm = 1;
int problem_dimension_dm = 0;
int PreLoad=0;
bool GravityForce=true;


bool Localizing = false; // for tracing localization
// Last discontinuity element correponding to SeedElement
vector<DisElement*> LastElement(0); 
vector<long> ElementOnPath(0);  // Element on the discontinuity path


using namespace std;
using FiniteElement::CFiniteElementVec;
using FiniteElement::CFiniteElementStd;
using FiniteElement::ElementValue_DM;
using SolidProp::CSolidProperties;
using Math_Group::Matrix;

//#define EXCAVATION 

namespace process{

CRFProcessDeformation::
     CRFProcessDeformation()
             :CRFProcess(), fem_dm(NULL), ARRAY(NULL), 
              counter(0), InitialNorm(0.0)

 {

 }


CRFProcessDeformation::~CRFProcessDeformation() 
{
   if(ARRAY) ARRAY = (double *) Free(ARRAY);
   if(fem_dm) delete fem_dm;

   fem_dm = NULL;
   ARRAY = NULL;
   // Release memory for element variables   
   // alle stationaeren Matrizen etc. berechnen 
   long i;
   Mesh_Group::CElem* elem = NULL;
   for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
   {
      elem = m_msh->ele_vector[i];
      if (elem->GetMark()) // Marked for use
      {
         delete ele_value_dm[i];
         ele_value_dm[i] = NULL;
      }
   }
   if(enhanced_strain_dm>0)
   {  
       while(ele_value_dm.size()>0) ele_value_dm.pop_back();

       for(i=0; i<(long)LastElement.size(); i++)
       {
           DisElement *disEle = LastElement[i];
           delete disEle->InterFace;
           delete disEle;
           disEle = NULL;
       }
       while(LastElement.size()>0)
           LastElement.pop_back();

   }
}


/*************************************************************************
ROCKFLOW - Function: CRFProcess::UpdateInitialStress() 
Task:  Compute number of element neighbors to a node
Dim : Default=2
Programming: 
 12/2003 WW 
**************************************************************************/
void CRFProcessDeformation::UpdateInitialStress() 
{
  long i;
  ElementValue_DM *eval_DM;

  // Over all elements
  CElem* elem = NULL;
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
  {
     elem = m_msh->ele_vector[i];
     if (elem->GetMark()) // Marked for use
     {           
         eval_DM = ele_value_dm[i];
         (*eval_DM->Stress0) = (*eval_DM->Stress);
         (*eval_DM->Stress) = 0.0;
     }
  }
}


/*************************************************************************
Task: Initilization for deformation process
Programming: 
 05/2003 OK/WW Implementation
 08/2003 WW   Some changes for monolithic scheme  
 08/2004 WW   Changes based on PCSCreateMProcess(obsolete)    
last modified: WW
**************************************************************************/
void CRFProcessDeformation::Initialization()
{
   long i;
   int j;
   // Local assembliers
   // An instaniate of CFiniteElementVec 
   fem_dm = new CFiniteElementVec(this, m_msh->GetCoordinateFlag());

   // Monolithic scheme
   if(type==41) fem =  new CFiniteElementStd(this, m_msh->GetCoordinateFlag());
   //
   dm_pcs_number = pcs_number;
  

  if (m_num){
    Tolerance_Local_Newton = m_num->nls_error_tolerance_local;
    Tolerance_global_Newton = m_num->nls_error_tolerance;
  }


   // Initialize linear solver
   InitEQS();


   InitialMBuffer();
   InitGauss();

   if(H_Process||MH_Process)
   {
      // Initial pressure
      j =  fem_dm->idx_P0;
      int idxP = -1;
      if(fem_dm->Flow_Type<=1)
      {
          idxP = fem_dm->idx_P1;
          if(fem_dm->dynamic)
            idxP = fem_dm->idx_P;
      }
      else
          idxP = fem_dm->idx_P2;
      for (i = 0; i < m_msh->GetNodesNumber(false); i++)
	  {
//TEST for Richard
//		  if(GetNodeValue(i,idxP)>0.0) // Test for Richard flow
             SetNodeValue(i,j, GetNodeValue(i,idxP));
//		  else
//             SetNodeValue(i,j, 0.0);
	  }
   } 

   if(fem_dm->dynamic)
      CalcBC_or_SecondaryVariable_Dynamics();

   if(num_type_name.find("EXCAVATION")!=string::npos)
     ExcavationSimulating();
  //TEST
  //   De_ActivateElement(false);

}

/*************************************************************************
ROCKFLOW - Function: CRFProcess::InitialMBuffer
Task:  Initialize the temporarily used variables
Programming: 
 12/2003 WW 
**************************************************************************/
void CRFProcessDeformation::InitialMBuffer() 
{ 
    long i;
    int bufferSize;
    bufferSize = 0;
    if(GetObjType()==4)
       bufferSize = GetPrimaryVNumber()*m_msh->GetNodesNumber(true);
    else if(GetObjType()==41) 
       bufferSize = (GetPrimaryVNumber()-1)*m_msh->GetNodesNumber(true)+
                     m_msh->GetNodesNumber(false);
    
    AllocateTempArrary(bufferSize);
    
    // Allocate memory for element variables
    Mesh_Group::CElem* elem = NULL;
    for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
    {
       elem = m_msh->ele_vector[i];
       if (elem->GetMark()) // Marked for use
        {
              ElementValue_DM *ele_val = new  ElementValue_DM(elem);
 		      ele_value_dm.push_back(ele_val);
         }
    }
}
/*************************************************************************
ROCKFLOW - Function: CRFProcess::
Task:  Solve plastic deformation by generalized Newton-Raphson method
Programming: 
 02/2003 OK Implementation
 05/2003 WW Polymorphism function by OK
last modified: 23.05.2003
**************************************************************************/
double CRFProcessDeformation::Execute(const int CouplingIterations)
{
#ifdef MFC
  CWnd *pWin = ((CWinApp *) AfxGetApp())->m_pMainWnd;
#endif
  DisplayMsg("\n->Process: "); DisplayLong(pcs_number); 
  DisplayMsg(", "); cout << pcs_type_name << endl; 

  clock_t start, finish;

  //-------------------------------------------------------
  // Controls for Newton-Raphson steps 
  int l;
  const int MaxLoadsteps=10; //20;
  //const int LoadAmp=5;
  double LoadFactor0 = 0.0;
  double minLoadRatio = 0.0000001;
  const double LoadAmplifier =2.0;

  const double TolNorm = 0.01;  	
  double damping = 1.0;
  
  double NormU, Norm= 0.0, Error1, Error=0.0;
  double MaxLoadRatio=1.0, maxStress=0.0;
  
  int ite_steps = 0;
 
  const int defaultSteps=100;
  int MaxIteration=30;

  int elasticity=0; 
  int monolithic=0;

  string delim = " | ";

  //----------------------------------------------------------
  start = clock();
  //
  if(num_type_name.find("EXCAVATION")!=0)
     CheckMarkedElement();
  // MarkNodesForGlobalAssembly();

  counter++; // Times of this method  to be called
  LoadFactor=1.0;
    
  // For pure elesticity
  if(pcs_deformation<=100&&!fem_dm->dynamic)
  { 
    elasticity=1;
    MaxIteration=1;
  }
    

  // For monolithic scheme
  if(type == 41)
  {  
     monolithic=1;
     number_of_load_steps = 1;
  }

  // system matrix 
  SetZeroLinearSolver(eqs);
  //
  //Check if Cam-Clay plasticiy involved
  bool Cam_Clay= false;
  for(l=0; l<(int)msp_vector.size(); l++)
  {
     if(msp_vector[l]->Plasticity_type==3)
     {
        Cam_Clay = true;
        break;
     } 
  }

  if(CouplingIterations==0) StoreLastSolution(); //u_n-->temp

  // For partitione THM scheme
  if(type!=41&&(H_Process||T_Process)) 
	  SetInterimVariableIndex(CouplingIterations);

 
  // Compute the maxium ratio of load increment and 
  //   predict the number of load steps 
  // --------------------------------------------------------------- 
  // Compute the ratio of the current load to initial yield load      
  // --------------------------------------------------------------- 
  number_of_load_steps=1;
  if(!elasticity&&!fem_dm->dynamic)
  { 
     InitializeNewtonSteps(0); // w=du=0 
     number_of_load_steps = 1;

     if(counter==1) // The first time this method is called.
     {          
         //  Auto increment loading  1 
         //-----------  Predict the number of load increment -------------------
         //  Prepared for the special usage. i.e, for test purpose. 
         //  Activate this segement if neccessary as well as Auto increment loading  2
         //---------------------------------------------------------------------  
         // InitializeNewtonSteps(1); // u=0 
         DisplayMsgLn("\nEvaluate load ratio: ");
         PreLoad = 1;
         GlobalAssembly();
		 PreLoad = 0;
         // {MXDumpGLS("rf_pcs.txt",1,eqs->b,eqs->x);  abort();}
         
         ExecuteLinearSolver();
 
         UpdateIterativeStep(1.0, 0); // w = w+dw 
         MaxLoadRatio=CaclMaxiumLoadRatio();		

         if(MaxLoadRatio<=1.0&&MaxLoadRatio>=0.0) number_of_load_steps=1;
         else if(MaxLoadRatio<0.0) 
         {
            if( fabs(maxStress)> MKleinsteZahl) 
            {
              int Incre=(int)fabs(MaxLoadRatio/maxStress);
              if(Incre)
              number_of_load_steps=defaultSteps;
            }       
            number_of_load_steps=defaultSteps;
         }
         else
         {				
            number_of_load_steps=(int)(LoadAmplifier*MaxLoadRatio);
            if(number_of_load_steps>=MaxLoadsteps) number_of_load_steps=MaxLoadsteps; 
         }
         cout<<"\n***Load ratio: "<< MaxLoadRatio<<endl;
 
       }
    }

	// --------------------------------------------------------------- 
    // Load steps                                                      
    // --------------------------------------------------------------- 
    /*
    //TEST
    if(Cam_Clay)
    {
        if(counter==1) number_of_load_steps = MaxLoadsteps;
        else number_of_load_steps = 1;
         if(fluid) number_of_load_steps = 10;
    }
    */
    for(l=1; l<=number_of_load_steps; l++)
    {
   		
      //  Auto increment loading  2 
      //-----------  Predict the load increment ration-------------------
      //     Prepared for the special usage. i.e, for test purpose. 
      //     Agitate this segement if neccessary as well as Auto increment loading  1
      //---------------------------------------------------------------------  
      if(elasticity!=1&&number_of_load_steps>1) 
      {
          // Predictor the size of the load increment, only perform at the first calling
           minLoadRatio = (double)l/(double)number_of_load_steps;

		   // Caculate load ratio \gamma_k 
           if(l==1)
           {  
             if(MaxLoadRatio>1.0) 
	         {
               LoadFactor=1.0/MaxLoadRatio;
               if(LoadFactor<minLoadRatio) LoadFactor=minLoadRatio; 
               LoadFactor0=LoadFactor;
	         }
	         else LoadFactor = (double)l/(double)number_of_load_steps;
           }
           else
           { 
             if(MaxLoadRatio>1.0)
               LoadFactor+=(1.0-LoadFactor0)/(number_of_load_steps-1); 
             else LoadFactor = (double)l/(double)number_of_load_steps;
           }
           // End of Caculate load ratio \gamma_k 
    
      }
 //TEST     else if(fluid)   LoadFactor = (double)l/(double)number_of_load_steps;


      // For p-u monolitic scheme. If first step (counter==1), take initial value
      if(pcs_deformation%11 == 0&&!fem_dm->dynamic) 
         InitializeNewtonSteps(1); // p=0        
	

      //
      // Initialize inremental displacement: w=0
      InitializeNewtonSteps(0);
      // 
      // Begin Newton-Raphson steps
      if(elasticity!=1) 
      {
         ite_steps = 0;
         Error = 1.0e+8;
         Norm = 1.0e+8;
         //Screan printing:
         cout<<"\n=================================================== "<<endl;         
         cout<<"*** Step "<<l<<" of the total loading steps "<<number_of_load_steps<<endl;
         cout<<"    Load factor: "<<LoadFactor<<endl;
      }
      while(ite_steps<MaxIteration) 
      {
        ite_steps++;

        // Refresh solver 
        SetZeroLinearSolver(eqs);
   
        // Assemble and solve system equation
/*
#ifdef MFC
        CString m_str;
        m_str.Format("Time step: t=%e sec, %s, Load step: %i, NR-Iteration: %i, Calculate element matrices",\
                      aktuelle_zeit,pcs_type_name.c_str(),l,ite_steps);
        pWin->SendMessage(WM_SETMESSAGESTRING,0,(LPARAM)(LPCSTR)m_str);
#endif
*/
#ifdef MFC
        CString m_str;
        m_str.Format("Time step: t=%e sec, %s, Load step: %i, NR-Iteration: %i, Assemble equation system",\
                      aktuelle_zeit,pcs_type_name.c_str(),l,ite_steps);
        pWin->SendMessage(WM_SETMESSAGESTRING,0,(LPARAM)(LPCSTR)m_str);
#endif
        GlobalAssembly();

        //Test 
        //if(counter==6&&ite_steps==2) 	{MXDumpGLS("rf_pcs.txt",1,eqs->b,eqs->x); abort();  }
  
       if(!elasticity)  Norm = CalcNormOfRHS(eqs);                               
        // 	
        if(monolithic == 0)
            SetInitialGuess_EQS_VEC();
 
#ifdef MFC
        m_str.Format("Time step: t=%e sec, %s, Load step: %i, NR-Iteration: %i, Solve equation system",\
                      aktuelle_zeit,pcs_type_name.c_str(),l,ite_steps);
        pWin->SendMessage(WM_SETMESSAGESTRING,0,(LPARAM)(LPCSTR)m_str);
#endif

        ExecuteLinearSolver();
        if(!elasticity){                                 
           // Check the convergence 
           Error1 = Error;
           if(ite_steps==1&&CouplingIterations==0) 
 //TEST          if(ite_steps==1) 
              InitialNorm = Norm;

           Error = Norm/InitialNorm;
           if(InitialNorm<10*Tolerance_global_Newton) 
                 Error = 0.01*Tolerance_global_Newton;
           if(Norm<0.001*InitialNorm)  Error = 0.01*Tolerance_global_Newton;
           if(Norm<Tolerance_global_Newton&&Error>Norm) Error = Norm;
           //	   if(NormU <TolNewton_Plasticity) 
           //    Error = NormU;  					  

           NormU = NormOfUnkonwn();
           //TEST if(Norm<0.001*TolNorm)  Error = 0.01*Tolerance_global_Newton;
           if(Norm<TolNorm)  Error = 0.01*Tolerance_global_Newton;


  	        //printf("\n\n\tNorm of unknowns %f\n", NormU);
	          //printf("\n\n\tNorm of displacement increment  %f\n", 
	          //           NormOfUpdatedNewton());	  
                   
           // Compute damping for Newton-Raphson step 
           damping=1.0;
           if(Error/Error1>1.0e-1) damping=0.5;

           //Screan printing:
           cout<<"\n--------------------------------------------------- "<<endl;         
           cout<<"*** Newton-Raphson steps: "<<ite_steps<<endl;  
           cout.width(10);
           cout.precision(3);
//           cout.setf(ios::fixed, ios::floatfield);
           cout.setf(ios::scientific);
           cout<<"\nError     "<<delim
               <<"RHS Norm 0"<<delim<<"RHS Norm  "
               <<delim<<"Damping"<<endl;
           cout<<Error<<delim<<InitialNorm<<delim
               <<Norm<<delim<<damping<<endl;
           cout<<"--------------------------------------------------- "<<endl;         

           if(Error<=Tolerance_global_Newton) break;
           //if(NormU<=TolNewton_Plasticity) break; 
           if(Error>100.0)
           {
              printf ("\n  Attention: Newton-Raphson step has diverged. Programme halt!\n");
             abort();
	          } 					

         }

		// w = w+dw for Newton-Raphson
         UpdateIterativeStep(damping, 0); // w = w+dw

       } // Newton-Raphson iteration 

       // Update stresses 
       UpdateSecondaryVariable(); 
       if(fem_dm->dynamic)
         CalcBC_or_SecondaryVariable_Dynamics();

       // Update displacements, u=u+w for the Newton-Raphson
	   // u1 = u0 for pure elasticity 
       UpdateIterativeStep(1.0,1); 

       if(elasticity!=1) 
          cout<<"Newton-Raphson (DM) terminated"<<endl;         

  } // Load step       	 
  //
  
  
  // For coupling control
  Error = 0.0;
  if(type!=41) // Partitioned scheme
  {
     for(long n=0; n<m_msh->GetNodesNumber(true); n++)
     {
       for(l=0; l<eqs->unknown_vector_dimension; l++)
       {
		  NormU= GetNodeValue(m_msh->Eqs2Global_NodeIndex[n], fem_dm->Idx_dm1[l]);
          Error += NormU*NormU;           
       }
     }
     NormU = Error;
     Error = sqrt(NormU); 
  }

  // Determine the discontinuity surface if enhanced strain methods is on.

  if(enhanced_strain_dm>0)
     Trace_Discontinuity();

  // Write Gauss stress // TEST for excavation analysis
#ifdef EXCAVATION    
    WriteGaussPointStress();
#endif

  if(type!=41&&(H_Process||T_Process)) 
	  SetInterimVariable();

  finish = clock();
 
  cout<<"CPU time elapsed in deformation process: "
      <<(double)(finish - start) / CLOCKS_PER_SEC<<"s"<<endl;
  cout<<"=================================================== "<<endl;         

  // Recovery the old solution.  Temp --> u_n	for flow proccess
  RecoverSolution();

  return Error;
}

// For THM coupling with partitioned scheme
void CRFProcessDeformation::SetInterimVariableIndex(const int CouplingIterations)
{   long i=0;
	if(fem_dm->Flow_Type==1) //Richards
	{
       fem_dm->idx_S0 = GetNodeValueIndex("SATURATION_M");      
	   if(CouplingIterations==0)
	   {
           for(i=0; i<m_msh->GetNodesNumber(false); i++)
              SetNodeValue(i, fem_dm->idx_S0, GetNodeValue(i,fem_dm->idx_S-1));
  	   }

	}
	if(fem_dm->Flow_Type>=0)  // 
	{
       if(CouplingIterations==0) fem_dm->idx_P0 = GetNodeValueIndex("POROPRESSURE0");
	   else fem_dm->idx_P0 = GetNodeValueIndex("PRESSURE_M");

	}
	if(fem_dm->T_Flag)  //
	{
	  if(CouplingIterations==0) fem_dm->idx_T0 = GetNodeValueIndex("TEMPERATURE1"); //T0
	  else fem_dm->idx_T0 = GetNodeValueIndex("TEMPERATURE_M");
	}

}
void CRFProcessDeformation::SetInterimVariable()
{
    int idx_P2_0=-1;
    if(fem_dm->Flow_Type==2)
      idx_P2_0 = GetNodeValueIndex("PRESSURE2")+1;
	else if(fem_dm->Flow_Type==0||fem_dm->Flow_Type==1)  //
      idx_P2_0 = fem_dm->idx_P1;
	if(fem_dm->Flow_Type>0)    
      fem_dm->idx_S0 = GetNodeValueIndex("SATURATION_M");
	if(fem_dm->Flow_Type>=0)    
      fem_dm->idx_P0 = GetNodeValueIndex("PRESSURE_M");
	if(fem_dm->T_Flag)  //
      fem_dm->idx_T0 = GetNodeValueIndex("TEMPERATURE_M");

	for(long i=0; i<m_msh->GetNodesNumber(false); i++)
	{
        SetNodeValue(i, fem_dm->idx_P0, GetNodeValue(i, idx_P2_0));
        if(fem_dm->Flow_Type==1)  //
            SetNodeValue(i, fem_dm->idx_S0, GetNodeValue(i, fem_dm->idx_S));
		if(fem_dm->T_Flag) 
			SetNodeValue(i, fem_dm->idx_T0, GetNodeValue(i, fem_dm->idx_T1));                 
	}

}
/*************************************************************************/

// Allocate memory for temporily used array
void CRFProcessDeformation::AllocateTempArrary(const int dims)
{
    ARRAY = (double*) Malloc(dims*sizeof(double));
} 


/**************************************************************************
 ROCKFLOW - Funktion: InitializeStress

 Aufgabe:
   Initilize all Gausss values and others       


 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   - const int NodesOfEelement: 

 Ergebnis:
   - void -

 Programmaenderungen:
   01/2003  WW  Erste Version

   letzte Aenderung:

**************************************************************************/
void CRFProcessDeformation::InitGauss(void) 
{
  const int LenMat=7;
  long i; 
  int j, gp, NGS, NGSS, MatGroup;
  int PModel = 1;
  static double Strs[6];
  ElementValue_DM *eleV_DM = NULL;
  CSolidProperties *SMat = NULL;
  

  double M_cam = 0.0; 
  double pc0 = 0.0; 
  double p=0.0;
  double q=0.0;
  double OCR = 1.0;
  int Idx_Strain[9];
   
  int NS =4;
  Idx_Strain[0] = GetNodeValueIndex("STRAIN_XX");
  Idx_Strain[1] = GetNodeValueIndex("STRAIN_YY");
  Idx_Strain[2] = GetNodeValueIndex("STRAIN_ZZ");
  Idx_Strain[3] = GetNodeValueIndex("STRAIN_XY");
  if(problem_dimension_dm==3)
  {
     NS = 6;
     Idx_Strain[4] = GetNodeValueIndex("STRAIN_XZ");
     Idx_Strain[5] = GetNodeValueIndex("STRAIN_YZ");
  }
  Idx_Strain[NS] = GetNodeValueIndex("STRAIN_PLS");

  for (i = 0; i < m_msh->GetNodesNumber(false); i++)
  {
     for(j=0; j<NS+1; j++)
        SetNodeValue(i, Idx_Strain[j], 0.0);
  }
  Mesh_Group::CElem* elem = NULL;
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
  {
     elem = m_msh->ele_vector[i];
     if (elem->GetMark()) // Marked for use
     {                           
           MatGroup = elem->GetPatchIndex();
           SMat = msp_vector[MatGroup];                          
		   fem_dm->ConfigElement(elem);
           eleV_DM = ele_value_dm[i];
           *(eleV_DM->Stress) = 0.0; 
           PModel = SMat->Plasticity_type;

           for (j = 3; j < fem_dm->ns; j++)
              Strs[j] = 0.0;

           if(PModel==2)
              *(eleV_DM->xi)= 0.0; 

           if(PModel==3)
           {
              M_cam = (*SMat->data_Plasticity)(0); 
              pc0 = (*SMat->data_Plasticity)(3); // The initial preconsolidation pressure 
              *(eleV_DM->e_i) = (*SMat->data_Plasticity)(4); // Void ratio
              OCR = (*SMat->data_Plasticity)(5); // Over consolidation ratio

              for (j = 0; j < 3; j++)
                 Strs[j] = (*SMat->data_Plasticity)(6+j);

			  /*
              g_s = GetSolidDensity(i);     
              if(g_s<=0.0)
              {
                 printf("\n !!! Input error. Gravity density should not be less than zero with Cam-Clay model\n  ");
                 abort();
              }
                            
              if(EleType== TriEle) // Triangle 
                nh = 6;                                
			  // Set soil profile. Cam-Clay. Step 2
              for (j = 0; j < nh; j++)
                 h_node[j]=GetNodeY(element_nodes[j]); //Note: for 3D, should be Z
              */
             
           }
 
           //
           //if 2D //ToDo: Set option for 3D

           // Loop over Gauss points
	   	   NGS = fem_dm->GetNumGaussPoints();
	       NGSS = fem_dm->GetNumGaussSamples();

           for (gp = 0; gp < NGS; gp++)
		   {
             /*  // For setting soil profile
             if(PModel==3) 
             {
                switch(EleType)
                {
	               case 4: // Triangle 
                     SamplePointTriHQ(gp, unit);
                     break;
	               case 2:    // Quadralateral 
                     gp_r = (int)(NGS/NGSS);
                     r = MXPGaussPkt(anzgp, gp_r);
                     break;
	             }
			 }
			 */
             switch(PModel)
             {
                case 2: // Weimar's model  
                  for (j = 0; j < LenMat; j++)
                    (*eleV_DM->MatP)(j, gp) = (*SMat->data_Plasticity)(j);
                  // Initial stress_xx, yy,zz 
                  for (j = 0; j < 3; j++)
                     (*eleV_DM->Stress)(j, gp) = (*SMat->data_Plasticity)(20+j);
                  break;   
                case 3: // Cam-Clay 
                  if(fabs(pc0)<MKleinsteZahl)
                  {                   
                     /*
                     if(EleType==TriEle)
                       ShapeFunctionTriHQ(ome, unit);
                     else
                     {
                       s = MXPGaussPkt(anzgp, gp_s);
                       MPhi2D_9N(ome, r, s);  
                     }
                   
                     depth = 0.0;
                     for (j = 0; j < nh; j++)
                        depth += fabs(h_node[j]*ome[j]); 
                     */
                       
                     //Initial stress perturbation  
                     // Ref. paper by R.I. Borja et al in Comp. Meth. in Appl. Mech & Engg 
                     // vol 78 (1990) 49-72  
  				     		
                     p = DeviatoricStress(Strs)/3.0;
                     q = 1.5*TensorMutiplication2(Strs, Strs, fem_dm->ele_dim);
                  
                     pc0 = -q/(M_cam*M_cam*p)-p;
                        
                  } 
                  //TEST
                  for (j = 0; j < 3; j++)
                    (*eleV_DM->Stress0)(j, gp) = Strs[j];
                // (*eleV_DM->Stress) = (*eleV_DM->Stress0); 
                  pc0 *= OCR; ///TEST
                  (*eleV_DM->prep0)(gp) = pc0;
                  break; 
             }
		   }

// Initial condition by LBNL
////////////////////////////////////////////////////////
#ifdef EXCAVATION
         int gp_r, gp_s, gp_t;
         double z=0.0;
         double xyz[3];
         for (gp = 0; gp < NGS; gp++)
		 {
            fem_dm->GetGaussData(gp, gp_r, gp_s, gp_t);
			fem_dm->ComputeShapefct(2);
            fem_dm->RealCoordinates(xyz);
			//THM2
            z = 250.0-xyz[1]; 
			(*eleV_DM->Stress0)(1, gp) = -2360*9.81*z;
            (*eleV_DM->Stress0)(2, gp) = 0.5*(*eleV_DM->Stress0)(1, gp);
            (*eleV_DM->Stress0)(0, gp) = 0.6*(*eleV_DM->Stress0)(1, gp);
            
            /*  
			//THM1
            z = 500-xyz[1];
            (*eleV_DM->Stress0)(2, gp) = -(0.02*z+0.6)*1.0e6;
            (*eleV_DM->Stress0)(1, gp) = -2700*9.81*z;
            (*eleV_DM->Stress0)(0, gp) = -(0.055*z+4.6)*1.0e6;
			*/
		 }
#endif
////////////////////////////////////////////////////////
		}
    }
//2. Closure simulation
	if(reload) ReadGaussPointStress();
}

/*************************************************************************
 ROCKFLOW - Funktion: TransferNodeValuesToVectorLinearSolver

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E LINEAR_SOLVER *ls: Zeiger auf eine Instanz vom Typ LINEAR_SOLVER.

 Programmaenderungen:
   02/2000   OK   aus TransferNodeValuesToVectorLinearSolver abgeleitet
   07/2005   WW  aus  TransferNodeValuesToVectorLinearSolver(OK)
*************************************************************************/
void CRFProcessDeformation::SetInitialGuess_EQS_VEC()
{
    int i;
	long j;
    int unknown_vector_dimension;
    long number_of_nodes;
    long shift=0;
    
    unknown_vector_dimension = GetUnknownVectorDimensionLinearSolver(eqs);
    for (i = 0; i < unknown_vector_dimension; i++)
    {   
        number_of_nodes=eqs->unknown_node_numbers[i];
		for(j=0; j<number_of_nodes; j++)
		{
           eqs->x[shift+j] = 
              GetNodeValue(m_msh->Eqs2Global_NodeIndex[j],eqs->unknown_vector_indeces[i]);
		}
        shift += number_of_nodes;
    }
}


/**************************************************************************
 ROCKFLOW - Funktion: UpdateIterativeStep(LINEAR_SOLVER * ls, const int Type)                                                                          

 Aufgabe:
   Update solution in Newton-Raphson procedure
                                                                          
 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: LINEAR_SOLVER * ls: linear solver 
   const double damp : damping for Newton-Raphson method  
   const int type    : 0,  update w=w+dw
                       1,  update u=u+w
                                                                          
 Ergebnis:
   - double - Eucleadian Norm
                                                                          
 Programmaenderungen:
   10/2002   WW   Erste Version
                                                                          
**************************************************************************/
void CRFProcessDeformation::UpdateIterativeStep(const double damp, const int Type)
{
    int i, j, k;
    int unknown_vector_dimension;
    long shift=0;
    long number_of_nodes;
	int ColIndex = 0;
	int Colshift = 1;


    if (!eqs) {printf(" \n Warning: solver not defined, exit from loop_ww.cc"); exit(0);}
    /* Ergebnisse eintragen */
    unknown_vector_dimension = GetUnknownVectorDimensionLinearSolver(eqs);
 
    for (i = 0; i < unknown_vector_dimension; i++)
    {   
        number_of_nodes=eqs->unknown_node_numbers[i];

		if(Type==0)
		{
		  ColIndex = eqs->unknown_vector_indeces[i]-Colshift ;
          for(j=0; j<number_of_nodes; j++)
		  { 
             k = m_msh->Eqs2Global_NodeIndex[j];
             SetNodeValue(k,ColIndex, GetNodeValue(k,ColIndex)+eqs->x[j+shift]*damp);
		  }  

          shift+=number_of_nodes;
        }
		else
        {			
		   ColIndex = eqs->unknown_vector_indeces[i];
           for(j=0; j<number_of_nodes; j++)
		   {
              k = m_msh->Eqs2Global_NodeIndex[j];
              SetNodeValue(k,ColIndex, GetNodeValue(k,ColIndex)+
                                      GetNodeValue(k, ColIndex-Colshift));
		   }

        }
    }
}


/**************************************************************************
 ROCKFLOW - Funktion: InitializeNewtonSteps(LINEAR_SOLVER * ls)                                                                          

 Aufgabe:
   Initialize the incremental unknows in Newton-Raphson procedure
                                                                          
 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: 
   LINEAR_SOLVER * ls: linear solver 
   const int type    : 0,  update w=0 (u0=0)
                       1,  update u=0 (u1=0) 
                                                                          
 Ergebnis:
   - double - Eucleadian Norm
                                                                          
 Programmaenderungen:
   10/2002   WW   Erste Version
                                                                          
**************************************************************************/
void CRFProcessDeformation::
       InitializeNewtonSteps(const int type)
{
    long i, j;
    long number_of_nodes;
	int Col = 0, start, end;
     
    start = 0;
    end = GetUnknownVectorDimensionLinearSolver(eqs);    

    // If monolithic scheme for p-u coupling, initialize p-->0 only
    if(pcs_deformation%11 == 0&&type==1)
      start = problem_dimension_dm;
    for (i = start; i < end; i++)
    {   
       Col = eqs->unknown_vector_indeces[i];
       if(type==0) Col -= 1;
       number_of_nodes=eqs->unknown_node_numbers[i];
       for (j=0; j<number_of_nodes; j++)
          SetNodeValue(m_msh->Eqs2Global_NodeIndex[j], Col, 0.0);
    } 
}


/**************************************************************************
 ROCKFLOW - Funktion: NormOfUpdatedNewton
                                                                          
 Aufgabe:
   Compute the norm of Newton increment
                                                                          
 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: LINEAR_SOLVER * ls: linear solver 
                                                                          
 Ergebnis:
   - double - Eucleadian Norm
                                                                          
 Programmaenderungen:
   12/2002   WW   Erste Version
                                                                          
**************************************************************************/
double CRFProcessDeformation::NormOfUpdatedNewton()
{
    int i, j;
    int unknown_vector_dimension;
    long number_of_nodes;
    double NormW = 0.0;
    double val; 
	int Colshift = 1;
    
    if (!eqs) {printf(" \n Warning: solver not defined, exit from loop_ww.cc"); exit(0);}
    /* Ergebnisse eintragen */
    unknown_vector_dimension = GetUnknownVectorDimensionLinearSolver(eqs);
    for (i = 0; i < unknown_vector_dimension; i++)
    {   
        number_of_nodes=eqs->unknown_node_numbers[i];
		for(j=0; j<number_of_nodes; j++)
		{
            val = GetNodeValue(m_msh->Eqs2Global_NodeIndex[j],  eqs->unknown_vector_indeces[i]-Colshift);
            NormW += val*val;
		}
    }
    return sqrt(NormW);
}


/**************************************************************************
 ROCKFLOW - Funktion: StoreDisplacement
                                                                          
 Aufgabe:
   Copy the displacement of the previous time interval to a vector
   temporarily
                                                                          
 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: LINEAR_SOLVER * ls: linear solver 
                                                                          
 Ergebnis:
   - double - Eucleadian Norm
                                                                          
 Programmaenderungen:
   10/2002   WW   Erste Version
                                                                          
**************************************************************************/

void CRFProcessDeformation::StoreLastSolution(const int ty)
{
    int i, j;
    int unknown_vector_dimension;
    long number_of_nodes;
    long shift=0;
    
    
    if (!eqs) {printf(" \n Warning: solver not defined, exit from loop_ww.cc"); exit(0);}
    /* Ergebnisse eintragen */
    unknown_vector_dimension = GetUnknownVectorDimensionLinearSolver(eqs);
    for (i = 0; i < unknown_vector_dimension; i++)
    {   
        number_of_nodes=eqs->unknown_node_numbers[i];
		for(j=0; j<number_of_nodes; j++)
		{
           ARRAY[shift+j] = 
             GetNodeValue(m_msh->Eqs2Global_NodeIndex[j],eqs->unknown_vector_indeces[i]-ty);
		}
        shift += number_of_nodes;
    }
}


/**************************************************************************
 ROCKFLOW - Funktion: RetrieveDisplacement(LINEAR_SOLVER * ls)
                                                                          
 Aufgabe:
   Retrive the displacement from the temporary array                                                                          
 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: LINEAR_SOLVER * ls: linear solver 
                                                                          
 Ergebnis:
                                                                          
 Programmaenderungen:
   10/2002   WW   Erste Version
                                                                          
**************************************************************************/
void CRFProcessDeformation::RecoverSolution(const int ty)
{
    int i, j;
    long number_of_nodes;
	int Colshift = 1;
    long shift=0;
    double tem = 0.0;
     
	int start, end;
     
    start = 0;
    end = GetUnknownVectorDimensionLinearSolver(eqs);    

    // If monolithic scheme for p-u coupling,  p_i-->p_0 only
    if(pcs_deformation%11 == 0&&ty>0)
    {
       start = problem_dimension_dm;
       for (i = 0; i < start; i++)
         shift += eqs->unknown_node_numbers[i];
    }
    for (i = start; i < end; i++)
    {   
        number_of_nodes=eqs->unknown_node_numbers[i];
		for(j=0; j<number_of_nodes; j++)
        {
           if(ty<2)
           { 
             if(ty==1) 
                 tem =  GetNodeValue(m_msh->Eqs2Global_NodeIndex[j],
				         eqs->unknown_vector_indeces[i]-Colshift);
             SetNodeValue(m_msh->Eqs2Global_NodeIndex[j], eqs->unknown_vector_indeces[i]-Colshift,
			 		 ARRAY[shift+j]);
             if(ty==1) ARRAY[shift+j] = tem;
          }
          else if(ty==2)
          {
             tem = ARRAY[shift+j];  
             ARRAY[shift+j] = GetNodeValue(m_msh->Eqs2Global_NodeIndex[j], eqs->unknown_vector_indeces[i]-Colshift);
             SetNodeValue(m_msh->Eqs2Global_NodeIndex[j],  eqs->unknown_vector_indeces[i]-Colshift,  tem);
          }
        }
        shift += number_of_nodes;
    }
}



/**************************************************************************
 ROCKFLOW - Funktion: NormOfDisp
                                                                          
 Aufgabe:
   Compute the norm of  u_{n+1}                                           
 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: LINEAR_SOLVER * ls: linear solver 
                                                                          
 Ergebnis:
   - double - Eucleadian Norm
                                                                          
 Programmaenderungen:
   10/2002   WW   Erste Version
                                                                          
**************************************************************************/
double CRFProcessDeformation::NormOfDisp()
{
    int i, j;
    int unknown_vector_dimension;
    long number_of_nodes;
	double Norm1 = 0.0;
    
    if (!eqs) {printf(" \n Warning: solver not defined, exit from loop_ww.cc"); exit(0);}
    /* Ergebnisse eintragen */
    unknown_vector_dimension = GetUnknownVectorDimensionLinearSolver(eqs);
    for (i = 0; i < unknown_vector_dimension; i++)
    {   
        number_of_nodes=eqs->unknown_node_numbers[i];
		for(j=0; j<number_of_nodes; j++)
		{
		   Norm1 += GetNodeValue(m_msh->Eqs2Global_NodeIndex[j],eqs->unknown_vector_indeces[i])
			       *GetNodeValue(m_msh->Eqs2Global_NodeIndex[j],eqs->unknown_vector_indeces[i]); 
					 
		}
    }
	return Norm1;
}




/**************************************************************************
 ROCKFLOW - Funktion: NormOfUnkonwn
                                                                          
 Aufgabe:
   Compute the norm of unkowns of a linear equation
                                                                          
 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: LINEAR_SOLVER * ls: linear solver 
                                                                          
 Ergebnis:
   - double - Eucleadian Norm
                                                                          
 Programmaenderungen:
   10/2002   WW   Erste Version
                                                                          
**************************************************************************/

double CRFProcessDeformation::NormOfUnkonwn()
{
    int i, j;
    int unknown_vector_dimension;
    long number_of_nodes;
    double NormW = 0.0;  
    
    if (!eqs) {printf(" \n Warning: solver not defined, exit from loop_ww.cc"); exit(0);}
    /* Ergebnisse eintragen */
    unknown_vector_dimension = GetUnknownVectorDimensionLinearSolver(eqs);
    for (i = 0; i < unknown_vector_dimension; i++)
    {   
        number_of_nodes=eqs->unknown_node_numbers[i];
		for(j=0; j<number_of_nodes; j++)
            NormW += eqs->x[number_of_nodes*i+j]*eqs->x[number_of_nodes*i+j];
    }
    return sqrt(NormW);
}

/**************************************************************************
 ROCKFLOW - Funktion: MaxiumLoadRatio

 Aufgabe:
   Calculate the muxium effective stress, Smax, of all Gauss points.
   (For 2-D 9 nodes element only up to now). Then compute the maxium ration
   by:
      
   Smax/Y0(initial yield stress)  
    


 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   - const int NodesOfEelement: 

 Ergebnis:
   - void -

 Programmaenderungen:
   01/2003  WW  Erste Version

  letzte Aenderung:

**************************************************************************/
//#define Modified_B_matrix
double CRFProcessDeformation::CaclMaxiumLoadRatio(void) 
{
  int j,gp, gp_r, gp_s, gp_t;
  int PModel = 1;
  long i = 0;
  double  *dstrain;

  double S0=0.0, p0 = 0.0;
  double MaxS = 0.000001;
  double EffS = 0.0;


  Mesh_Group::CElem* elem = NULL;
  ElementValue_DM *eleV_DM = NULL;
  CSolidProperties *SMat = NULL;

  // Weimar's model 
  Matrix *Mat = NULL; 
  double II = 0.0;
  double III = 0.0;


  double PRatio = 0.0;
  const double MaxR=20.0;

  int NGS, NGPS;

  gp_t = 0;
   
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
  {
      elem = m_msh->ele_vector[i];
      if (elem->GetMark()) // Marked for use
      {                             
         fem_dm->ConfigElement(elem);
         fem_dm->SetMaterial();
         eleV_DM = ele_value_dm[i];
         SMat = fem_dm->smat;
         PModel = SMat->Plasticity_type;
		 //
         switch(PModel)
         {
            case 1:
              SMat->Calculate_Lame_Constant();
              SMat->ElasticConsitutive(fem_dm->Dim(), fem_dm->De);  
              SMat->CalulateCoefficent_DP();
              S0 = MSqrt2Over3*SMat->BetaN*SMat->Y0;
              break;
            case 2:
              SMat->Calculate_Lame_Constant();
              SMat->ElasticConsitutive(fem_dm->Dim(), fem_dm->De);  
              Mat = eleV_DM->MatP; 
              break;
            case 3:
              Mat = SMat->data_Plasticity;
              S0 = (*Mat)(3);
              break;
         }
         NGS = fem_dm->GetNumGaussPoints();
         NGPS = fem_dm->GetNumGaussSamples();
         //
         for (gp = 0; gp < NGS; gp++)
         {
              switch(elem->GetElementType())
              {
	            case 4: // Triangle 
                   SamplePointTriHQ(gp, fem_dm->unit);
                   break;
	             case 2:    // Quadralateral 
                  gp_r = (int)(gp/NGPS);
                  gp_s = gp%NGPS;
                  fem_dm->unit[0] = MXPGaussPkt(NGPS, gp_r);
                  fem_dm->unit[1] = MXPGaussPkt(NGPS, gp_s);
                  break;
	           }
               fem_dm->computeJacobian(2);
			   fem_dm->ComputeGradShapefct(2);
			   fem_dm->ComputeStrain();

			   dstrain = fem_dm->GetStrain();
 
               if(PModel==3) //Cam-Clay
               {
                  p0 = ( (*eleV_DM->Stress)(0,gp)
                        +(*eleV_DM->Stress)(1,gp)
                        +(*eleV_DM->Stress)(2,gp))/3.0;
                  // Swelling index: (*SMat->data_Plasticity)(2)
                  if(fabs(p0)<MKleinsteZahl)
                     p0 = (*SMat->data_Plasticity)(3); // The initial preconsolidation pressure 

                  SMat->K = (1.0+(*eleV_DM->e_i)(gp))*fabs(p0)/(*SMat->data_Plasticity)(2);
                  SMat->G = 1.5*SMat->K*(1-2.0*SMat->PoissonRatio)/(1+SMat->PoissonRatio); 
                  SMat->Lambda = SMat->K-2.0*SMat->G/3.0;        
                  SMat->ElasticConsitutive(fem_dm->Dim(), fem_dm->De);  
               }
              
			   // Stress of the previous time step 
               for(j=0; j<fem_dm->ns; j++)
                   fem_dm->dstress[j] = (*eleV_DM->Stress)(j,gp);

               // Compute try stress, stress incremental:
               fem_dm->De->multi(dstrain, fem_dm->dstress);

               p0 = DeviatoricStress(fem_dm->dstress)/3.0;
              
               switch(PModel)
               {  
                  case 1: // Drucker-Prager model
                     EffS=sqrt(TensorMutiplication2(fem_dm->dstress, 
                                    fem_dm->dstress, fem_dm->Dim()))+3.0*SMat->Al*p0;

                     if(EffS>S0&&EffS>MaxS&&fabs(S0)> MKleinsteZahl)
                     {					   
                        MaxS = EffS;
                        PRatio = MaxS/S0;

                     }
                     break;
 
                  case 2: // Single yield surface
                     // Compute try stress, stress incremental: 
                     II=TensorMutiplication2(fem_dm->dstress, 
                                       fem_dm->dstress, fem_dm->Dim()); 
                     III=TensorMutiplication3(fem_dm->dstress, 
                             fem_dm->dstress, fem_dm->dstress, fem_dm->Dim());
                     p0 *= 3.0;
                     EffS=sqrt(II*pow(1.0+(*Mat)(5)*III/pow(II,1.5),(*Mat)(6))+0.5*(*Mat)(0)*p0*p0
                         +(*Mat)(2)*(*Mat)(2)*p0*p0*p0*p0)+(*Mat)(1)*p0+(*Mat)(3)*p0*p0;

                     if(EffS>(*Mat)(4))
                     {
                        if((*Mat)(4)>0.0)
                        { 
                          if(EffS>MaxS) MaxS = EffS;
                          PRatio = MaxS/(*Mat)(4);
                          if(PRatio>MaxR)  PRatio = MaxR;
                        }
                        else
                          PRatio = EffS;                        
                     }  
                     break;

                  case 3:  // Cam-Clay 
                     II=1.5*TensorMutiplication2(fem_dm->dstress, 
                                   fem_dm->dstress, fem_dm->Dim()); 
                     if(S0>0.0)
                     {
                        EffS = II/(p0*(*Mat)(0)*(*Mat)(0))+p0;
                        if(EffS>S0)
                          PRatio = EffS/S0;
                        else
                          PRatio = 1.0;                          
                     }
                     else
                        PRatio = 1.0;                    
                     break;                
             }
		  }               
      }
  }

  return PRatio;
}


/**************************************************************************
 ROCKFLOW - Funktion: Extropolation_GaussValue

 Aufgabe:
   Calculate the stresses of element nodes using the values at Gauss points.


 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   - const int NodesOfEelement: 
 
 Ergebnis:
   - void -

 Programmaenderungen:
   10/2002  WW  Erste Version
   07/2003  WW  Extroplolation in quadraitc triangle element is added
   letzte Aenderung:

**************************************************************************/
void CRFProcessDeformation::Extropolation_GaussValue(bool update) 
{
  int k, NS;
  long i = 0;
  int Idx_Stress[6];
  const long LowOrderNodes= m_msh->GetNodesNumber(false);
  ElementValue_DM *eval_DM;
  Mesh_Group::CElem* elem = NULL;

  // Clean nodal stresses
  NS =4;
  Idx_Stress[0] = GetNodeValueIndex("STRESS_XX");
  Idx_Stress[1] = GetNodeValueIndex("STRESS_YY");
  Idx_Stress[2] = GetNodeValueIndex("STRESS_ZZ");
  Idx_Stress[3] = GetNodeValueIndex("STRESS_XY");
  if(problem_dimension_dm==3)
  {
     NS = 6;
     Idx_Stress[4] = GetNodeValueIndex("STRESS_XZ");
     Idx_Stress[5] = GetNodeValueIndex("STRESS_YZ");
  }

  for (i = 0; i < LowOrderNodes; i++)
  {
     for(k=0; k<NS; k++)
        SetNodeValue (i, Idx_Stress[k], 0.0);
  }
 
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
  {
     elem = m_msh->ele_vector[i];
     if (elem->GetMark()) // Marked for use
     {
         fem_dm->ConfigElement(elem);
		 fem_dm->SetMaterial();
         eval_DM = ele_value_dm[i];
         (*eval_DM->Stress) += (*eval_DM->Stress0);
         fem_dm->ExtropolateGuassStress(i);
         if(!update)
          (*eval_DM->Stress) -= (*eval_DM->Stress0);
      }
   }
}

/*--------------------------------------------------------------------------
Trace discontinuity path. Belong to Geometry
--------------------------------------------------------------------------*/
void CRFProcessDeformation::Trace_Discontinuity()
{

   long k,l;
   int i, nn, Size, bFaces, bFacesCounter, intP;
   int locEleFound, numf, nPathNodes;
   bool thisLoop; //, neighborSeed;

   //Element value
   ElementValue_DM *eleV_DM;
   
   static double xn[20], yn[20], zn[20];
   static double xa[3], xb[3], n[3],ts[3];
   double v1,v2;

  
   int FNodes0[8];
   vector<long> SeedElement;

   locEleFound=0;
   nPathNodes=2; //2D element
   bFaces = 0;

   intP = 0;

   // Check all element for bifurcation
   Mesh_Group::CElem* elem = NULL;
   for (l = 0; l < (long)m_msh->ele_vector.size(); l++)
   {
     elem = m_msh->ele_vector[l];
     if (elem->GetMark()) // Marked for use
     {
        if(fem_dm->LocalAssembly_CheckLocalization(l))
        {
            locEleFound++;
            // If this is first bifurcated element, call them as seeds
            if(!Localizing) SeedElement.push_back(l);
        }
     }
   }


   if(locEleFound>0&&!Localizing) // Bifurcation inception
   {
//TEST
//mesh1   de =23;
//mesh2_iregular de = 76
//mesh coarst de = 5;
//mesh quad de=23
//crack tri de=0
 /*     
  SeedElement.clear();
   int de = 225;
   SeedElement.push_back(de);
   */

//TEST

       // Determine the seed element
       Size = (long)SeedElement.size();
       for(l=0; l<Size; l++)
       {
          k = SeedElement[l];   
          elem = m_msh->ele_vector[k];

		  numf = elem->GetFacesNumber();
          eleV_DM = ele_value_dm[k];
   
          //If seed element are neighbor. Choose one
          /*//TEST
          neighborSeed = false;
          for(m=0; m<Size; m++) 
          { 
             if(m==l) continue;
             for(i=0; i<numf; i++) 
             {
                 if(neighbor[i]==SeedElement[m]) 
                 {
                    neighborSeed = true;
                    break;
                 }
             }
          }
          if(neighborSeed) 
          {
              delete eleV_DM->orientation;
              eleV_DM->orientation = NULL;
              continue; 
          }*/
          //

		  nn = elem->GetNodesNumber(true);
          for(i=0; i<nn; i++)
          {
             // Coordinates of all element nodes
             xn[i] = elem->nodes[i]->X();
			 yn[i] = elem->nodes[i]->Y();
			 zn[i] = elem->nodes[i]->Z();
          }
         // Elements which have one only boundary face are chosen as seed element
          bFaces = -1;
          bFacesCounter = 0;
          for(i=0; i<numf; i++) 
          {
			  if(elem->neighbors[i]->onBoundary()) 
              {
                  bFaces = i;
                  bFacesCounter++;
              }
          }

          // Elements which have one only boundary face are chosen as seed element
		  if(bFacesCounter>1||bFacesCounter==0)
          { 
             eleV_DM->Localized=false;
             delete eleV_DM->orientation;
             eleV_DM->orientation = NULL;
             continue;
          }

          fem_dm->ConfigElement(elem); 
          elem->GetElementFaceNodes(bFaces, FNodes0); //2D
		  if(elem->GetElementType()==2||elem->GetElementType()==4)
             nPathNodes=2;  
          // Locate memory for points on the path of this element
          eleV_DM->NodesOnPath = new Matrix(3, nPathNodes);            
          *eleV_DM->NodesOnPath = 0.0;
          if(nPathNodes==2) // 2D
          {
              // Departure point
              (*eleV_DM->NodesOnPath)(0,0) = 0.5*(xn[FNodes0[0]]+xn[FNodes0[1]]);
              (*eleV_DM->NodesOnPath)(1,0) = 0.5*(yn[FNodes0[0]]+yn[FNodes0[1]]);
              (*eleV_DM->NodesOnPath)(2,0) = 0.5*(zn[FNodes0[0]]+zn[FNodes0[1]]);
              
              xa[0] = (*eleV_DM->NodesOnPath)(0,0);
              xa[1] = (*eleV_DM->NodesOnPath)(1,0);
              xa[2] = (*eleV_DM->NodesOnPath)(2,0);

              
              // Check oreintation again.
              ts[0] = xn[FNodes0[1]]-xn[FNodes0[0]]; 
              ts[1] = yn[FNodes0[1]]-yn[FNodes0[0]]; 
              //ts[2] = zn[FNodes0[1]]-zn[FNodes0[0]]; 
              v1 = sqrt(ts[0]*ts[0]+ts[1]*ts[1]); 
              ts[0] /= v1;        
              ts[1] /= v1;        

              n[0] = cos(eleV_DM->orientation[0]);
              n[1] = sin(eleV_DM->orientation[0]);
              v1 =  n[0]*ts[0]+n[1]*ts[1];  
              n[0] = cos(eleV_DM->orientation[1]);
              n[1] = sin(eleV_DM->orientation[1]);
              v2 =  n[0]*ts[0]+n[1]*ts[1];  
              if(fabs(v2)>fabs(v1))
              {
                 v1 = eleV_DM->orientation[0];
                 eleV_DM->orientation[0] = eleV_DM->orientation[1];
                 eleV_DM->orientation[1] = v1;
              }

              intP = fem_dm->IntersectionPoint(bFaces, xa, xb);

              (*eleV_DM->NodesOnPath)(0,1) = xb[0];
              (*eleV_DM->NodesOnPath)(1,1) = xb[1];
              (*eleV_DM->NodesOnPath)(2,1) = xb[2];

          }                   
      
           // Last element to this seed      
          DisElement *disEle = new DisElement;
          disEle->NumInterFace = 1;
          disEle->ElementIndex = k;
          disEle->InterFace = new int[1];
          disEle->InterFace[0] = intP;
          LastElement.push_back(disEle);
          ElementOnPath.push_back(k);

       }

       Localizing = true;
   }

   // Seek path from the last bifurcated element of corrsponding seeds.
   if(Localizing) 
   {
    
      Size = (long)LastElement.size();
      for(i=0; i<Size; i++)
      {
          thisLoop = true;
          while(thisLoop)
          {
             nn = MarkBifurcatedNeighbor(i);
             if(nn<0)
                 break;
             ElementOnPath.push_back(nn);
          }
      }

      Size = (long)ElementOnPath.size();
	  for(l=0; l<(long)m_msh->ele_vector.size(); l++)
      {
         elem = m_msh->ele_vector[l];
         if (elem->GetMark()) // Marked for use
	     {
             eleV_DM = ele_value_dm[l];
             if(eleV_DM->Localized)
             {
                 thisLoop = false;
                 for(k=0; k<Size; k++)
                 {
                     if(l==ElementOnPath[k])
                     {
                        thisLoop = true;
                         break;
                     }
                 }
                 if(!thisLoop)
                 {
                    eleV_DM->Localized = false;
                    delete eleV_DM->orientation;
                    eleV_DM->orientation = NULL;
                }
            }
         }
      }
   }
}


//WW
long CRFProcessDeformation::MarkBifurcatedNeighbor(const int PathIndex)
{
  int j;
  int f1, f2, nb, numf, numf1, nfnode; 
  long index, Extended;
  bool adjacent, onPath;
  ElementValue_DM *eleV_DM, *eleV_DM1;
  DisElement *disEle;
  static double n1[2],n2[2], xA[3],xB[3];
  static int Face_node[8]; // Only 2D
  Mesh_Group::CElem* elem;
  Mesh_Group::CElem* elem1;

  double pd1, pd2;

  Extended = -1;
  // 2D only
  disEle = LastElement[PathIndex]; 
  index = disEle->ElementIndex;
  f1 = disEle->InterFace[0];

  elem = m_msh->ele_vector[index];
  eleV_DM = ele_value_dm[index];

  numf = elem->GetFacesNumber();
 
  n1[0] = cos(eleV_DM->orientation[0]);
  n1[1] = sin(eleV_DM->orientation[0]);

  xA[0] = (*eleV_DM->NodesOnPath)(0,1); 
  xA[1] = (*eleV_DM->NodesOnPath)(1,1); 
  xA[2] = (*eleV_DM->NodesOnPath)(2,1); 
     
  nfnode = elem->GetElementFaceNodes(f1, Face_node); 

  // Check discintinuity path goes to which neighbor
  elem1 = elem->neighbors[f1];
  // Boundary reached
  if(elem1->GetDimension()!=elem->GetDimension()) return -1;
  nb = elem1->GetIndex(); 
  // Check if the element is already in discontinuity line/surface
  onPath = false;
  for(j=0; j<(int)ElementOnPath.size(); j++)
  {
     if(nb==ElementOnPath[j])
     {
         onPath = true;
         break;
     }
  }

  // If has neighbor and it is not on the discontinuity surface.
  if(!onPath) 
  {// Has neighbor
     if(ele_value_dm[nb]->Localized)
     {
         adjacent = false;
         numf1 = elem1->GetFacesNumber();
         eleV_DM1 = ele_value_dm[nb];
         fem_dm->ConfigElement(elem1);
         // Search faces of neighbor's neighbors 
         for(j=0; j<numf1; j++)
         {
            // Neighbor is a face on surface
            if(elem1->neighbors[j]->GetDimension()!=elem1->GetDimension()) continue;   
            if(index!=elem1->neighbors[j]->GetIndex()) continue;
            {
                adjacent = true;
                Extended = nb;    
                // Choose a smooth direction
                n2[0] = cos(eleV_DM1->orientation[0]);
                n2[1] = sin(eleV_DM1->orientation[0]);
                pd1 = n1[0]*n2[0]+n1[1]*n2[1];
                n2[0] = cos(eleV_DM1->orientation[1]);
                n2[1] = sin(eleV_DM1->orientation[1]);
                pd2 = n1[0]*n2[0]+n1[1]*n2[1];
                if(pd2>pd1) // Always use the first entry of orientation
                {
                   // Swap the values
                   pd1 = eleV_DM1->orientation[1];
                   eleV_DM1->orientation[1] = eleV_DM1->orientation[0];
                   eleV_DM1->orientation[0] = pd1;
                }
                eleV_DM1->NodesOnPath = new Matrix(3, 2);            
                *eleV_DM1->NodesOnPath = 0.0;
                // Get another intersection point
                f2 = fem_dm->IntersectionPoint(j, xA, xB);
                (*eleV_DM1->NodesOnPath)(0,0) = xA[0];
                (*eleV_DM1->NodesOnPath)(1,0) = xA[1];
                (*eleV_DM1->NodesOnPath)(2,0) = xA[2];
                (*eleV_DM1->NodesOnPath)(0,1) = xB[0];
                (*eleV_DM1->NodesOnPath)(1,1) = xB[1];
                (*eleV_DM1->NodesOnPath)(2,1) = xB[2];     

                // Last element
                disEle->ElementIndex = nb;
                disEle->NumInterFace = 1;
                disEle->InterFace[0] = f2;
                break;
            }
        }

        // If not on the discontinuity surface
        // Release memory
        if(!adjacent) 
        {
            delete eleV_DM1->orientation;
            eleV_DM1->orientation = NULL;
            eleV_DM1->Localized = false;              
        }
    }
 }

  // If true, discontinuity extended to its neighbor
  return Extended;
}

/**************************************************************************
FEMLib-Method: 
Task: Assemble local matrices and RHS for each element
Programing:
02/2005 WW
**************************************************************************/
void CRFProcessDeformation:: GlobalAssembly()
{
   long i;
   // 1.
   // Assemble deformation eqs
   CElem* elem = NULL;
   m_msh->SwitchOnQuadraticNodes(true);
   for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
   {
       elem = m_msh->ele_vector[i];
       if (elem->GetMark()) // Marked for use
	   {
		   fem_dm->ConfigElement(elem);
		   fem_dm->LocalAssembly(i, 0);
	   } 
   }
   if(type==41) // p-u monolithic scheme
   {
      if(!fem_dm->dynamic)
          RecoverSolution(1);  // p_i-->p_0
      // 2.
      // Assemble pressure eqs
      for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
      {
          elem = m_msh->ele_vector[i];
          if (elem->GetMark()) // Marked for use
          {
            fem->ConfigElement(elem);
            fem->Assembly();
         }
      }
      if(!fem_dm->dynamic)
         RecoverSolution(2);  // p_i-->p_0
   }

#ifdef PARALLEL
   DDCAssembleGlobalMatrix();
#endif
   // Apply Neumann BC

   IncorporateSourceTerms();
   // Apply Dirchlete bounday condition
   IncorporateBoundaryConditions();
   if(fem_dm->dynamic)
      CalcBC_or_SecondaryVariable_Dynamics(true);
}

/**************************************************************************
FEMLib-Method: 
Task: Update stresses and straines at each Gauss points
Argument: 
Programing:
02/2005 WW
**************************************************************************/
void CRFProcessDeformation::UpdateSecondaryVariable()
{
   long i;
   Mesh_Group::CElem* elem = NULL;
   for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
   {
      elem = m_msh->ele_vector[i];
      if (elem->GetMark()) // Marked for use
	  { 
          fem_dm->ConfigElement(elem);
          fem_dm->LocalAssembly(i, 1);
	  }
   }
}

// Coupling
void CRFProcessDeformation::ConfigureCoupling()
{
    fem_dm->ConfigureCoupling(this, Shift);
}

/**************************************************************************
 ROCKFLOW - Funktion: WriteGaussPointStress()

 Aufgabe:
   Write Gauss point stresses to a file

 Programmaenderungen:
   03/2005  WW  Erste Version
   letzte Aenderung:

**************************************************************************/
void CRFProcessDeformation::WriteGaussPointStress()
{
    int j, k, NGS;
    long i;
    string deli = " ";
    string StressFileName = FileName+".stress";
    fstream file_stress (StressFileName.data(),ios::trunc|ios::out);   
    ElementValue_DM *eleV_DM = NULL;

    int ActiveElements = 0;
    Mesh_Group::CElem* elem = NULL;
    for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
    {
      elem = m_msh->ele_vector[i];
      if (elem->GetMark()) // Marked for use
           ActiveElements++;
    }
  
     
    file_stress<<ActiveElements<<endl;
    for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
    {
        elem = m_msh->ele_vector[i];
        if (elem->GetMark()) // Marked for use
		{                                      
           eleV_DM = ele_value_dm[i];
		   fem_dm->ConfigNumerics(elem->GetIndex());        
           file_stress<<i<<endl;
	   	   NGS = fem_dm->GetNumGaussPoints();
           file_stress<<NGS<<endl;
           for(j=0; j<NGS; j++)
           {
              for(k=0; k<fem_dm->ns; k++)
                 file_stress<<(*eleV_DM->Stress)(k, j)+(*eleV_DM->Stress0)(k, j)<<deli;
              file_stress<<endl;
           }               
           file_stress<<endl;           
        }
   }
   
   file_stress.close();
}
/**************************************************************************
 ROCKFLOW - Funktion: ReadGaussPointStress()

 Aufgabe:
   Read Gauss point stresses to a file

 Programmaenderungen:
   03/2005  WW  Erste Version
   letzte Aenderung:

**************************************************************************/
void CRFProcessDeformation::ReadGaussPointStress()
{
    int j, k, NGS;
    long i,index, ActiveElements;
    string deli = " ";
    string StressFileName = FileName+".stress";
    fstream file_stress (StressFileName.data(), ios::in);   
    ElementValue_DM *eleV_DM = NULL;

	Mesh_Group::CElem* elem = NULL;
    file_stress>>ActiveElements>>ws;
    for (i=0;i<ActiveElements;i++)
    {                                      

        file_stress>>index>>ws;
        elem = m_msh->ele_vector[index];
        eleV_DM = ele_value_dm[index];
		fem_dm->ConfigNumerics(elem->GetIndex());
        //
        file_stress>>NGS>>ws;
        //
        for(j=0; j<NGS; j++)
        {
           for(k=0; k<fem_dm->ns; k++)
              file_stress>>(*eleV_DM->Stress0)(k, j);
           file_stress>>ws;
        }               

        file_stress>>ws;
       // (*eleV_DM->Stress) = (*eleV_DM->Stress0);
    }  


   file_stress.close();
   
}


/**************************************************************************
 ROCKFLOW - Funktion: ReleaseLoadingByExcavation()

 Aufgabe:
   Compute the nodal forces produced by excavated body

 Programmaenderungen:
   04/2005  WW  Erste Version
   letzte Aenderung:

**************************************************************************/
void CRFProcessDeformation::ReleaseLoadingByExcavation()
{
   long i, actElements;
   int j, k, SizeSt, SizeSubD;

   vector<int> ExcavDomainIndex;
   vector<long> NodesOnCaveSurface;

   CSourceTerm *m_st = NULL;
   SizeSt = (int)st_vector.size();

   for(k=0; k<SizeSt; k++)
   {
      m_st = st_vector[k];
      if(m_st->pcs_pv_name.find("EXCAVATION")!=string::npos)
      {
          ExcavDomainIndex.push_back(m_st->geo_type);
      }                     
   }
   SizeSubD = (int)ExcavDomainIndex.size();

   // 1. De-active host domain to be exvacated
   actElements = 0;
   CElem* elem = NULL;
   for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
   {
      elem = m_msh->ele_vector[i];
      elem->SetMark(false);
      for(k=0; k<SizeSubD; k++)
      {
         if(elem->GetPatchIndex()==ExcavDomainIndex[k])            
           elem->SetMark(true);
      }
      if(elem->GetMark()) actElements++;
   }
   if(actElements==0) 
   {
       cout<<"No element specified for excavation. Please check data in .st file "<<endl;
       abort();
   }
   
   // 2. Compute the released node loading
   SetZeroLinearSolver(eqs);
   PreLoad = 11;
   LoadFactor = 1.0;
  for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
   {
      elem = m_msh->ele_vector[i];
      if (elem->GetMark()) // Marked for use
	  {
		  fem_dm->ConfigElement(elem);
          fem_dm->LocalAssembly(i, 0);
	  }       
   }

   // 3 --------------------------------------------------------
   // Store the released loads to source term buffer
   long number_of_nodes;
   CNodeValue *m_node_value = NULL;
   CSourceTermGroup *m_st_group = NULL;
   CGLPolyline *m_polyline = NULL;
   Surface *m_surface = NULL;
   vector<long> nodes_vector(0);

   number_of_nodes = 0;
   for(k=0; k<SizeSt; k++)
   {
      // Get nodes on cave surface
      m_st = st_vector[k];
      if(m_st->pcs_pv_name.find("EXCAVATION")==string::npos) continue;
      if(m_st->geo_type_name.compare("POLYLINE")==0)
      { 
         m_polyline = GEOGetPLYByName(m_st->geo_name);//CC 10/05
         if(m_polyline) 
         {
            m_st->SetPolyline(m_polyline);
            if(m_polyline->type==100)
				m_msh->GetNodesOnArc(m_polyline, nodes_vector); //WW
	        else
		    {
               m_polyline->type = 3;  
			   m_msh->GetNODOnPLY(m_polyline, nodes_vector);
	        }
          }
      }
      if(m_st->geo_type_name.compare("SURFACE")==0)
      { 
          
          m_surface = GEOGetSFCByName(m_st->geo_name);//CC 10/05
          if(m_surface) {
			  m_msh->GetNODOnSFC_PLY(m_surface, nodes_vector);
          }
      }
      // Set released node forces from eqs->b;
      number_of_nodes = (int)nodes_vector.size();
      for(j=0;j<problem_dimension_dm;j++)
      {
          m_st_group = STGetGroup(pcs_type_name,pcs_primary_function_name[j]);
          if(!m_st_group) {
             m_st_group = new CSourceTermGroup();
             st_group_list.push_back(m_st_group);
             m_st_group->pcs_name = pcs_primary_function_name[j];
          }
          m_st_group->RecordNodeVSize((int)m_st_group->group_vector.size());
          for(i=0;i<number_of_nodes;i++){
             m_node_value = new CNodeValue();
             m_node_value->msh_node_number = nodes_vector[i]+Shift[j];
             m_node_value->geo_node_number = nodes_vector[i];
			 m_node_value->node_value = -eqs->b[m_msh->nod_vector[m_node_value->geo_node_number]
                                                 ->GetEquationIndex()+Shift[j]];
             m_node_value->CurveIndex = -1;
             m_st_group->group_vector.push_back(m_node_value);
             m_st_group->st_group_vector.push_back(m_st);
          }
      }
      
   }


   // Activate the host domain for excavtion analysis
   for (i = 0; i < (long)m_msh->ele_vector.size(); i++)
   {
      elem = m_msh->ele_vector[i];
	  j=elem->GetMark();
	  elem->SetMark(!j);
   }
   PreLoad = 1;

//TEST OUTPUT
//{MXDumpGLS("rf_pcs.txt",1,eqs->b,eqs->x);  abort();}
}


/**************************************************************************
 ROCKFLOW - Funktion: ExcavationSimulating()

 Aufgabe:
   Exacavation simulating

 Programmaenderungen:
   04/2005  WW  Erste Version
   letzte Aenderung:

**************************************************************************/
void CRFProcessDeformation::ExcavationSimulating()
{
    cout<<"------------------------------------------"<<endl;
    cout<<"*** Simulating excavation"<<endl;

    int i, j;
    int no_out =(int)out_vector.size();
    const int OrigType = type;
    const int Orig_pcs_deformation = pcs_deformation;
    const int Orig_pcs_number_of_primary_nvals = pcs_number_of_primary_nvals;
	bool update;
    COutput *m_out = NULL;
  
    if(type==41)  pcs_number_of_primary_nvals--;
    type = 4;
    pcs_deformation = 1; //Elasticity excavation

    // If an initial stress state is given, skip this
    // Establishing gravity force profile 
    if (m_num&&m_num->GravityProfile==1)
	{ 
       GravityForce = true;
       cout<<"1. Establish gravity force profile..."<<endl;
       counter = 0;
       Execute();
	   update = false;
	}
	else // Initial stress from input
	   update = true; 	 
    Extropolation_GaussValue(update);

    // Write the results
    for(i=0;i<no_out;i++){
      m_out = out_vector[i];
      m_out->time = -1.0;
      if(m_out->geo_type_name.find("DOMAIN")!=string::npos)
      {
          cout << "Data output" << endl;
          m_out->NODWriteDOMDataTEC(); //OK
      }    
    }

    // Excavating  
    cout<<"2. Excavating..."<<endl;
    counter = 0;
    InitializeNewtonSteps(0);
    InitializeNewtonSteps(2);
	GravityForce = false;
    ReleaseLoadingByExcavation();
	UpdateInitialStress(); 
	
	Execute();
    Extropolation_GaussValue(true);

    // Write the results
    for(i=0;i<no_out;i++){
      m_out = out_vector[i];
      m_out->time = 0.0;
      if(m_out->geo_type_name.find("DOMAIN")!=string::npos)
      {
          cout << "Data output" << endl;
          m_out->NODWriteDOMDataTEC(); //OK
      }    
    }

	UpdateInitialStress();
    InitializeNewtonSteps(2);

    // Remove boundary loading by excavated domain
    
    CSourceTermGroup *m_st_group = NULL;
    for(j=0;j<problem_dimension_dm;j++)
    {
        m_st_group = m_st_group->Get(pcs_primary_function_name[j]);
        if(m_st_group)
        {
           for(i=(int)m_st_group->group_vector.size();i>m_st_group->GetOrigNodeVSize();i--)
		   {
              delete m_st_group->group_vector[i];
              m_st_group->group_vector.pop_back();
			  m_st_group->st_group_vector.pop_back();
		   }
        }
    }
    
    type = OrigType;
    pcs_deformation = Orig_pcs_deformation;
    pcs_number_of_primary_nvals = Orig_pcs_number_of_primary_nvals;
    PreLoad = 0;
 
    // If monlithic scheme, relocate memory for linear solver
    if(type==41)
    {
       (int*) Free(eqs->unknown_vector_indeces);
       (long*) Free(eqs->unknown_node_numbers);
       (int*) Free(eqs->unknown_update_methods);
        DestroyLinearSolver(eqs);
        j = Shift[pcs_number_of_primary_nvals-1]+m_msh->GetNodesNumber(false);
        eqs = CreateLinearSolverDim(m_num->ls_storage_method,pcs_number_of_primary_nvals,j);    
        InitializeLinearSolver(eqs,m_num);  
        InitEQS();     
    } 
    
    cout<<"*** End of excavation"<<endl;
    cout<<"------------------------------------------"<<endl;
}

/**************************************************************************
 GEOSYS - Funktion: CalcBC_or_SecondaryVariable_Dynamics(bool BC);
 Programmaenderungen:
   05/2005  WW  Erste Version
   letzte Aenderung:

**************************************************************************/
bool CRFProcessDeformation::CalcBC_or_SecondaryVariable_Dynamics(bool BC)
{
  char *function_name[7];
  long i, j;
  double v, bc_value, time_fac = 1.0;

  vector<int> bc_type;
  long bc_msh_node;
  long bc_eqs_index;
  int interp_method=0;
  int curve, valid=0;
  int idx_disp[3], idx_vel[3], idx_acc[3], idx_acc0[3];
  int idx_pre, idx_dpre, idx_dpre0;
  int nv, k;
  
  int Size = m_msh->GetNodesNumber(true)+ m_msh->GetNodesNumber(false);

  bc_type.resize(Size);

  v = 0.0;
  // 0: not given
  // 1, 2, 3: x,y, or z is given
  for(i=0; i<Size; i++) bc_type[i] = 0; 

  idx_dpre0 = GetNodeValueIndex("PRESSURE_RATE1");  
  idx_dpre = idx_dpre0+1;
  idx_pre = GetNodeValueIndex("PRESSURE1");

  nv = 2*problem_dimension_dm+1;  
  if(m_msh->GetCoordinateFlag()/10==2) // 2D
  {
    function_name[0] = "DISPLACEMENT_X1";
    function_name[1] = "DISPLACEMENT_Y1";
    function_name[2] = "VELOCITY_X1";
    function_name[3] = "VELOCITY_Y1";
    function_name[4] = "PRESSURE1"; 
    idx_disp[0] = GetNodeValueIndex("DISPLACEMENT_X1");
    idx_disp[1] = GetNodeValueIndex("DISPLACEMENT_Y1");
    idx_vel[0] = GetNodeValueIndex("VELOCITY_X1");
    idx_vel[1] = GetNodeValueIndex("VELOCITY_Y1");
    idx_acc0[0] = GetNodeValueIndex("ACCELERATION_X1");
    idx_acc0[1] = GetNodeValueIndex("ACCELERATION_Y1");
    idx_acc[0] =  idx_acc0[0]+1;
    idx_acc[1] =  idx_acc0[1]+1;
  }
  else if(m_msh->GetCoordinateFlag()/10==3)  //3D
  {
    function_name[0] = "DISPLACEMENT_X1";
    function_name[1] = "DISPLACEMENT_Y1";
    function_name[2] = "DISPLACEMENT_Z1";
    function_name[3] = "VELOCITY_X1";
    function_name[4] = "VELOCITY_Y1";
    function_name[5] = "VELOCITY_Z1";
    function_name[6] = "PRESSURE1"; 
    idx_disp[0] = GetNodeValueIndex("DISPLACEMENT_X1");
    idx_disp[1] = GetNodeValueIndex("DISPLACEMENT_Y1");
    idx_disp[2] = GetNodeValueIndex("DISPLACEMENT_Z1");
    idx_vel[0] = GetNodeValueIndex("VELOCITY_X1");
    idx_vel[1] = GetNodeValueIndex("VELOCITY_Y1");
    idx_vel[2] = GetNodeValueIndex("VELOCITY_Z1");
    idx_acc0[0] = GetNodeValueIndex("ACCELERATION_X1");
    idx_acc0[1] = GetNodeValueIndex("ACCELERATION_Y1");
    idx_acc0[2] = GetNodeValueIndex("ACCELERATION_Z1");
    for(k=0; k<3; k++)
      idx_acc[k] = idx_acc0[k]+1;
  }
  
  


  //-----------------------------------------------------------------------
  CBoundaryConditionsGroup *m_bc_group = NULL;
  for(j=0; j<nv; j++)
  {
     m_bc_group = m_bc_group->Get((string)function_name[j]);
     if(!m_bc_group) continue;

     //-----------------------------------------------------------------------
     // Dirichlet-Randbedingungen eintragen
     long bc_group_vector_length = (long)m_bc_group->group_vector.size();
     for(i=0;i<bc_group_vector_length;i++) {
       bc_msh_node = m_bc_group->group_vector[i]->msh_node_number-Shift[j];
       if(!m_msh->nod_vector[bc_msh_node]->GetMark()) 
            continue;
       if(bc_msh_node>=0) {
          curve =  m_bc_group->group_vector[i]->CurveIndex;
          if(curve>0) {
             time_fac = GetCurveValue(curve,interp_method,aktuelle_zeit,&valid);
             if(!valid)
               continue;
	      }
	      else
             time_fac = 1.0;
           bc_value = time_fac*m_bc_group->group_vector[i]->node_value; 
		   bc_eqs_index = m_msh->nod_vector[bc_msh_node]->GetEquationIndex();
           
           if(BC)
           {
              if(j<problem_dimension_dm) // da 
              {
                 bc_eqs_index += Shift[j]; 
                 // da = v = 0.0;                      
                 MXRandbed(bc_eqs_index,0.0,eqs->b);
              }
              else if(j==nv-1) // P
              {
                 bc_eqs_index += Shift[problem_dimension_dm]; 
                 // da = v = 0.0;                      
                 MXRandbed(bc_eqs_index,0.0,eqs->b);
                  
              }
           } 
           else
           {
              // Bit operator
              if(!(bc_type[bc_eqs_index]&(int)pow(2.0, (double)j)))
                 bc_type[bc_eqs_index] += (int)pow(2.0, (double)j);                   
              if(j<problem_dimension_dm) // Disp 
              {
                  SetNodeValue(bc_eqs_index, idx_disp[j], bc_value);
                  SetNodeValue(bc_eqs_index, idx_vel[j], 0.0);
                  SetNodeValue(bc_eqs_index, idx_acc[j], 0.0);
              }
              else if(j>=problem_dimension_dm&&j<nv-1) // Vel  
              {
                  v = GetNodeValue(bc_eqs_index, idx_disp[j]);
                  v += bc_value*dt
                       +0.5*dt*dt*(ARRAY[bc_eqs_index+Shift[j]]
                       +m_num->GetDynamicDamping_beta2()
                        *GetNodeValue(bc_eqs_index, idx_acc0[j]));
                  SetNodeValue(bc_eqs_index, idx_disp[j], v);
                  SetNodeValue(bc_eqs_index, idx_vel[j], bc_value);

              }
              else if(j==nv-1) // Vel  
              {  //p
                  SetNodeValue(bc_eqs_index, idx_pre, bc_value);
                  SetNodeValue(bc_eqs_index, idx_dpre, 0.0);
              }                  
           } 

         }
      }
  }
  if(BC) return BC;
 

  // BC
  for(i=0; i<m_msh->GetNodesNumber(true); i++)
  {
     for(k=0; k<problem_dimension_dm; k++)
     {
        // If boundary
        if(bc_type[i]&(int)pow(2.0, (double)k)) 
           continue; //u
        // 
        v = GetNodeValue(i, idx_disp[k]);
        v += GetNodeValue(i, idx_vel[k])*dt
                  +0.5*dt*dt*(ARRAY[i+Shift[k]]
                  +m_num->GetDynamicDamping_beta2()
                  *GetNodeValue(i, idx_acc0[k]));
        SetNodeValue(i, idx_disp[k], v);
        if(bc_type[i]&(int)pow(2.0, (double)(k+problem_dimension_dm)))
            continue;
        // v
        v = GetNodeValue(i, idx_vel[k]);
        v += dt*ARRAY[i+Shift[k]]
                  +m_num->GetDynamicDamping_beta1()*dt
                  *GetNodeValue(i, idx_acc0[k]);
        SetNodeValue(i, idx_vel[k], v);
        
     }          
  }
  
  for(i=0; i<m_msh->GetNodesNumber(false); i++)
  {
    if(bc_type[i]&(int)pow(2.0, (double)(nv-1)))
         continue;
     v = GetNodeValue(i, idx_pre);
     v +=  ARRAY[i+Shift[problem_dimension_dm]]*dt
          +m_num->GetDynamicDamping_beta1()*dt*
           GetNodeValue(i, idx_dpre0);     
     SetNodeValue(i, idx_pre, v);          
  }  
   
 
  
  return BC;
}
}// end namespace
