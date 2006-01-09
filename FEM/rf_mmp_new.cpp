/**************************************************************************
FEMLib-Object: MAT-MP 
Task: MediumProperties
Programing:
01/2004 OK Implementation
**************************************************************************/

#include "stdafx.h" //MFC
#include "makros.h"
// C++ STL
#include <iostream>
using namespace std;
// FEMLib
#include "nodes.h"
#include "tools.h"
#include "rf_pcs.h"
#include "femlib.h"
extern double* GEOGetELEJacobianMatrix(long number,double *detjac);
#include "mathlib.h"
#include "rf_mfp_new.h"
#include "rf_msp_new.h"
//#include "material.h"
#include "rf_tim_new.h"
#include "rfmat_cp.h"
extern double gravity_constant;
using SolidProp::CSolidProperties;
// LIB
#include "geo_strings.h"
#include "rfstring.h"
// this
#include "rf_mmp_new.h"
#include "rf_react.h"
#include "elements.h"
#include "gs_project.h"
// Gauss point veclocity
#include "fem_ele_std.h"
// MSHLib
#include "msh_lib.h"

// MAT-MP data base lists
list<string>keywd_list; 
list<string>mat_name_list;
list<char*>mat_name_list_char;
list<CMediumProperties*>db_mat_mp_list;
// MAT-MP list
vector<CMediumProperties*>mmp_vector;

using FiniteElement::ElementValue;
using FiniteElement::CFiniteElementStd;
////////////////////////////////////////////////////////////////////////////
// CMediumProperties
////////////////////////////////////////////////////////////////////////////

/**************************************************************************
FEMLib-Method: CMediumProperties
Task: constructor
Programing:
02/2004 OK Implementation
last modification:
**************************************************************************/
CMediumProperties::CMediumProperties(void)
{
  name = "DEFAULT";
  mode = 0;
  selected = false;
  m_pcs = NULL; //OK
  // GEO
  geo_dimension = 0;
  geo_area = 1.0; //OK
  porosity_model = -1;
  porosity_model_values[0] = 0.1;
  tortuosity_model = -1;
  tortuosity_model_values[0] = 1.;
  // MSH
  m_msh = NULL; //OK
  // flow
  storage_model = -1;
  storage_model_values[0]= 0.;
  permeability_model = -1;
  permeability_tensor_type = 0;
  permeability_tensor[0] = 1.e-13;
  conductivity_model = -1;
  flowlinearity_model = 0;
  capillary_pressure_model = -1;
  capillary_pressure_model_values[0] = 0.0;
  permeability_saturation_model[0] = -1;
  permeability_saturation_model_values[0] = 1.0;
  unconfined_flow_group = -1;
  // surface flow
  friction_coefficient = -1;
  friction_model = -1;
  // mass transport
  // heat transport
  heat_dispersion_model = -1; //WW
  heat_dispersion_longitudinal = 0.;
  heat_dispersion_transverse = 0.;
  heat_diffusion_model = -1; //WW
  geo_area = 1.0;
  geo_type_name = "DOMAIN"; //OK
}

/**************************************************************************
FEMLib-Method: CMediumProperties
Task: destructor
Programing:
02/2004 OK Implementation
last modification:
**************************************************************************/
CMediumProperties::~CMediumProperties(void)
{
  geo_name_vector.clear();
}

////////////////////////////////////////////////////////////////////////////
// IO functions
////////////////////////////////////////////////////////////////////////////

/**************************************************************************
FEMLib-Method: MMPRead
Task: master read functionn
Programing:
02/2004 OK Implementation
last modification:
**************************************************************************/
bool MMPRead(string base_file_name)
{
  //----------------------------------------------------------------------
  MMPDelete();  
  //----------------------------------------------------------------------
  cout << "MMPRead" << endl;
  CMediumProperties *m_mat_mp = NULL;
  char line[MAX_ZEILE];
  string sub_line;
  string line_string;
  ios::pos_type position;
  //========================================================================
  // file handling
  string mp_file_name;
  mp_file_name = base_file_name + MMP_FILE_EXTENSION;
  ifstream mp_file (mp_file_name.data(),ios::in);
  if (!mp_file.good()){
    cout << "! Error in MMPRead: No material data !" << endl;
    return false;
  }  
  mp_file.seekg(0L,ios::beg);
  //========================================================================
  // keyword loop
  while (!mp_file.eof()) {
    mp_file.getline(line,MAX_ZEILE);
    line_string = line;
    if(line_string.find("#STOP")!=string::npos)
      return true;
    //----------------------------------------------------------------------
    if(line_string.find("#MEDIUM_PROPERTIES")!=string::npos) { // keyword found
      m_mat_mp = new CMediumProperties();
      position = m_mat_mp->Read(&mp_file);
      m_mat_mp->number = (int)mmp_vector.size(); //OK41
      mmp_vector.push_back(m_mat_mp);
      mp_file.seekg(position,ios::beg);
    } // keyword found
  } // eof
  return true;
  // Tests
}

/**************************************************************************
FEMLib-Method: CMediumProperties::Read
Task: read functionn
Programing:
02/2004 OK Template
08/2004 CMCD Implementation
10/2004 MX/OK Porosity model 3, swelling
11/2004 CMCD String streaming
07/2005 MB porosity_file, permeability_file, GEO_TYPE layer
10/2005 OK GEO_TYPE geo_name_vector
last modification:
**************************************************************************/
// Order of Key Words
/*
			0. $NAME
				(i)    _BORDEN
			1. $GEOTYPE
				(i)		_CLAY
				(ii)	_SILT
				(iii)	_SAND
				(iv)	_GRAVEL
				(v)		_CRYSTALINE
			2. $GEOMETRY
				(i)		_DIMENSION
				(ii)	_AREA
			3. $POROSITY
			4. $TORTUOSITY
			5. $MOBILE_IMOBILE_MODEL
			6. $LITHOLOGY_GRAIN_CLASS
			7. $FLOWLINEARITY
			8. $SORPTION_MODEL
			9. $STORAGE
			11.$PERMEABILITY
			12.$PERMEABILITY_FUNCTION_
				(1)		DEFORMATION
				(2) 	PRESSURE
				(3)	    SATURATION
				(4)	    STRESS
				(5)		VELOCITY
				(6)		POROSITY
			13.$CAPILLARY_PRESSURE
			14.$MASSDISPERSION
				(i)		_LONGITUDINAL
				(ii)	_TRANSVERSE
			15.$HEATDISPERSION
				(i)		_LONGITUDINAL
				(ii)	_TRANSVERSE
			19.$ELECTRIC_CONDUCTIVITY
			20.$UNCONFINED_FLOW_GROUP
			21.$FLUID_EXCHANGE_WITH_OTHER_CONTINUA
*/			
ios::pos_type CMediumProperties::Read(ifstream *mmp_file)
{
  int i;
  string line_string;
  std::stringstream in;
  ios::pos_type position;
  string dollar("$");
  string hash("#");
  bool new_subkeyword = false;
  bool new_keyword = false;
  string m_string;
  //======================================================================
   while (!new_keyword) {
    new_subkeyword = false;
    position = mmp_file->tellg();
	line_string = GetLineFromFile1(mmp_file);
	if(line_string.size() < 1) break;
    if(line_string.find(hash)!=string::npos) {
      new_keyword = true;
      break;
    }
    //--------------------------------------------------------------------
    //NAME
    if(line_string.find("$NAME")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> name; //sub_line
	  in.clear();
      continue;
    }
    //--------------------------------------------------------------------
    //GEO_TYPE
    if(line_string.find("$GEO_TYPE")!=string::npos) { //subkeyword found
/*
      in.str(GetLineFromFile1(mmp_file));
      in >> geo_type_name;
      in.clear();
      if(geo_type_name.compare("DOMAIN")==0)
        continue;
      while(!(geo_name.find("$")!=string::npos)&&(!(geo_name.find("#")!=string::npos)))
      {
        position = mmp_file->tellg();
        in.str(GetLineFromFile1(mmp_file));
        in >> geo_name;
        in.clear();
        if(!(geo_name.find("$")!=string::npos))
          geo_name_vector.push_back(geo_name);
      }
      mmp_file->seekg(position,ios::beg);
      continue;
*/
      while(!(m_string.find("$")!=string::npos)&&(!(m_string.find("#")!=string::npos)))
      {
        position = mmp_file->tellg();
        in.str(GetLineFromFile1(mmp_file));
        in >> m_string >> geo_name;
        in.clear();
        if(!(m_string.find("$")!=string::npos)&&(!(m_string.find("#")!=string::npos))){
          geo_type_name = m_string;
          geo_name_vector.push_back(geo_name);
        }
      }
      mmp_file->seekg(position,ios::beg);
      continue;
    }
    //....................................................................
// ToDo to GeoLib
//2i..GEOMETRY_DIMENSION
//------------------------------------------------------------------------
    if(line_string.find("$GEOMETRY_DIMENSION")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> geo_dimension;
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
// ToDo to GeoLib
//2ii..GEOMETRY_AREA
//------------------------------------------------------------------------
    if(line_string.find("$GEOMETRY_AREA")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> line_string;
      if(line_string.find("FILE")!=string::npos){
        in >> geo_area_file;
      }
      else{
        geo_area = strtod(line_string.data(),NULL);
      }
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
//3..POROSITY
//------------------------------------------------------------------------
    if(line_string.find("$POROSITY")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> porosity_model;
	  int m=0;
      switch(porosity_model) {
        case 0: // n=f(x)
		  in >> porosity_curve;
          break;
        case 1: // n=const
          in >> porosity_model_values[0];
          break;
		case 2: // f(normal effective stress for fracture systems)
		  in >> porosity_model_values[0];
		  in >> porosity_model_values[1];
		  in >> porosity_model_values[2];
		  in >> porosity_model_values[3];
		  pcs_name_vector.push_back("PRESSURE1");
		  break;
		case 3: // Chemical swelling model
		  in >> porosity_model_values[0]; // Initial porosity
		  in >> porosity_model_values[1]; // Specific surface[m^2/g]
		  in >> porosity_model_values[2]; // Expansive min. fragtion
		  in >> porosity_model_values[3]; // m
		  in >> porosity_model_values[4]; // I
		  in >> porosity_model_values[5]; // S^l_0
		  in >> porosity_model_values[6]; // beta
		  pcs_name_vector.push_back("SATURATION2");
		  pcs_name_vector.push_back("TEMPERATURE1");
		 break;
		case 4: // Chemical swelling model (constrained swelling)
		 in >> porosity_model_values[0]; // Initial porosity
		 in >> porosity_model_values[1]; // Specific surface[m^2/g]
		 in >> porosity_model_values[2]; // Expansive min. fragtion
		 in >> porosity_model_values[3]; // m
		 in >> porosity_model_values[4]; // I
		 in >> porosity_model_values[5]; // S^l_0
		 in >> porosity_model_values[6]; // beta
		 in >> porosity_model_values[7]; // n_min
		 pcs_name_vector.push_back("SATURATION2");
		 pcs_name_vector.push_back("TEMPERATURE1");
		 break;
 		case 5: // Chemical swelling model (free swelling, constant I)
		 in >> porosity_model_values[0]; // Initial porosity
		 in >> porosity_model_values[1]; // Specific surface[m^2/g]
		 in >> porosity_model_values[2]; // Expansive min. fragtion
		 in >> porosity_model_values[3]; // m
		 in >> porosity_model_values[4]; // I
		 in >> porosity_model_values[5]; // S^l_0
		 in >> porosity_model_values[6]; // beta
		 pcs_name_vector.push_back("SATURATION2");
		 pcs_name_vector.push_back("TEMPERATURE1");
		 break;
 		case 6: // Chemical swelling model (constrained swelling, constant I)
		 in >> porosity_model_values[0]; // Initial porosity
		 in >> porosity_model_values[1]; // Specific surface[m^2/g]
		 in >> porosity_model_values[2]; // Expansive min. fragtion
		 in >> porosity_model_values[3]; // m
		 in >> porosity_model_values[4]; // I
		 in >> porosity_model_values[5]; // S^l_0
		 in >> porosity_model_values[6]; // beta
		 in >> porosity_model_values[7]; // n_min
		 pcs_name_vector.push_back("SATURATION2");
		 pcs_name_vector.push_back("TEMPERATURE1");
		 break;
  		case 10: // Chemical swelling model (constrained swelling, constant I)
		 in >> porosity_model_values[0]; // Initial porosity
		 in >> m; // m
		 if (m>15) cout<<"Maximal number of solid phases is now limited to be 15!!!"<<endl;
		 for (int i=0; i<m+1; i++)
			in >> porosity_model_values[i+1]; // molar volume [l/mol]
		 break;
        case 11: //MB: read from file ToDo
		  in >> porosity_file;
		  break;
        default:
          cout << "Error in MMPRead: no valid porosity model" << endl;
		 break;
      }
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
//4..TORTUOSITY
//------------------------------------------------------------------------
   if(line_string.find("$TORTUOSITY")!=string::npos) { //subkeyword found
     in.str(GetLineFromFile1(mmp_file));
     in >> tortuosity_model;
      switch(tortuosity_model) {
        case 0: // n=f(x)
          break;
        case 1: // n=const
          in >> tortuosity_model_values[0];
          break;
        default:
          cout << "Error in MMPRead: no valid tortuosity model" << endl;
          break;
      }
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
//5..MOBILE_IMOBILE_MODEL
//------------------------------------------------------------------------
//To do as necessary

//------------------------------------------------------------------------
//6..LITHOLOGY_GRAIN_CLASS
//------------------------------------------------------------------------
//To do as necessary

//------------------------------------------------------------------------
//7..FLOWLINEARITY
//------------------------------------------------------------------------
  if(line_string.find("$FLOWLINEARITY")!=string::npos) { //subkeyword found
    in.str(GetLineFromFile1(mmp_file));
    in >> flowlinearity_model;
    switch(flowlinearity_model) {
      case 0: // k=f(x)
        break;
      case 1: // Read in alpha
        in >> flowlinearity_model_values[0];//Alpha
        break;
	  case 2: // For equivalent flow in trianglular elements
  	    in >> flowlinearity_model_values[0]; //Alpha
 	    in >> flowlinearity_model_values[1]; //Number of Fractures in Equivalent Medium
	    in >> flowlinearity_model_values[2]; //Reynolds Number above which non linear flow occurs.
	    pcs_name_vector.push_back("PRESSURE1");
		break;
      default:
        cout << "Error in MMPRead: no valid flow linearity model" << endl;
      break;
    }
    in.clear();
    continue;
  }
//------------------------------------------------------------------------
//8..SORPTION_MODEL
//------------------------------------------------------------------------
//To do as necessary

//------------------------------------------------------------------------
//9..STORAGE
//------------------------------------------------------------------------
   if(line_string.find("$STORAGE")!=string::npos) { //subkeyword found
     in.str(GetLineFromFile1(mmp_file));
      in >> storage_model;
      switch(storage_model) {
        case 0: // S=f(x)
		  in >> storage_model_values[0]; //Function of pressure defined by curve
          pcs_name_vector.push_back("PRESSURE1");
          break;
        case 1: // S=const
          in >> storage_model_values[0]; //Constant value in Pa
          break;
		case 2:
		  in >> storage_model_values[0];//S0
		  in >> storage_model_values[1];//increase
		  in >> storage_model_values[2];//sigma(z0)
		  in >> storage_model_values[3];//d_sigma/d_z
		  break;
		case 3:
		  in >> storage_model_values[0];//curve number (as real number)
		  in >> storage_model_values[1];//sigma(z0)
		  in >> storage_model_values[2];//_sigma/d_z
		  break;
		case 4:
		  in >> storage_model_values[0];//curve number (as real number)
		  in >> storage_model_values[1];//time collation
		  in >> storage_model_values[2];//solid density
		  in >> storage_model_values[3];//curve fitting factor, default 1
		  pcs_name_vector.push_back("PRESSURE1");
		  break;
		case 5://Storativity is a function of normal effective stress defined by curve, set up for KTB.
		  in >> storage_model_values[0];//curve number
		  in >> storage_model_values[1];//time collation
		  in >> storage_model_values[2];//Default storage value for material groups > 0
		  in >> storage_model_values[3];//Angular difference between Y direction and Sigma 1 
		  pcs_name_vector.push_back("PRESSURE1");
		  break;
		case 6://Storativity is a function of normal effective stress defined by curve and distance from borehole, set up for KTB.
		  in >> storage_model_values[0];//curve number
		  in >> storage_model_values[1];//time collation
		  in >> storage_model_values[2];//Default storage value for material groups > 0
		  in >> storage_model_values[3];//Angular difference between Y direction and Sigma 1 
		  in >> storage_model_values[4];//Borehole (x) coordinate
		  in >> storage_model_values[5];//Borehole (y) coordinate
		  in >> storage_model_values[6];//Borehole (z) coordinate
		  in >> storage_model_values[7];//Maximum thickness of shear zone
		  in >> storage_model_values[8];//Fracture density
		  pcs_name_vector.push_back("PRESSURE1");
		  break;
        default:
          cout << "Error in MMPRead: no valid storativity model" << endl;
          break;
      }
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
//10..CONDUCTIVITY_MODEL
//------------------------------------------------------------------------
    if(line_string.find("$CONDUCTIVITY_MODEL")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> conductivity_model;
      switch(conductivity_model) {
        case 0: // K=f(x)
          break;
        case 1: // K=const
          in >> conductivity;
          break;
        case 2: // Manning
          break;
        case 3: // Chezy
          break;
        default:
          cout << "Error in MMPRead: no valid conductivity model" << endl;
          break;
      }
      in.clear();
      continue;
    }

    if(line_string.find("$UNCONFINED")!=string::npos) { //subkeyword found
      unconfined_flow_group = 1;
      continue;
    }

//------------------------------------------------------------------------
//11..PERMEABILITY_TENSOR
//------------------------------------------------------------------------
      if(line_string.find("$PERMEABILITY_TENSOR")!=string::npos) { //subkeyword found
        in.str(GetLineFromFile1(mmp_file));
        in >> permeability_tensor_type_name;
        switch(permeability_tensor_type_name[0]) {
         case 'I': // isotropic
           permeability_tensor_type = 0;
           permeability_model = 1;
           in >> permeability_tensor[0];
		   permeability_tensor[1]=permeability_tensor[2]=permeability_tensor[0];//CMCD to pick up 2D and 3D Isotropic case.
          break;
         case 'O': // orthotropic
           permeability_tensor_type = 1;
           if(geo_dimension==0)
             cout << "Error in CMediumProperties::Read: no geometric dimension" << endl;
           if(geo_dimension==2){
             in >> permeability_tensor[0];
             in >> permeability_tensor[1];
           }
           if(geo_dimension==3){
             in >> permeability_tensor[0];
             in >> permeability_tensor[1];
             in >> permeability_tensor[2];
           }
           break;
         case 'A': // anisotropic
           permeability_tensor_type = 2;
           if(geo_dimension==0)
             cout << "Error in CMediumProperties::Read: no geometric dimension" << endl;
           if(geo_dimension==2){
             in >> permeability_tensor[0];
             in >> permeability_tensor[1];
             in >> permeability_tensor[2];
             in >> permeability_tensor[3];
           }
           if(geo_dimension==3){
             in >> permeability_tensor[0];
             in >> permeability_tensor[1];
             in >> permeability_tensor[2];
             in >> permeability_tensor[3];
             in >> permeability_tensor[4];
             in >> permeability_tensor[5];
             in >> permeability_tensor[6];
             in >> permeability_tensor[7];
             in >> permeability_tensor[8];
           }
           break;
		 case 'F': //SB: read from file
            permeability_model = 2; //OK
		    in >> permeability_file;
		   break;
         default:
           cout << "Error in MMPRead: no valid permeability tensor type" << endl;
           break;
       }
       in.clear();
       continue;
     }

//------------------------------------------------------------------------
//12. $PERMEABILITY_FUNCTION
//				(i)		_DEFORMATION
//				(ii)	_PRESSURE
//				(iii)	_SATURATION
//				(iv)	_STRESS
//				(v)		_VELOCITY
//				(vi)    _POROSITY				
//------------------------------------------------------------------------	
//------------------------------------------------------------------------
//12.1 PERMEABILITY_FUNCTION_DEFORMATION
//------------------------------------------------------------------------
    if(line_string.find("$PERMEABILITY_FUNCTION_DEFORMATION")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> permeability_model;
      switch(permeability_model) {
        case 0: // k=f(x)
          break;
        case 1: // k=const
          in >> permeability;
          break;
        default:
          cout << "Error in MMPRead: no valid permeability model" << endl;
          break;
      }
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
//12.2 PERMEABILITY_FUNCTION_PRESSURE
//------------------------------------------------------------------------ 
   if(line_string.find("$PERMEABILITY_FUNCTION_PRESSURE")!=string::npos) { //subkeyword found
     in.str(GetLineFromFile1(mmp_file));
     in >> permeability_pressure_model;
     switch(permeability_pressure_model) {
        case 0: // k=f(x)
          break;
        case 1: // k=const
          in >> permeability_pressure_model_values[0];
		  pcs_name_vector.push_back("PRESSURE1");
          break;
	    case 2: // Permebility is a function of effective stress
          in >> permeability_pressure_model_values[0];
		  in >> permeability_pressure_model_values[1];
		  in >> permeability_pressure_model_values[2];
		  in >> permeability_pressure_model_values[3];
		  pcs_name_vector.push_back("PRESSURE1");
          break;
	    case 3: // Permeability is a function of non linear flow
          in >> permeability_pressure_model_values[0];
		  in >> permeability_pressure_model_values[1];
		  in >> permeability_pressure_model_values[2];
		  in >> permeability_pressure_model_values[3];
		  pcs_name_vector.push_back("PRESSURE1");
          break;
	    case 4: // Function of effective stress from a curve
          in >> permeability_pressure_model_values[0];
		  in >> permeability_pressure_model_values[1];
		  in >> permeability_pressure_model_values[2];
		  pcs_name_vector.push_back("PRESSURE1");
          break;
	    case 5: // Function of overburden converted to effective stress and related to a curve.
          in >> permeability_pressure_model_values[0];
		  in >> permeability_pressure_model_values[1];
		  in >> permeability_pressure_model_values[2];
		  pcs_name_vector.push_back("PRESSURE1");
		  break;
	    case 6: // Normal effective stress calculated according to fracture orientation, related over a curve : KTB site
          in >> permeability_pressure_model_values[0];
		  in >> permeability_pressure_model_values[1];
		  in >> permeability_pressure_model_values[2];
		  in >> permeability_pressure_model_values[3];
		  pcs_name_vector.push_back("PRESSURE1");
          break;
	    case 7: // Normal effective stress calculated according to fracture orientation, related over a curve : KTB site, and distance to the borehole.
          in >> permeability_pressure_model_values[0];
		  in >> permeability_pressure_model_values[1];
		  in >> permeability_pressure_model_values[2];
		  in >> permeability_pressure_model_values[3];
		  in >> permeability_pressure_model_values[4];
		  in >> permeability_pressure_model_values[5];
		  in >> permeability_pressure_model_values[6];
		  in >> permeability_pressure_model_values[7];
		  in >> permeability_pressure_model_values[8];
		  pcs_name_vector.push_back("PRESSURE1");
          break;
	    case 8: // Effective stress related to depth and curve, Urach
          in >> permeability_pressure_model_values[0];
		  in >> permeability_pressure_model_values[1];
		  pcs_name_vector.push_back("PRESSURE1");
          break;
	    case 9: // Effective stress related to depth and curve, and to temperature, Urach
          in >> permeability_pressure_model_values[0];
		  in >> permeability_pressure_model_values[1];
		  in >> permeability_pressure_model_values[2];
		  in >> permeability_pressure_model_values[3];
		  in >> permeability_pressure_model_values[4];
		  pcs_name_vector.push_back("PRESSURE1");
		  pcs_name_vector.push_back("TEMPERATURE1");
          break;
		default:
          cout << "Error in MMPRead: no valid permeability model" << endl;
          break;
      }
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
//12.3 PERMEABILITY_FUNCTION_SATURATION
//------------------------------------------------------------------------ 
    //....................................................................
    if(line_string.find("$PERMEABILITY_SATURATION")!=string::npos) { //subkeyword found
     int no_fluid_phases =(int)mfp_vector.size();
       for(i=0;i<no_fluid_phases;i++){
         in.str(GetLineFromFile1(mmp_file));
         in >> permeability_saturation_model[i];
         switch(permeability_saturation_model[i]) {
          case 0: // k=f(x)
            in >> permeability_saturation_model_values[i];
            break;
          case 21: // k=1-S_eff
            in >> saturation_res[i];
            in >> saturation_max[i];
            break;
          case 3: // power function
            in >> saturation_res[i];
            in >> saturation_max[i];
            in >> saturation_exp[i];
            break;
          case 4: // van Genuchten
            in >> saturation_res[i];
            in >> saturation_max[i];
            in >> saturation_exp[i];
            break;
          case 14: // van Genuchten for liquid MX 03.2005 paper swelling pressure
            in >> saturation_res[i];
            in >> saturation_max[i];
            in >> saturation_exp[i];
            break;
          case 15: // van Genuchten for gas MX 03.2005 paper swelling pressure
            in >> saturation_res[i];
            in >> saturation_max[i];
            in >> saturation_exp[i];
            break;
          default:
            cout << "Error in MMPRead: no valid permeability saturation model" << endl;
            break;
        }
        in.clear();
      }
      continue;
    }
/* See case 3 above   CMCD 11/2004...who put this in?, please come and see me so we can put it in together.
    if(line_string.find("$PERMEABILITY_FUNCTION_SATURATION")!=string::npos) { //subkeyword found
     int no_fluid_phases =(int)mfp_vector.size();
     for(i=0;i<no_fluid_phases;i++){
       *mmp_file >> permeability_saturation_model[i];
       *mmp_file >> delimiter;
        switch(permeability_saturation_model[i]) {
          case 0: // k=f(x)
            break;
          case 3: // power function
           *mmp_file >> saturation_res[i];
           *mmp_file >> saturation_max[i];
           *mmp_file >> saturation_exp[i];
            break;
          default:
            cout << "Error in MMPRead: no valid permeability saturation model" << endl;
            break;
        }
        mmp_file->ignore(MAX_ZEILE,'\n');
      }
      continue;
    }*/
//------------------------------------------------------------------------
//12.4 PERMEABILITY_FUNCTION_STRESS
//------------------------------------------------------------------------ 
   if(line_string.find("$PERMEABILITY_FUNCTION_STRESS")!=string::npos) { //subkeyword found
     in.str(GetLineFromFile1(mmp_file));
     in >> permeability_model;
      switch(permeability_model) {
        case 0: // k=f(x)
          break;
        case 1: // k=const
          in >> permeability;
          break;
        default:
          cout << "Error in MMPRead: no valid permeability model" << endl;
          break;
      }
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
//12.5 PERMEABILITY_FUNCTION_VELOCITY
//------------------------------------------------------------------------ 
   if(line_string.find("$PERMEABILITY_FUNCTION_STRESS")!=string::npos) { //subkeyword found
     in.str(GetLineFromFile1(mmp_file));
     in >> permeability_model;
      switch(permeability_model) {
        case 0: // k=f(x)
          break;
        case 1: // k=const
          in >> permeability;
          break;
        default:
          cout << "Error in MMPRead: no valid permeability model" << endl;
          break;
      }
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
//12.6 PERMEABILITY_FUNCTION_POROSITY
//------------------------------------------------------------------------
	
	if(line_string.find("$PERMEABILITY_FUNCTION_POROSITY")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> permeability_porosity_model;
      switch(permeability_porosity_model) {
        case 0: // k=f(x)
          break;
        case 1: // k=const
          in >> permeability_porosity_model_values[0];
          mmp_file->ignore(MAX_ZEILE,'\n');
          break;
		case 2:// Model from Ming Lian
          in >> permeability_porosity_model_values[0];
		  in >> permeability_porosity_model_values[1];
          break;

        default:
          cout << "Error in MMPRead: no valid permeability model" << endl;
          break;
      }
      in.clear();
      continue;
    }

    //....................................................................
    if(line_string.find("$CAPILLARY_PRESSURE")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> capillary_pressure_model;
      switch(capillary_pressure_model) {
        case 0: // k=f(x)
             in >> capillary_pressure_model_values[0];
          break;
        case 1: // const
             in >> capillary_pressure;
          break;
		case 4: // van Genuchten
			 in >> capillary_pressure_model_values[0];
        default:
          cout << "Error in MMPRead: no valid permeability saturation model" << endl;
          break;
      }
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
//14 MASSDISPERSION_
//			(1) LONGITUDINAL
//			(2) TRANSVERSE
//------------------------------------------------------------------------ 
    if(line_string.find("$MASS_DISPERSION")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> mass_dispersion_model;
      switch(mass_dispersion_model) {
        case 0: // f(x)
          break;
        case 1: // Constant value
         in >> mass_dispersion_longitudinal;
         in >> mass_dispersion_transverse;
          break;
        default:
          cout << "Error in CMediumProperties::Read: no valid mass dispersion model" << endl;
          break;
      }
      in.clear();
      continue;
	}
//------------------------------------------------------------------------
//15 HEATDISPERSION
//			(1) LONGTIDUINAL
//			(2) TRANSVERSE
//------------------------------------------------------------------------ 
    if(line_string.find("$HEAT_DISPERSION")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> heat_dispersion_model;
      switch(heat_dispersion_model) {
        case 0: // f(x)
          break;
        case 1: // Constant value
          in >> heat_dispersion_longitudinal;
          in >> heat_dispersion_transverse;
          break;
        default:
          cout << "Error in CMediumProperties::Read: no valid heat dispersion model" << endl;
          break;
      }
      in.clear();
      continue;
	}
//------------------------------------------------------------------------
//16. Surface water
//------------------------------------------------------------------------
    if(line_string.find("$MANNING_COEFFICIENT")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> friction_coefficient;
      friction_model = 1;
      in.clear();
      continue;
    }

    if(line_string.find("$CHEZY_COEFFICIENT")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> friction_coefficient;
      friction_model = 2;
      in.clear();
      continue;
    }
  
   if(line_string.find("$DARCY_WEISBACH_COEFFICIENT")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> friction_coefficient;
      friction_model = 3;
      in.clear();
      continue;
    }
    if(line_string.find("$DIFFUSION")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
	  in >> heat_diffusion_model;
      in.clear();
      continue;
    }

//------------------------------------------------------------------------
//19 ELECTRIC_CONDUCTIVITY
//------------------------------------------------------------------------ 
//------------------------------------------------------------------------
//20 UNCONFINED_FLOW_GROUP
//------------------------------------------------------------------------ 
//------------------------------------------------------------------------
//21 FLUID_EXCHANGE_WITH_OTHER_CONTINUA
//------------------------------------------------------------------------ 

//------------------------------------------------------------------------
//11..PERMEABILITY_DISTRIBUTION
//------------------------------------------------------------------------
    if(line_string.find("$PERMEABILITY_DISTRIBUTION")!=string::npos) { //subkeyword found
      in.str(GetLineFromFile1(mmp_file));
      in >> permeability_file;
      permeability_model = 2;
      in.clear();
      continue;
    }
//------------------------------------------------------------------------
  }
  return position;
}


/**************************************************************************
FEMLib-Method: MMPWrite
Task: master write functionn
Programing:
02/2004 OK Implementation
last modification:
**************************************************************************/
void MMPWrite(string file_base_name)
//void MATWriteMediumProperties(fstream *mp_file)
{
  CMediumProperties *m_mat_mp = NULL;
  string sub_line;
  string line_string;
  //========================================================================
  // File handling
  string mp_file_name = file_base_name + MMP_FILE_EXTENSION;
  fstream mp_file (mp_file_name.c_str(),ios::trunc|ios::out);
  mp_file.setf(ios::scientific,ios::floatfield);
  mp_file.precision(12);
  if (!mp_file.good()) return;
  mp_file << "GeoSys-MMP: Material Medium Properties ------------------------------------------------\n";
  //========================================================================
  // MAT-MP list
  int no_mat =(int)mmp_vector.size();
  int i;
  for(i=0;i<no_mat;i++){
    m_mat_mp = mmp_vector[i];
    m_mat_mp->Write(&mp_file);
  }
  mp_file << "#STOP";
  mp_file.close();
}

/**************************************************************************
FEMLib-Method: CMediumProperties::Write
Task: read functionn
Programing:
02/2004 OK Template
01/2005 OK new formats
05/2005 OK CAPILLARY_PRESSURE,
05/2005 OK Surface flow parameter
06/2005 MB file names
10/2005 OK/YD bugfix model 4
10/2005 OK geo_name_vector
ToDo: MB CONDUCTIVITY_MODEL, PERMEABILITY_SATURATION
**************************************************************************/
void CMediumProperties::Write(fstream* mmp_file)
{
  //KEYWORD
  *mmp_file  << "#MEDIUM_PROPERTIES\n";
  //--------------------------------------------------------------------
  //NAME
if(name.length()>0){
  *mmp_file << " $NAME" << endl;
  *mmp_file << "  ";
  *mmp_file << name << endl;
}
  //--------------------------------------------------------------------
  //GEO_TYPE
if(geo_type_name.compare("DOMAIN")==0){ //OK
  *mmp_file << " $GEO_TYPE" << endl;
  *mmp_file << "  DOMAIN" << endl;
}
else if((int)geo_name_vector.size()>0){ //OK
  *mmp_file << " $GEO_TYPE" << endl;
  for(int i=0;i<(int)geo_name_vector.size();i++){
    *mmp_file << "  ";
    *mmp_file << geo_type_name;
    *mmp_file << " ";
    *mmp_file << geo_name_vector[i] << endl;
  }
}
  //--------------------------------------------------------------------
  //DIMENSION
  *mmp_file << " $GEOMETRY_DIMENSION" << endl;
  *mmp_file << "  ";
  *mmp_file << geo_dimension << endl;
  *mmp_file << " $GEOMETRY_AREA" << endl;
  *mmp_file << "  ";
  *mmp_file << geo_area << endl;
  //--------------------------------------------------------------------
  //PROPERTIES
  //....................................................................
  //POROSITY
if(porosity_model>-1){
  *mmp_file << " $POROSITY" << endl;
  *mmp_file << "  ";
  *mmp_file << porosity_model << " "; 
  switch (porosity_model) {
    case 1:
      *mmp_file << porosity_model_values[0] << endl;
      break;
    case 11: //MB ToDo
      *mmp_file << porosity_file << endl; //MB
      break;
  }
}
  //....................................................................
  //TORTUOSITY //OK4104
if(tortuosity_model>-1){
  *mmp_file << " $TORTUOSITY" << endl;
  *mmp_file << "  ";
  *mmp_file << tortuosity_model << " "; 
  switch (tortuosity_model) {
    case 1:
      *mmp_file << tortuosity_model_values[0] << endl; 
      break;
  }
}
  //....................................................................
  //CONDUCTIVITY //MB ToDo
if(conductivity_model>-1){
  *mmp_file << " $CONDUCTIVITY_MODEL" << endl;
  *mmp_file << "  ";
  *mmp_file << conductivity_model << " "; 
  switch (conductivity_model) {
    case 1:
      *mmp_file << conductivity; 
      break;
  }
  *mmp_file << endl; 
}
  //....................................................................
  //STORAGE
if(storage_model>-1){
  *mmp_file << " $STORAGE" << endl;
  switch (storage_model) {
    case 1:
      *mmp_file << "  " << storage_model << " " << storage_model_values[0] << endl;
      break;
  }
}
  //....................................................................
  //PERMEABILITY_TENSOR 
if(permeability_tensor_type>-1){
  *mmp_file << " $PERMEABILITY_TENSOR" << endl;
  switch (permeability_tensor_type) {
    case 0:
      *mmp_file << "  " << "ISOTROPIC" << " " << permeability_tensor[0] << endl;
      break;
    case 3: //MB for heterogeneous fields //OK
      *mmp_file << "  " << "FILE" << " " << permeability_file << endl;
      break;
  }
}
  //....................................................................
  //PERMEABILITY_DISTRIBUTION
if(permeability_model==2){
  *mmp_file << " $PERMEABILITY_DISTRIBUTION" << endl;
  *mmp_file << "  " << permeability_file << endl;
}
  //....................................................................
  //PERMEABILITY 
if(permeability_saturation_model[0]>-1){
  *mmp_file << " $PERMEABILITY_SATURATION" << endl;
  for(int i=0;i<(int)mfp_vector.size();i++){
    *mmp_file << "  ";
    *mmp_file << permeability_saturation_model[0] << " "; 
    switch (permeability_saturation_model[0]) {
      case 0:
        *mmp_file << (int)permeability_saturation_model_values[0] << endl;
        break;
      case 4: //VG
        *mmp_file << saturation_res[0] << " ";
        *mmp_file << saturation_max[0] << " ";
        *mmp_file << saturation_exp[0] << endl;
        break;
    }
  }
}
  //....................................................................
  //CAPILLARY PRESSURE
if(capillary_pressure_model>-1){
  *mmp_file << " $CAPILLARY_PRESSURE" << endl;
  *mmp_file << "  ";
  *mmp_file << capillary_pressure_model << " "; 
  switch (capillary_pressure_model) {
    case 0:
      *mmp_file << (int)capillary_pressure_model_values[0] << endl;
      break;
    case 4: //VG
      *mmp_file << capillary_pressure_model_values[0] << endl;
      break;
    case 9: // HydroSphere
      *mmp_file << saturation_res[0] << " ";
      *mmp_file << capillary_pressure_model_values[0] << " ";
      *mmp_file << capillary_pressure_model_values[1] << endl;
      break;
  }
}
  //....................................................................
  //HEAT DISPERSION 
if(heat_dispersion_model>-1){
  *mmp_file << " $HEAT_DISPERSION" << endl;
  switch (heat_dispersion_model) {
    case 1:
      *mmp_file << "  " << heat_dispersion_model << " " << heat_dispersion_longitudinal <<" "<<heat_dispersion_transverse<<endl; //CMCD permeability
      break;
  }
}
  //....................................................................
  //Surface flow
if(friction_model==3){
   *mmp_file << " $DARCY_WEISBACH_COEFFICIENT" << endl; //OK
   *mmp_file << "  " << friction_coefficient << endl;
}
if(friction_model==2){
   *mmp_file << " $CHEZY_COEFFICIENT" << endl; //OK
   *mmp_file << "  " << friction_coefficient << endl;
}
if(friction_model==1){
   *mmp_file << " $MANNING_COEFFICIENT" << endl; //OK
   *mmp_file << "  " << friction_coefficient << endl;
}
  //----------------------------------------------------------------------
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
03/2004 OK Implementation
last modification:
**************************************************************************/
void MMPWriteTecplot(string msh_name)
{
  CMediumProperties *m_mmp = NULL;
  int i;
  int no_mat =(int)mmp_vector.size();
  for(i=0;i<no_mat;i++){
    m_mmp = mmp_vector[i];
    m_mmp->WriteTecplot(msh_name);
  }
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
01/2005 OK Implementation
09/2005 OK MSH
10/2005 OK OO-ELE
last modification:
**************************************************************************/
void CMediumProperties::WriteTecplot(string msh_name)
{
  long i,j;
  string element_type;
  //----------------------------------------------------------------------
  // GSP
  CGSProject* m_gsp = NULL;
  m_gsp = GSPGetMember("msh");
  if(!m_gsp){
#ifdef MFC
    AfxMessageBox("Error: No MSH member");
#endif
    return;
  }
  //--------------------------------------------------------------------
  // file handling
  string mat_file_name = m_gsp->path + "MAT_" + name + TEC_FILE_EXTENSION;
  fstream mat_file (mat_file_name.data(),ios::trunc|ios::out);
  mat_file.setf(ios::scientific,ios::floatfield);
  mat_file.precision(12);
  //--------------------------------------------------------------------
  // MSH
  CFEMesh* m_msh = NULL;
  CNode* m_nod = NULL;
  CElem* m_ele = NULL;
  m_msh = FEMGet(msh_name);
  if(!m_msh)
    return;
  //--------------------------------------------------------------------
  if (!mat_file.good()) return;
  mat_file.seekg(0L,ios::beg);
  //--------------------------------------------------------------------
  j=0;
  for(i=0;i<(long)m_msh->ele_vector.size();i++){
    m_ele = m_msh->ele_vector[i];
    if(m_ele->GetPatchIndex()==number) {
      switch(m_ele->GetElementType()){
        case 1:
            j++; 
            break;
          case 2:
            j++; 
            break;
          case 3:
            j++; 
            break;
          case 4:
            j++; 
            break;
          case 5:
            j++; 
            break;
          case 6:
          j++; 
          break;
      }
    }
  }
  long no_elements = j-1;
  //--------------------------------------------------------------------
  mat_file << "VARIABLES = X,Y,Z,MAT" << endl;
  long no_nodes = (long)m_msh->nod_vector.size();
  mat_file << "ZONE T = " << name << ", " \
           << "N = " << no_nodes << ", " \
           << "E = " << no_elements << ", " \
           << "F = FEPOINT" << ", " << "ET = BRICK" << endl;
  for(i=0;i<no_nodes;i++) {
    m_nod = m_msh->nod_vector[i];
    mat_file \
      << m_nod->X() << " " << m_nod->Y() << " " << m_nod->Z() << " " \
      << number << endl;
  }
  j=0;
  for(i=0;i<(long)m_msh->ele_vector.size();i++){
    m_ele = m_msh->ele_vector[i];
    if(m_ele->GetPatchIndex()==number) {
      switch(m_ele->GetElementType()){
        case 1:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[0]+1 << endl;
            j++; 
            element_type = "ET = QUADRILATERAL";
            break;
          case 2:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << " " << m_ele->nodes_index[3]+1 << endl;
            j++; 
            element_type = "ET = QUADRILATERAL";
            break;
          case 3:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << " " << m_ele->nodes_index[3]+1 << " " \
              << m_ele->nodes_index[4]+1 << " " << m_ele->nodes_index[5]+1 << " " << m_ele->nodes_index[6]+1 << " " << m_ele->nodes_index[7]+1 << endl;
            j++; 
            element_type = "ET = BRICK";
            break;
          case 4:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << endl;
            j++; 
            element_type = "ET = TRIANGLE";
            break;
          case 5:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << " " << m_ele->nodes_index[3]+1 << endl;
            j++; 
            element_type = "ET = TETRAHEDRON";
            break;
          case 6:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << " " \
              << m_ele->nodes_index[3]+1 << " " << m_ele->nodes_index[3]+1 << " " << m_ele->nodes_index[4]+1 << " " << m_ele->nodes_index[5]+1 << endl;
          j++; 
          element_type = "ET = BRICK";
          break;
      }
    }
  }
}


////////////////////////////////////////////////////////////////////////////
// Access functions
////////////////////////////////////////////////////////////////////////////

/**************************************************************************
FEMLib-Method: CMediumProperties
Task: get instance by name
Programing:
02/2004 OK Implementation
last modification:
**************************************************************************/
CMediumProperties* MMPGet(string mat_name)
{
  CMediumProperties *m_mat = NULL;
  int no_mat =(int)mmp_vector.size();
  int i;
  for(i=0;i<no_mat;i++){
    m_mat = mmp_vector[i];
    if(mat_name.compare(m_mat->name)==0)
      return m_mat;
  }
  return NULL;
}

/**************************************************************************
FEMLib-Method: CMediumProperties
Task: get instance by name
Programing:
02/2004 OK Implementation
last modification:
**************************************************************************/
CMediumProperties* CMediumProperties::GetByGroupNumber(int group_number)
{
  CMediumProperties *m_mat = NULL;
  int no_mat =(int)mmp_vector.size();
  int i;
  for(i=0;i<no_mat;i++){
    m_mat = mmp_vector[i];
    if(m_mat->number==group_number)
      return m_mat;
  }
  return NULL;
}


////////////////////////////////////////////////////////////////////////////
// Properties functions
////////////////////////////////////////////////////////////////////////////

/**************************************************************************
FEMLib-Method:
Task: 
Programing:
03/1997 CT Erste Version
09/1998 CT Kurven ueber Schluesselwort #CURVES
08/1999 CT Erweiterung auf n-Phasen begonnen
09/2002 OK case 13
05/2003 MX case 14
08/2004 OK Template for MMP implementation
05/2005 MX Case 14, 15 
05/2005 OK MSH
last modification:
ToDo: GetSoilRelPermSatu
**************************************************************************/
double CMediumProperties::PermeabilitySaturationFunction(long number,double*gp,double theta,\
                                                         int phase)
{
  int no_fluid_phases =(int)mfp_vector.size();
  if(!m_pcs){
    cout << "CMediumProperties::PermeabilitySaturationFunction: no PCS data" << endl;
    return 1.0;
  }
  if(!(m_pcs->pcs_type_name.compare("RICHARDS_FLOW")==0)&&no_fluid_phases!=2)
     return 1.0;
  static int nidx0,nidx1;
  double saturation,saturation_eff;
  int gueltig;
  //---------------------------------------------------------------------
  if(mode==2){ // Given value
    saturation = argument;
  }
else{
  string pcs_name_this = "SATURATION";
  char phase_char[1];
  sprintf(phase_char,"%i",phase+1);
  pcs_name_this.append(phase_char);
/* //WW DEelete these
  if(m_pcs->m_msh){
    if(gp==NULL){ // NOD value
      nidx0 = GetNodeValueIndex(pcs_name_this);
      nidx1 = GetNodeValueIndex(pcs_name_this)+1;
      saturation = (1.-theta)*GetNodeValue(number,nidx0) \
                         + theta*GetNodeValue(number,nidx1);
    }
    else // ELE GP value
      saturation = m_pcs->GetELEValue(number,gp,theta,pcs_name_this);
  }

  else{
*/
  nidx0 = PCSGetNODValueIndex(pcs_name_this,0);
  nidx1 = PCSGetNODValueIndex(pcs_name_this,1);
  if(mode==0){ // Gauss point values
    saturation = (1.-theta)*InterpolValue(number,nidx0,gp[0],gp[1],gp[2]) \
               + theta*InterpolValue(number,nidx1,gp[0],gp[1],gp[2]);
  }
  else{ // Node values
    saturation = (1.-theta)*GetNodeVal(number,nidx0) \
               + theta*GetNodeVal(number,nidx1);
  }
 // }
}
  //----------------------------------------------------------------------
  switch(permeability_saturation_model[phase]){ 
    case 0:  // k = f(x) user-defined function
      permeability_saturation = GetCurveValue((int)permeability_saturation_model_values[phase],0,saturation,&gueltig);  //CMCD permeability_saturation_model_values[phase]
     break;
    case 1:  // linear function
      break;
    case 21:  // linear function from ! liquid saturation
        if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;
        saturation_eff = (saturation - saturation_res[phase]) / (saturation_max[phase] - saturation_res[phase]);
        permeability_saturation = saturation_eff;
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    case 3:  // Nichtlineare Permeabilitaets-Saettigungs-Beziehung
        saturation_eff = (saturation-saturation_res[phase])/(saturation_max[phase]-saturation_res[phase]);
        saturation_eff = MRange(0.,saturation_eff,1.);
        permeability_saturation = pow(saturation_eff,saturation_exp[phase]);
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    case 4:  // Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 Page 894 Equation 19 (Burdine's Equation)
        if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;  /* Mehr als Vollsaettigung mit Wasser */
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */
        //
        saturation_eff = (saturation - saturation_res[phase]) / (saturation_max[phase] - saturation_res[phase]);
        //SB relperm[1] = pow(s_eff, 2.) * pow((1 - pow((1 - pow(s_eff, 1. / m)), m)),2.0); //SB:todo added ^2N1
        //permeability_saturation = pow(saturation_eff,2.) 
        //                        * (1.-pow((1.- pow(saturation_eff,1./saturation_exp[phase])),saturation_exp[phase]));	
        // YD Advances in Water Resource 19 (1995) 25-38 
        permeability_saturation = pow(saturation_eff,0.5) \
           * pow(1.-pow(1-pow(saturation_eff,1./saturation_exp[phase]),saturation_exp[phase]),2.0);
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    case 5:  // Haverkamp Problem: aus SOIL SCI. SOC. AM. J., VOL. 41, 1977, Pages 285ff 
      break;
    case 6:  // Brooks/Corey: Alte Variante nach Helmig 
      break;
    case 7:  // Sprungfunktion 
      break;
    case 8:  // Brooks/Corey: Journal of the Irrigation and Drainage Division June/1966 Pages 61ff
      break;
    case 11: // Drei Phasen ueber Kurven
      break;
    case 12: // Drei Phasen nichtlineare Permeabilitaets-Saettigungs-Beziehung
      break;
    case 13: //OK Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 (Mualems Equation)
      break;
    case 14: //MX Van Genuchten: Wasser aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 (Mualems Equation)
		if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;  /* Mehr als Vollsaettigung mit Wasser */
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */
        //
        saturation_eff = (saturation - saturation_res[phase]) / (saturation_max[phase] - saturation_res[phase]);
        //MX relperm[1] = pow(s_eff, 0.5) * pow((1 - pow((1 - pow(s_eff, 1. / m)), m)),2.0); 
		permeability_saturation = pow(saturation_eff,0.5) \
                                * pow((1.-pow((1.- pow(saturation_eff,1./saturation_exp[phase])),saturation_exp[phase])),2.0);
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    case 15: //MX Van Genuchten: Gas aus Water Resour. Res. VOL. 23, pp2197-2206 1987 
		if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;  /* Mehr als Vollsaettigung mit Wasser */
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */
        //
        saturation_eff = (saturation - saturation_res[phase]) / (saturation_max[phase] - saturation_res[phase]);
        //MX relperm[1] = pow((1-s_eff), 0.5) * pow((1 - pow(s_eff, 1. / m)),2m); 
		permeability_saturation = pow((1-saturation_eff),0.5) \
                                * pow((1.-pow(saturation_eff,1./saturation_exp[phase])),2.0*saturation_exp[phase]);
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    default:
      cout << "Error in CFluidProperties::PermeabilitySaturationFunction: no valid material model" << endl;
      break;
  }
  return permeability_saturation;
}



/**************************************************************************
FEMLib-Method:
Task: 
Programing:
03/1997 CT Erste Version
09/1998 CT Kurven ueber Schluesselwort #CURVES
08/1999 CT Erweiterung auf n-Phasen begonnen
09/2002 OK case 13
05/2003 MX case 14
08/2004 OK Template for MMP implementation
05/2005 MX Case 14, 15 
05/2005 OK MSH
08/2005 WW Remove interpolation
last modification:
ToDo: GetSoilRelPermSatu
**************************************************************************/
double CMediumProperties::PermeabilitySaturationFunction(const double Saturation,\
                                                         int phase)
{
  int no_fluid_phases =(int)mfp_vector.size();
  if(!m_pcs){
    cout << "CMediumProperties::PermeabilitySaturationFunction: no PCS data" << endl;
    return 1.0;
  }
  if(!(m_pcs->pcs_type_name.compare("RICHARDS_FLOW")==0)&&no_fluid_phases!=2)
     return 1.0;
  double saturation, saturation_eff;
  int gueltig;
  saturation = Saturation;
  //----------------------------------------------------------------------
  switch(permeability_saturation_model[phase]){ 
    case 0:  // k = f(x) user-defined function
      permeability_saturation = GetCurveValue((int)permeability_saturation_model_values[phase],0,saturation,&gueltig);  //CMCD permeability_saturation_model_values[phase]
     break;
    case 1:  // linear function
      break;
    case 21:  // linear function from ! liquid saturation
        if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;
        saturation_eff = (saturation - saturation_res[phase]) / (saturation_max[phase] - saturation_res[phase]);
        permeability_saturation = saturation_eff;
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    case 3:  // Nichtlineare Permeabilitaets-Saettigungs-Beziehung
        saturation_eff = (saturation-saturation_res[phase])/(saturation_max[phase]-saturation_res[phase]);
        saturation_eff = MRange(0.,saturation_eff,1.);
        permeability_saturation = pow(saturation_eff,saturation_exp[phase]);
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    case 4:  // Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 Page 894 Equation 19 (Burdine's Equation)
        if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;  /* Mehr als Vollsaettigung mit Wasser */
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */
        //
        saturation_eff = (saturation - saturation_res[phase]) / (saturation_max[phase] - saturation_res[phase]);
        //SB relperm[1] = pow(s_eff, 2.) * pow((1 - pow((1 - pow(s_eff, 1. / m)), m)),2.0); //SB:todo added ^2N1
		//permeability_saturation = pow(saturation_eff,2.) 
        //                        * (1.-pow((1.- pow(saturation_eff,1./saturation_exp[phase])),saturation_exp[phase]));
		// YD Advances in Water Resource 19 (1995) 25-38 
        permeability_saturation = pow(saturation_eff,0.5) \
                     * pow(1.-pow(1-pow(saturation_eff,1./saturation_exp[phase]),saturation_exp[phase]),2.0);
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    case 5:  // Haverkamp Problem: aus SOIL SCI. SOC. AM. J., VOL. 41, 1977, Pages 285ff 
      break;
    case 6:  // Brooks/Corey: Alte Variante nach Helmig 
      break;
    case 7:  // Sprungfunktion 
      break;
    case 8:  // Brooks/Corey: Journal of the Irrigation and Drainage Division June/1966 Pages 61ff
      break;
    case 11: // Drei Phasen ueber Kurven
      break;
    case 12: // Drei Phasen nichtlineare Permeabilitaets-Saettigungs-Beziehung
      break;
    case 13: //OK Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 (Mualems Equation)
      break;
    case 14: //MX Van Genuchten: Wasser aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 (Mualems Equation)
		if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;  /* Mehr als Vollsaettigung mit Wasser */
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */
        //
        saturation_eff = (saturation - saturation_res[phase]) / (saturation_max[phase] - saturation_res[phase]);
        //MX relperm[1] = pow(s_eff, 0.5) * pow((1 - pow((1 - pow(s_eff, 1. / m)), m)),2.0); 
		permeability_saturation = pow(saturation_eff,0.5) \
                                * pow((1.-pow((1.- pow(saturation_eff,1./saturation_exp[phase])),saturation_exp[phase])),2.0);
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    case 15: //MX Van Genuchten: Gas aus Water Resour. Res. VOL. 23, pp2197-2206 1987 
		if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;  /* Mehr als Vollsaettigung mit Wasser */
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */
        //
        saturation_eff = (saturation - saturation_res[phase]) / (saturation_max[phase] - saturation_res[phase]);
        //MX relperm[1] = pow((1-s_eff), 0.5) * pow((1 - pow(s_eff, 1. / m)),2m); 
		permeability_saturation = pow((1-saturation_eff),0.5) \
                                * pow((1.-pow(saturation_eff,1./saturation_exp[phase])),2.0*saturation_exp[phase]);
        permeability_saturation = MRange(0.,permeability_saturation,1.);
      break;
    default:
      cout << "Error in CFluidProperties::PermeabilitySaturationFunction: no valid material model" << endl;
      break;
  }
  return permeability_saturation;
}


/**************************************************************************
FEMLib-Method:
Task: Calculate heat capacity of porous medium
c rho = (c rho)^f + (c rho)^s
Programing:
04/2002 OK Implementation
08/2004 OK MMP implementation 
03/2005 WW Case of no fluids 
10/2005 YD/OK: general concept for heat capacity
**************************************************************************/

double CMediumProperties::HeatCapacity(long number, double*gp,double theta,
                                       CFiniteElementStd* assem)
{
  CSolidProperties *m_msp = NULL;
  int heat_capcity_model = 1;
  double heat_capacity_fluids,specific_heat_capacity_solid; 
  double density_solid;
  double porosity;
  int group;
  int nphase = (int)mfp_vector.size();
  // ??? 
  bool FLOW = false; //WW
  for(int ii=0;ii<(int)pcs_vector.size();ii++){
     if(pcs_vector[ii]->pcs_type_name.find("FLOW")!=string::npos)
       FLOW = true;
     if(pcs_vector[ii]->pcs_type_name.find("RICHARDS")!=string::npos)
       nphase = 3;	 
  }
  //----------------------------------------------------------------------
  switch(heat_capcity_model){
    //....................................................................
    case 0:  // f(x) user-defined function
      break;
    //....................................................................
    case 1:  // const
      if(fem_msh_vector.size()>0)
        group = m_pcs->m_msh->ele_vector[number]->GetPatchIndex();
	    else
        group = ElGetElementGroupNumber(number);
      m_msp = msp_vector[group];
      specific_heat_capacity_solid = m_msp->Heat_Capacity();
      density_solid = fabs(m_msp->Density());
      if(FLOW)
      {
        porosity = Porosity(number,gp,theta);
        heat_capacity_fluids = porosity*MFPCalcFluidsHeatCapacity(number,gp,theta, assem);
      }
      else
      {   
        heat_capacity_fluids = 0.0;
        porosity = 0.0;
      }
      heat_capacity = heat_capacity_fluids \
                       + (1.-porosity) * specific_heat_capacity_solid * density_solid;
      break;
    /*case 2:  //boiling model for YD
      //YD/OK: n c rho = n S^g c^g rho^g + n S^l c^l rho^l + (1-n) c^s rho^s
      T1 = interpolate(NodalVal1);
  	  if(heat_phase_change){
        T0 = interpolate(NodalVal0);
        if(fabs(T1-T0)<1.0e-8) T1 +=1.0e-8;
        H0 = interpolate(NodalVal2);
        H1 = interpolate(NodalVal3);
		    heat_capacity = (H1-H0)/(T1-T0);
		  }
		  else heat_capacity = SolidProp->Heat_Capacity(-T1, 0.0);
      break;
    case 3:  // D_THM1
      T1 = interpolate(NodalVal1);
      poro = MediaProp->Porosity(Index,unit,pcs->m_num->ls_theta);
      Sw = interpolate(NodalVal_Sat);
      val = SolidProp->Heat_Capacity(T1)*fabs(SolidProp->Density())+ \
      Sw*poro*MFPCalcFluidsHeatCapacity(Index,unit,pcs->m_num->ls_theta, this);
      if(FluidProp) val_l = FluidProp->SpecificHeatCapacity(this)
      val_s = SolidProp->Heat_Capacity(0.0)/time_unit_factor;
      val_g = GasProp->SpecificHeatCapacity(this)/time_unit_factor;; 
      val = val_l + val_s;
      break;*/
    //....................................................................
    default:
      cout << "Error in CMediumProperties::HeatCapacity: no valid material model" << endl;
      break;
    //....................................................................
  }
  return heat_capacity;
}

/**************************************************************************
FEMLib-Method:
Task: 
Programing:
04/2002 OK Implementation
08/2004 MB case 5 tet, case 6 prism
09/2004 OK MMP implementation 
03/2005 WW Case of no fluids and symmetry
11/2005 CMCD
last modification:
ToDo:
**************************************************************************/
double* CMediumProperties::HeatConductivityTensor()
{
  int i, dimen;
  CSolidProperties *m_msp = NULL;
  double heat_conductivity_fluids;
  double porosity =  this->porosity;
  bool FLOW = false; //WW
//  int heat_capacity_model = 0;
  CFluidProperties *m_mfp;
 // long group = Fem_Ele_Std->GetMeshElement()->GetPatchIndex();
  m_mfp = Fem_Ele_Std->FluidProp;

  for(int ii=0;ii<(int)pcs_vector.size();ii++){
    if(pcs_vector[ii]->pcs_type_name.find("FLOW")!=string::npos)
      FLOW = true;
  }
  if(FLOW)
    heat_conductivity_fluids = m_mfp->HeatConductivity();
  else {
    heat_conductivity_fluids = 0.0;
    porosity = 0.0;
  }
  dimen = m_pcs->m_msh->GetCoordinateFlag()/10;
  int group = m_pcs->m_msh->ele_vector[number]->GetPatchIndex();
 
  m_msp = msp_vector[group];
  m_msp->HeatConductivityTensor(dimen,heat_conductivity_tensor);


  for(i=0;i<dimen*dimen;i++)
    heat_conductivity_tensor[i] *= (1.0 - porosity);
  for(i=0;i<dimen;i++)
    heat_conductivity_tensor[i*dimen+i] += porosity*heat_conductivity_fluids; 

  return heat_conductivity_tensor;
}


/**************************************************************************
FEMLib-Method:
Task: 
Programing:
04/2002 OK Implementation
09/2004 OK MMP implementation 
12/2005 CMCD
last modification:
ToDo:
**************************************************************************/
double* CMediumProperties::HeatDispersionTensorNew(int ip)
{
  static double heat_dispersion_tensor[9];
  double *heat_conductivity_porous_medium;
  double vg, D[9];
  double heat_capacity_fluids=0.0;
  double fluid_density;
  double alpha_t, alpha_l;
  long index = Fem_Ele_Std->GetMeshElement()->GetIndex();
  CFluidProperties *m_mfp;
  int Dim = m_pcs->m_msh->GetCoordinateFlag()/10;
  int i;
  ElementValue* gp_ele = ele_gp_value[index]; 

  // Materials
  heat_conductivity_porous_medium = HeatConductivityTensor();
  m_mfp = Fem_Ele_Std->FluidProp;
  fluid_density = m_mfp->Density();
  heat_capacity_fluids = m_mfp->specific_heat_capacity;
  
  //Global Velocity
  double velocity[3]={0.,0.,0.};
  gp_ele->getIPvalue_vec(ip, velocity);//gp velocities
  vg = MBtrgVec(velocity,3);

//Dl in local coordinates
  alpha_l = heat_dispersion_longitudinal;
  alpha_t = heat_dispersion_transverse;

  if (abs(vg) > MKleinsteZahl){//For the case of diffusive transport only   
    switch (Dim) {
      case 1: // line elements
  	    heat_dispersion_tensor[0] =   heat_conductivity_porous_medium[0] + alpha_l*heat_capacity_fluids*fluid_density*vg;
        break;
      case 2:
        D[0] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[0]*velocity[0])/vg;
        D[1] = ((alpha_l - alpha_t)*(velocity[0]*velocity[1]))/vg;
        D[2] = ((alpha_l - alpha_t)*(velocity[1]*velocity[0]))/vg;
        D[3] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[1]*velocity[1])/vg;
        for (i = 0; i<4; i++)
          heat_dispersion_tensor[i] =   heat_conductivity_porous_medium[i] + (D[i]*heat_capacity_fluids*fluid_density);  
        break;
      case 3: 
        D[0] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[0]*velocity[0])/vg;
        D[1] = ((alpha_l - alpha_t)*(velocity[0]*velocity[1]))/vg;
        D[2] = ((alpha_l - alpha_t)*(velocity[0]*velocity[2]))/vg;
        D[3] = ((alpha_l - alpha_t)*(velocity[1]*velocity[0]))/vg;
        D[4] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[1]*velocity[1])/vg;
        D[5] = ((alpha_l - alpha_t)*(velocity[1]*velocity[2]))/vg;
        D[6] = ((alpha_l - alpha_t)*(velocity[2]*velocity[0]))/vg;
        D[7] = ((alpha_l - alpha_t)*(velocity[2]*velocity[1]))/vg;
        D[8] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[2]*velocity[2])/vg;
        for (i = 0; i<9; i++)
          heat_dispersion_tensor[i] =   heat_conductivity_porous_medium[i] + (D[i]*heat_capacity_fluids*fluid_density);  
        break;
    }
  }
  else {
    for (i = 0; i<Dim*Dim; i++)
          heat_dispersion_tensor[i] =   heat_conductivity_porous_medium[i];
  }
  return heat_dispersion_tensor;
}

/**************************************************************************
FEMLib-Method:
Task: 
Programing:
04/2002 OK Implementation
09/2004 OK MMP implementation 
10/2004 SB Adapted to mass dispersion
last modification:
ToDo:
**************************************************************************/
double* CMediumProperties::MassDispersionTensor(long number,double*gp,double theta, long component)
{
  static double dispersion_tensor[9];
//  static double *heat_conductivity_porous_medium;
  static double molecular_diffusion;
  static double art_diff=0.0;
  static double vg, *velovec;
//  double heat_capacity_fluids=0.0;
//  int phase = 0;
  static double dreh[4];
  static double matrix2x2a[4];
  static double matrix2x2b[4];
  int dim;
//  int eidx;
//  char name[80];
  static double *invjac, detjac;
  static double matrix3x3a[24]; //zwa[24]
  static double matrix3x3b[9]; //zwo[9];
  static double matrix3x3c[64]; //zwi[64];
  double fkt;
  int ii,l;

  //  static double zwa[24];
  // static double zwo[9];
  // static double zwi[64];
  static double d[9];
  MNulleMat(d,3,3);

  CompProperties *m_cp = cp_vec[component];

  //----------------------------------------------------------------------
  invjac = GEOGetELEJacobianMatrix(number,&detjac);
  //----------------------------------------------------------------------
  // Materials
  molecular_diffusion = m_cp->CalcDiffusionCoefficientCP(number) * TortuosityFunction(number,gp,theta);
//  heat_capacity_fluids = MFPCalcFluidsHeatCapacity(number,gp,theta);
  MNulleMat(dispersion_tensor,3,3);
  //----------------------------------------------------------------------
  // Velocity
//old version 
/*
  double v[3]={0.,0.,0.};
  sprintf(name,"%s%d_X","VELOCITY",phase+1);
  eidx = PCSGetELEValueIndex(name);
  v[0] = ElGetElementVal(number,eidx)/porosity;;
  sprintf(name,"%s%d_Y","VELOCITY",phase+1);
  eidx = PCSGetELEValueIndex(name);
  v[1] = ElGetElementVal(number,eidx)/porosity;;
  sprintf(name,"%s%d_Z","VELOCITY",phase+1);
  eidx = PCSGetELEValueIndex(name);
  v[2] = ElGetElementVal(number,eidx)/porosity;; 
  vg = MBtrgVec(v,3);
*/
// end old version
// new version
  double v[3]={0.,0.,0.};
  velovec = ElGetVelocity(number);
  v[0] = velovec[0]/porosity;
  v[1] = velovec[1]/porosity;
  v[2] = velovec[2]/porosity; //OK
  vg = MBtrgVec(v,3);
// end new version
//   if(number<10){DisplayMsgLn(" dispersion_tensor calculation: v[0]= "); DisplayDouble(v[0],0,0); DisplayMsg(", "); DisplayDouble(v[1],0,0); DisplayMsg(", "); DisplayDouble(v[2],0,0); DisplayMsg(",    vg:"); DisplayDouble(vg,0,0); DisplayMsgLn(" ");}
  //----------------------------------------------------------------------
  switch (ElGetElementType(number)) {
    //--------------------------------------------------------------------
    case 1: // line elements
//      dispersion_tensor[0] = heat_conductivity_porous_medium[0] + art_diff + heat_capacity_fluids * heat_dispersion_longitudinal * vg;
	  dispersion_tensor[0] = (molecular_diffusion + art_diff) + vg * mass_dispersion_longitudinal;
      // tdt = d * MSkalarprodukt(invjac, invjac, 3);
       dispersion_tensor[0] *= MSkalarprodukt(invjac,invjac,3);
      break;
    //--------------------------------------------------------------------
    case 2: // quad elements
      dim=2;
      dispersion_tensor[0] = molecular_diffusion + art_diff + mass_dispersion_longitudinal * vg;
      dispersion_tensor[3] = molecular_diffusion + art_diff + mass_dispersion_transverse * vg;
      /* Drehen des Tensors von stromlinienorientierten in lokale physikalische (Element) Koordinaten,
         wird benoetigt, wenn Dispersionstensor in Hauptachsen angegeben ist */
      if ((vg > MKleinsteZahl) && (dispersion_tensor[0] > MKleinsteZahl || dispersion_tensor[3] > MKleinsteZahl)){
        dreh[0] = dreh[3] = v[0] / vg;
        dreh[1] = v[1] / vg;
        dreh[2] = -dreh[1];
        // [T^T] [D_v] [T] -> [D_ab]
        MMultMatMat(dispersion_tensor,dim,dim,dreh,dim,dim, matrix2x2a,dim,dim);
        MTranspoMat(dreh,dim,dim,matrix2x2b);
        MMultMatMat(matrix2x2b,dim,dim,matrix2x2a,dim,dim,dispersion_tensor,dim,dim);
      }

      break;
    //--------------------------------------------------------------------
    case 4: // tri elements
      dim=2;
      dispersion_tensor[0] = molecular_diffusion + art_diff \
                           + mass_dispersion_longitudinal * vg;
      dispersion_tensor[1] = molecular_diffusion + art_diff \
                           + mass_dispersion_transverse * vg;
      break;
    //--------------------------------------------------------------------
    case 3: // hex elements
      dim=3;
      //..................................................................
      // Tensor in pathline coordinates
      dispersion_tensor[0] = molecular_diffusion + art_diff \
                           + mass_dispersion_longitudinal * vg;
      dispersion_tensor[4] = dispersion_tensor[8] = \
                             molecular_diffusion + art_diff \
                           + mass_dispersion_transverse * vg;
      //..................................................................
      // Tensor in local element coordinates
       // Drehen des Tensors von stromlinienorientierten in lokale physikalische (Element) Koordinaten,
       // wird benoetigt, wenn Dispersionstensor in Hauptachsen angegeben ist
      if (dispersion_tensor[0]>MKleinsteZahl||dispersion_tensor[4]>MKleinsteZahl||dispersion_tensor[8]>MKleinsteZahl){
        // Drehen: Stromrichtung - r,s,t
        if(vg>MKleinsteZahl){
          // 1. Zeile 
          for(l=0;l<3;l++)
            matrix3x3a[l] = v[l];
          MNormiere(matrix3x3a,dim);
          // 2. Zeile
          fkt = fabs(v[0]);
          ii = 0;
          for(l=1;l<3;l++)
            if(fabs(v[l])<fkt){
              fkt = fabs(v[l]);
              ii = l;
            }
          matrix3x3b[ii] = 0.0;
          matrix3x3b[(ii + 1) % 3] = v[(ii + 2) % 3];
          matrix3x3b[(ii + 2) % 3] = -v[(ii + 1) % 3];
          MNormiere(matrix3x3b,dim);
          // 3. Zeile
          M3KreuzProdukt(matrix3x3a,matrix3x3b,matrix3x3c);
          MNormiere(matrix3x3c,dim);
          for(l=0;l<dim;l++){
            matrix3x3a[3+l] = matrix3x3b[l];
            matrix3x3a[6+l] = matrix3x3c[l];
          }
          // dreh^T * D * dreh
          MMultMatMat(dispersion_tensor,dim,dim, matrix3x3a,dim,dim,matrix3x3c,dim,dim);
          MTranspoMat(matrix3x3a,dim,dim,matrix3x3b);
          MMultMatMat(matrix3x3b,dim,dim,matrix3x3c,dim,dim,dispersion_tensor,dim,dim);
        }
      }
    break;
	case 5: // tet elements
      dim =3;
      dispersion_tensor[0] = molecular_diffusion + art_diff \
                           + mass_dispersion_longitudinal * vg;
      dispersion_tensor[4] = molecular_diffusion + art_diff \
                           + mass_dispersion_transverse * vg;
      dispersion_tensor[8] = molecular_diffusion + art_diff \
                           + mass_dispersion_transverse * vg;
      break;
    //--------------------------------------------------------------------
    case 6: // pris elements
      dim=3;
      //..................................................................
      // Tensor in pathline coordinates
      dispersion_tensor[0] = molecular_diffusion + art_diff \
                           + mass_dispersion_longitudinal * vg;
      dispersion_tensor[4] = dispersion_tensor[8] = \
                             molecular_diffusion + art_diff \
                           + mass_dispersion_transverse * vg;
      //..................................................................
      // Tensor in local element coordinates
      break;
    default:
      cout << "Error in CMediumProperties::HeatDispersionTensor: no valid element type" << endl;
  }
  return dispersion_tensor;
}

/**************************************************************************
FEMLib-Method:
Task: 
Programing:
04/2002 OK Implementation
09/2004 OK MMP implementation 
10/2004 SB Adapted to mass dispersion
11/2005 CMCD
ToDo:
**************************************************************************/
double* CMediumProperties::MassDispersionTensorNew(int ip)
{
  static double advection_dispersion_tensor[9];//Name change due to static conflict
  int eleType;
  int component = Fem_Ele_Std->pcs->pcs_component_number;
  int i;
  long index = Fem_Ele_Std->GetMeshElement()->GetIndex();
  double molecular_diffusion[9], molecular_diffusion_value;
  double vg; 
  double D[9];
  double alpha_l,alpha_t;
  double theta = Fem_Ele_Std->pcs->m_num->ls_theta;
  double g[3]={0.,0.,0.};
  ElementValue* gp_ele = ele_gp_value[index]; 
  CompProperties *m_cp = cp_vec[component];
  eleType=m_pcs->m_msh->ele_vector[number]->GetElementType();
  int Dim = m_pcs->m_msh->GetCoordinateFlag()/10;

  //----------------------------------------------------------------------
  // Materials
  molecular_diffusion_value = m_cp->CalcDiffusionCoefficientCP(index) * TortuosityFunction(index,g,theta)*porosity;
  for (i = 0; i<Dim*Dim; i++)
    molecular_diffusion[i] = 0.0;
  for (i = 0; i<Dim; i++)
    molecular_diffusion[i*Dim+i] = molecular_diffusion_value;
  //----------------------------------------------------------------------

  //Global Velocity
  double velocity[3]={0.,0.,0.};
  gp_ele->getIPvalue_vec(ip, velocity);//gp velocities
  vg = MBtrgVec(velocity,3);

  //Dl in local coordinates
  alpha_l = mass_dispersion_longitudinal;
  alpha_t = mass_dispersion_transverse;
 
  //----------------------------------------------------------------------

  if (abs(vg) > MKleinsteZahl){ //For the case of diffusive transport only.
    switch (Dim) {
      //--------------------------------------------------------------------
      case 1: // line elements
  	    advection_dispersion_tensor[0] = molecular_diffusion[0] + alpha_l*vg;
        break;
      //--------------------------------------------------------------------
      case 2:  
        D[0] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[0]*velocity[0])/vg;
        D[1] = ((alpha_l - alpha_t)*(velocity[0]*velocity[1]))/vg;
        D[2] = ((alpha_l - alpha_t)*(velocity[1]*velocity[0]))/vg;
        D[3] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[1]*velocity[1])/vg;
        for (i = 0; i<4; i++)
          advection_dispersion_tensor[i] = molecular_diffusion[i] + D[i];
        break;
      //--------------------------------------------------------------------
      case 3: 
        D[0] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[0]*velocity[0])/vg;
        D[1] = ((alpha_l - alpha_t)*(velocity[0]*velocity[1]))/vg;
        D[2] = ((alpha_l - alpha_t)*(velocity[0]*velocity[2]))/vg;
        D[3] = ((alpha_l - alpha_t)*(velocity[1]*velocity[0]))/vg;
        D[4] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[1]*velocity[1])/vg;
        D[5] = ((alpha_l - alpha_t)*(velocity[1]*velocity[2]))/vg;
        D[6] = ((alpha_l - alpha_t)*(velocity[2]*velocity[0]))/vg;
        D[7] = ((alpha_l - alpha_t)*(velocity[2]*velocity[1]))/vg;
        D[8] = (alpha_t * vg) + (alpha_l - alpha_t)*(velocity[2]*velocity[2])/vg;
        for (i = 0; i<9; i++)
          advection_dispersion_tensor[i] = molecular_diffusion[i] + D[i];
        break;
    }
  }
  else {
    for (i = 0; i<Dim*Dim; i++)
          advection_dispersion_tensor[i] = molecular_diffusion[i];
  }
  return advection_dispersion_tensor;
}

////////////////////////////////////////////////////////////////////////////
// DB functions
////////////////////////////////////////////////////////////////////////////

/**************************************************************************
FEMLib-Method: CMediumProperties
Task: get instance by name
Programing:
02/2004 OK Implementation
last modification:
**************************************************************************/
CMediumProperties* CMediumProperties::GetDB(string mat_name)
{
  CMediumProperties *m_mat = NULL;
  list<CMediumProperties*>::const_iterator p_mat = db_mat_mp_list.begin();
  while(p_mat!=db_mat_mp_list.end()) {
    m_mat = *p_mat;
    if(mat_name.compare(m_mat->name)==0)
      return m_mat;
    ++p_mat;
  }
  return NULL;
}

/**************************************************************************
FEMLib-Method: CMediumProperties
Task: set properties 
Programing:
02/2004 OK Implementation
last modification:
**************************************************************************/
void CMediumProperties::SetDB(string mat_name,string prop_name,double value)
{
  CMediumProperties *m_mat = NULL;
  m_mat = GetDB(mat_name);
  switch (GetPropertyType(prop_name)) {
    case 0:
      m_mat->conductivity = value;
      break;
    case 1:
      m_mat->permeability = value;
      break;
    case 2:
      m_mat->porosity = value;
      break;
  }
}

/**************************************************************************
FEMLib-Method: CMediumProperties::GetPropertyType
Task: get property type
Programing:
02/2004 OK Implementation
last modification:
**************************************************************************/
int CMediumProperties::GetPropertyType(string prop_name)
{
  int counter=0;
  string keyword_name;
  list<string>::const_iterator p_keywords = keywd_list.begin();
  while(p_keywords!=keywd_list.end()) {
    keyword_name = *p_keywords;
    if(prop_name.compare(keyword_name)==0)
      return counter;
    ++p_keywords,
    counter++;
  }
  return -1;
}




////////////////////////////////////////////////////////////////////////////
// MAT-MP data base
////////////////////////////////////////////////////////////////////////////
string read_MAT_name(string in, string *z_rest_out)
{
  string mat_name;
  string z_rest;
  string delimiter(";");
  if(in.find_first_not_of(delimiter)!=string::npos)//wenn eine mg gefunden wird nach dem Schlüsselwort
  {
    z_rest = in.substr(in.find_first_not_of(delimiter));
    mat_name = z_rest.substr(0,z_rest.find_first_of(delimiter)); //string für die erste (1) material group
    *z_rest_out = z_rest.substr(mat_name.length());
	return mat_name;
  }
  else
    return "";
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
01/2004 OK/JG Implementation
last modification:
**************************************************************************/
void MATLoadDB(string csv_file_name)
{
  string keyword("MATERIALS");
  string in;
  string line;
  string z_rest;
  string mat_name;
  string mat_name_tmp("MAT_NAME");
  //========================================================================
  // Read available MAT-MP keywords
  read_keywd_list();
  //========================================================================
  // File handling
  ifstream csv_file(csv_file_name.data(),ios::in);
  if (!csv_file.good()) return;
  csv_file.seekg(0L,ios::beg); // rewind
  //========================================================================
  // Read MATERIALS group names
  comp_keywd_list(csv_file_name);
/*
   //iostream
   // 2.1 Create MAT-MP instances
   // search "MATERIALS"
   SOIL_PROPERTIES *sp = NULL;
   sp = MATAddSoilPropertiesObject(); //OK
   strcpy(sp->mat_type_name,"Brooklawn");
   // 2.2 Insert to db_mat_mp_list
   db_material_mp_list.push_back(sp);
   // 2.3 Read material properties
   string line;
   mat_read_hydraulic_conductivity_mean(line,sp->mat_type_name);
   // select corresponding MAT-MP
  }
*/
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
01/2004 JG Implementation
last modification:
**************************************************************************/
void read_keywd_list(void)
{
  //File handling=============================
  ifstream eingabe("mat_mp_keywords.dat",ios::in);
  if (eingabe.good()) {
	eingabe.seekg(0L,ios::beg);
    //==========================================  
    string keyword("MATERIALS");
    string in;
    string line;
    string z_rest;
    string mat_name;
    string delimiter(";");
    string keywd_tmp("KEYWD");
    char line_char[MAX_ZEILE];
    // Read MATERIALS group names
      //1 get string with keywords
    while (!eingabe.eof()) {
	  eingabe.getline(line_char, MAX_ZEILE);
      line = line_char;
      if(line.find_first_not_of(delimiter)!=string::npos) {
        in = line.substr(line.find_first_not_of(delimiter));//schneidet delimiter ab
        keywd_tmp = in.substr(0,in.find_first_of(delimiter));
        keywd_list.push_back(keywd_tmp);
      }
      //keywd_list.remove(keyword);
    } // eof
  }// if eingabe.good
  else {
#ifdef MFC
	  AfxMessageBox("No keyword file: mat_mp_keywords.dat");
#else
	  printf("No keyword file: mat_mp_keywords.dat");
#endif
  }
  return;
}


/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
01/2004 JG/OK Implementation
last modification:
**************************************************************************/
void comp_keywd_list(string csv_file_name)
{
  string mat_names("MATERIALS");
  string in;
  string line;
  string z_rest;
  string mat_name;
  string mat_name_tmp("MAT_NAME");
  char line_char[MAX_ZEILE];
  string in1;//zwischenstring zum abschneiden der einheit
  string delimiter(";");
  string keyword("MATERIALS");
  double kwvalue;

  //File handling------------------------------------
  string sp;
  ifstream eingabe(csv_file_name.data(),ios::in);
  if (eingabe.good()) {
    eingabe.seekg(0L,ios::beg);//rewind um materialgruppen auszulesen
	eingabe.getline(line_char, MAX_ZEILE);
    line = line_char;
    //-----------------------------------------------  
    // 1 - read MAT names
    // ReadMATNames(csv_file_name);
    if(line.find(mat_names)!=string::npos) {
      in = line.substr(mat_names.length()+1);
      // 2 Read material group names
      while(!mat_name_tmp.empty()) {
         mat_name_tmp = read_MAT_name(in,&z_rest);
        if(mat_name_tmp.empty()) 
          break; 
        else {
          mat_name = mat_name_tmp;
          mat_name_list.push_back(mat_name);
          in = z_rest;
       }
      } // while mat_name
    } // keyword found
    //-----------------------------------------------  
    // 2 - create MAT-SP instances
    CMediumProperties *m_mat_mp = NULL;
    list<string>::const_iterator p_mat_names=mat_name_list.begin();
    while(p_mat_names!=mat_name_list.end()) {
      mat_name = *p_mat_names;
      m_mat_mp = new CMediumProperties;
      m_mat_mp->name = mat_name;
      db_mat_mp_list.push_back(m_mat_mp);
      ++p_mat_names;
    }
    //-----------------------------------------------  
    // 3 - read MAT properties
    
     //1 get string where keyword hits 
    CMediumProperties *m_mat_mp1 = NULL;
    while (!eingabe.eof()) {
	  eingabe.getline(line_char, MAX_ZEILE);
      line = line_char;
      string sp;
      list<string>::const_iterator pm =keywd_list.begin();
      while(pm!=keywd_list.end()) {
        sp = *pm;
        if(line.find(sp)!=string::npos) {
          in1 = line.substr(line.find_first_not_of(delimiter)+sp.length()+1); //schneidet keyword ab
          in = in1.substr(in1.find_first_of(delimiter)); //schneidet keyword ab
          //Schleife über alle MAT-Gruppen
          list<string>::const_iterator p_mat_names=mat_name_list.begin();
          while(p_mat_names!=mat_name_list.end()) {
            mat_name = *p_mat_names;
            m_mat_mp1 = m_mat_mp1->GetDB(mat_name);
            mat_name_tmp = read_MAT_name(in,&z_rest);
            kwvalue = atof(mat_name_tmp.data());
//Val = strtod(pDispInfo->item.strText, NULL);
            m_mat_mp1->SetDB(mat_name,sp,kwvalue);
            ++p_mat_names;
          }
        } // 
        ++pm;
      } // kwlist
    } // eof
  } // if eingabe.good
  return;
}




///*************************************************************************************************
////////////////////////////////////////////////////////////////////////////
// Properties functions
////////////////////////////////////////////////////////////////////////////

/*************************************************************************
 ROCKFLOW - Funktion: SetSoilPropertiesDefaultsClay

 Aufgabe:

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: SOIL_PROPERTIES *sp: Zeiger auf SOIL_PROPERTIES

 Ergebnis:
   - void -

 Programmaenderungen:
   10/2003   CMCD   Erste Version

*************************************************************************/

void CMediumProperties::SetMediumPropertiesDefaultsClay(void)
{
	int i;
    geo_dimension = 3;
    geo_area = 1.0;
    porosity = 0.4;
    tortuosity = 1.0;
    storage = 1.5e-10;//m/pa
    for (i = 0; i < 9; i++) permeability_tensor[i]=1.e-17;
    heat_dispersion_longitudinal = 0.01;
	heat_dispersion_transverse = 0.01;
    mass_dispersion_longitudinal = 0.01;
	mass_dispersion_transverse = 0.01;
    for (i = 0; i < 9; i++) heat_conductivity_tensor[i] = 3.2;
   
return;
}

/*************************************************************************
 ROCKFLOW - Funktion: SetSoilPropertiesDefaultsSilt

 Aufgabe:

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: SOIL_PROPERTIES *sp: Zeiger auf SOIL_PROPERTIES

 Ergebnis:
   - void -

 Programmaenderungen:
   10/2003   CMCD   Erste Version

*************************************************************************/

void CMediumProperties::SetMediumPropertiesDefaultsSilt(void)
{
	int i;
    geo_dimension = 3;
    geo_area = 1.0;
    porosity = 0.4;
    tortuosity = 1.0;
    storage = 5.e-7;//m/pa
    for (i = 0; i < 9; i++) permeability_tensor[i]=1.e-14;//m²
    heat_dispersion_longitudinal = 0.01;
	heat_dispersion_transverse = 0.01;
    mass_dispersion_longitudinal = 0.01;
	mass_dispersion_transverse = 0.01;
    for (i = 0; i < 9; i++) heat_conductivity_tensor[i] = 3.2;
   
return;
}
/*************************************************************************
 ROCKFLOW - Funktion: SetSoilPropertiesDefaultsSand

 Aufgabe:

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: SOIL_PROPERTIES *sp: Zeiger auf SOIL_PROPERTIES

 Ergebnis:
   - void -

 Programmaenderungen:
   10/2003   CMCD   Erste Version

*************************************************************************/
void CMediumProperties::SetMediumPropertiesDefaultsSand(void)
{
	int i;
    geo_dimension = 3;
    geo_area = 1.0;
    porosity = 0.3;
    tortuosity = 1.0;
    storage = 5.e-8;//m/pa
    for (i = 0; i < 9; i++) permeability_tensor[i]=1.e-11;//m²
    heat_dispersion_longitudinal = 0.01;
	heat_dispersion_transverse = 0.01;
    mass_dispersion_longitudinal = 0.01;
	mass_dispersion_transverse = 0.01;
    for (i = 0; i < 9; i++) heat_conductivity_tensor[i] = 3.2;
   
return;
}

/*************************************************************************
 ROCKFLOW - Funktion: SetSoilPropertiesDefaultsGravel

 Aufgabe:

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: SOIL_PROPERTIES *sp: Zeiger auf SOIL_PROPERTIES

 Ergebnis:
   - void -

 Programmaenderungen:
   10/2003   CMCD   Erste Version

*************************************************************************/

void CMediumProperties::SetMediumPropertiesDefaultsGravel(void)
{
	int i;
    geo_dimension = 3;
    geo_area = 1.0;
    porosity = 0.32;
    tortuosity = 1.0;
    storage = 5.e-9;//m/pa
    for (i = 0; i < 9; i++) permeability_tensor[i]=1.e-9;//m²
    heat_dispersion_longitudinal = 0.01;
	heat_dispersion_transverse = 0.01;
    mass_dispersion_longitudinal = 0.01;
	mass_dispersion_transverse = 0.01;
    for (i = 0; i < 9; i++) heat_conductivity_tensor[i] = 3.2;
   
return;
}

/*************************************************************************
 ROCKFLOW - Funktion: SetMediumPropertiesDefaultsCrystaline

 Aufgabe:

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: SOIL_PROPERTIES *sp: Zeiger auf SOIL_PROPERTIES

 Ergebnis:
   - void -

 Programmaenderungen:
   10/2003   CMCD   Erste Version

*************************************************************************/
void CMediumProperties::SetMediumPropertiesDefaultsCrystalline(void)
{
	int i;
    geo_dimension = 3;
    geo_area = 1.0;
    porosity = 0.05;
    tortuosity = 1.0;
    storage = 5.e-11;//m/pa
    for (i = 0; i < 9; i++) permeability_tensor[i]=1.e-14;//m²
    heat_dispersion_longitudinal = 0.01;
	heat_dispersion_transverse = 0.01;
    mass_dispersion_longitudinal = 0.01;
	mass_dispersion_transverse = 0.01;
    for (i = 0; i < 9; i++) heat_conductivity_tensor[i] = 3.2;
   
return;
}

/*************************************************************************
 ROCKFLOW - Funktion: SetMediumPropertiesDefaultsCrystaline

 Aufgabe:

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: SOIL_PROPERTIES *sp: Zeiger auf SOIL_PROPERTIES

 Ergebnis:
   - void -

 Programmaenderungen:
   10/2003   CMCD   Erste Version

*************************************************************************/
void CMediumProperties::SetMediumPropertiesDefaultsBordenAquifer(void)
{
	int i;
    geo_dimension = 3;
    geo_area = 1.0;
    porosity = 0.10;
    tortuosity = 1.0;
    storage = 5.e-11;//m/pa
    for (i = 0; i < 9; i++) permeability_tensor[i]=1.e-12;//m²
    heat_dispersion_longitudinal = 0.01;
	heat_dispersion_transverse = 0.01;
    mass_dispersion_longitudinal = 0.01;
	mass_dispersion_transverse = 0.01;
    for (i = 0; i < 9; i++) heat_conductivity_tensor[i] = 3.2;
   
return;
}

/*------------------------------------------------------------------------*/
/* MAT-MP geometric properties */
/* 3 porosity */
/*------------------------------------------------------------------------*/

/**************************************************************************
FEMLib-Method:
Task: Porosity calc function
  Case overview
  0 Curve function 
  1 Constant Value
  2 Function of normal stress from Geomechnical model
  3 Free swelling
  4 Constraint swelling
Programing:
07/2004 OK C++ Implementation
08/2004	CMCD Re-written based on MATCalcPorosity
10/2004 MX Swelling processes
last modification:
*************************************************************************/
double CMediumProperties::Porosity(long number,double*gp,double theta) 
{
  static int nidx0,nidx1;
  double primary_variable[PCS_NUMBER_MAX];
  int count_nodes;
  long* element_nodes = NULL;
  int gueltig;
  double porosity_sw;
  CFiniteElementStd* assem = m_pcs->GetAssember();
  ///
  bool New = false; // To be removed WW
  if(fem_msh_vector.size()>0) New = true;
 
  //----------------------------------------------------------------------
  // Functional dependencies
  int i;
  int no_pcs_names =(int)pcs_name_vector.size();
  for(i=0;i<no_pcs_names;i++){
    if(New)
    {
       nidx0 = m_pcs->GetNodeValueIndex(pcs_name_vector[i]);
	   nidx1 = nidx0+1;  
      if(mode==0){ // Gauss point values
         assem->ComputeShapefct(1);
         primary_variable[i] = (1.-theta)*assem->interpolate(nidx0,m_pcs) 
                                  + theta*assem->interpolate(nidx1,m_pcs);
      }
      else if(mode==1){ // Node values
         primary_variable[i] = (1.-theta)*m_pcs->GetNodeValue(number,nidx0) \
                            + theta*m_pcs->GetNodeValue(number,nidx1);
      }
      else if(mode==2){ // Element average value
         primary_variable[i] = (1.-theta)*assem->elemnt_average(nidx0,m_pcs)
                                  + theta*assem->elemnt_average(nidx1,m_pcs);
      }
	}
	else
	{
       nidx0 = PCSGetNODValueIndex(pcs_name_vector[i],0);
       nidx1 = PCSGetNODValueIndex(pcs_name_vector[i],1);
       if(mode==0){ // Gauss point values
        primary_variable[i] = (1.-theta)*InterpolValue(number,nidx0,gp[0],gp[1],gp[2]) \
                          + theta*InterpolValue(number,nidx1,gp[0],gp[1],gp[2]);
       }
       else if(mode==1){ // Node values
         primary_variable[i] = (1.-theta)*GetNodeVal(number,nidx0) \
                          + theta*GetNodeVal(number,nidx1);
       }
       else if(mode==2){ // Element average value
         count_nodes = ElNumberOfNodes[ElGetElementType(number) - 1];
         element_nodes = ElGetElementNodes(number);
         for (i = 0; i < count_nodes; i++)
	       primary_variable[i] += GetNodeVal(element_nodes[i],nidx1);
         primary_variable[i]/= count_nodes;
       }
    }
  }
  //----------------------------------------------------------------------
  // Material models
  switch (porosity_model) {
    case 0: // n = f(x)
      porosity = GetCurveValue(fct_number,0,primary_variable[0],&gueltig);
      break;
    case 1: // n = const
      porosity = porosity_model_values[0];
      break;
    case 2: // n = f(sigma_eff), Stress dependance
	  porosity = PorosityEffectiveStress(number,primary_variable[0]);
      break;
	case 3: // n = f(S), Free chemical swelling
      porosity = PorosityVolumetricFreeSwellingConstantIonicstrength(number,primary_variable[0],primary_variable[1]);
      break;
	case 4: // n = f(S), Constrained chemical swelling 
      porosity = PorosityEffectiveConstrainedSwellingConstantIonicStrength(number,primary_variable[0],primary_variable[1],&porosity_sw);
      break;
  	case 5: // n = f(S), Free chemical swelling, I const
      porosity = PorosityVolumetricFreeSwelling(number,primary_variable[0],primary_variable[1]);
	  break;
	case 6: // n = f(S), Constrained chemical swelling, I const 
      porosity = PorosityEffectiveConstrainedSwelling(number,primary_variable[0],primary_variable[1],&porosity_sw);
      break;
    case 10:
      porosity = PorosityVolumetricChemicalReaction(number);     /* porosity change through dissolution/precipitation */
      break;
	default:
      DisplayMsgLn("Unknown porosity model!");
      break;
  }
  //----------------------------------------------------------------------
  return (porosity);
}


/*------------------------------------------------------------------------*/
/* MAT-MP geometric properties */
/* 3 porosity */
/*------------------------------------------------------------------------*/

/**************************************************************************
FEMLib-Method:
Task: Porosity calc function
  Case overview
  0 Curve function 
  1 Constant Value
  2 Function of normal stress from Geomechnical model
  3 Free swelling
  4 Constraint swelling
Programing:
07/2004 OK C++ Implementation
08/2004	CMCD Re-written based on MATCalcPorosity
10/2004 MX Swelling processes
last modification:
*************************************************************************/
double CMediumProperties::Porosity(CFiniteElementStd* assem) 
{
  static int nidx0,nidx1;
  double primary_variable[PCS_NUMBER_MAX];
//  long* element_nodes = NULL;
  int gueltig;
  double porosity_sw;
  long number = assem->GetMeshElement()->GetIndex();

  //CFiniteElementStd* assem = m_pcs->GetAssember();
  ///
  bool New = false; // To be removed WW
  if(fem_msh_vector.size()>0) New = true;
 
  //----------------------------------------------------------------------
  // Functional dependencies
  int i;
  int no_pcs_names =(int)pcs_name_vector.size();
  for(i=0;i<no_pcs_names;i++){
       nidx0 = m_pcs->GetNodeValueIndex(pcs_name_vector[i]);
	   nidx1 = nidx0+1;  
      if(mode==0){ // Gauss point values
         assem->ComputeShapefct(1);
         primary_variable[i] = (1.-assem->pcs->m_num->ls_theta)*assem->interpolate(nidx0,m_pcs) 
                                  + assem->pcs->m_num->ls_theta*assem->interpolate(nidx1,m_pcs);
      }
/*
      else if(mode==1){ // Node values
         primary_variable[i] = (1.-theta)*GetNodeValue(assem->Index,nidx0) \
                            + theta*GetNodeValue(number,nidx1);
      }
*/
      else if(mode==2){ // Element average value
         primary_variable[i] = (1.-assem->pcs->m_num->ls_theta)*assem->elemnt_average(nidx0,m_pcs)
                                  + assem->pcs->m_num->ls_theta*assem->elemnt_average(nidx1,m_pcs);
      }
  }
  //----------------------------------------------------------------------
  // Material models
  switch (porosity_model) {
    case 0: // n = f(x)
      porosity = GetCurveValue(fct_number,0,primary_variable[0],&gueltig);
      break;
    case 1: // n = const
      porosity = porosity_model_values[0];
      break;
    case 2: // n = f(sigma_eff), Stress dependance
	  porosity = PorosityEffectiveStress(number,primary_variable[0]);
      break;
	case 3: // n = f(S), Free chemical swelling
      porosity = PorosityVolumetricFreeSwellingConstantIonicstrength(number,primary_variable[0],primary_variable[1]);
      break;
	case 4: // n = f(S), Constrained chemical swelling 
      porosity = PorosityEffectiveConstrainedSwellingConstantIonicStrength(number,primary_variable[0],primary_variable[1],&porosity_sw);
      break;
  	case 5: // n = f(S), Free chemical swelling, I const
      porosity = PorosityVolumetricFreeSwelling(number,primary_variable[0],primary_variable[1]);
	  break;
	case 6: // n = f(S), Constrained chemical swelling, I const 
      porosity = PorosityEffectiveConstrainedSwelling(number,primary_variable[0],primary_variable[1],&porosity_sw);
      break;
    case 10:
      porosity = PorosityVolumetricChemicalReaction(number);     /* porosity change through dissolution/precipitation */
      break;
	default:
      DisplayMsgLn("Unknown porosity model!");
      break;
  }
  //----------------------------------------------------------------------
  return (porosity);
}


/**************************************************************************
//3.1 Subfunction of Porosity
 ROCKFLOW - Funktion: MATCalcSoilPorosityMethod1

 Aufgabe:
   Takes the porosity from a Geomechanical model, calulcates the effective stress
   and then takes the value of porosity from a curve.

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E SOIL_PROPERTIES *sp: Zeiger auf eine Instanz vom Typ SOIL_PROPERTIES.

 Ergebnis:
   - s.o. -
Programming Example
Programmaenderungen:
   03/2004  CMCD First version

**************************************************************************/
double CMediumProperties::PorosityEffectiveStress(long index, double element_pressure)
{
    int  nn, i, idummy, Type;
    long *element_nodes;
    double p;
	double znodes[8],ynodes[8],xnodes[8];
    double zelnodes[8],yelnodes[8],xelnodes[8];
	double coords[3];
	double Pie,angle;
	double sigma1,sigma2,sigma3;
	double a1,a2,a3,b1,b2,b3;
	double normx,normy,normz,normlen;
	double dircosl, dircosm, dircosn;
    double tot_norm_stress, eff_norm_stress;
	int material_group;
	double x_mid, y_mid, z_mid;


/* Normal stress calculated according to the orientation of the fracture element*/
        
		Type=ElGetElementType(index);
		material_group = ElGetElementGroupNumber(index);
	
		dircosl = dircosm = dircosn = 0.0;//Initialise variable
				

		if (Type == 2||Type == 3||Type == 4)  /*Function defined for square, triangular and cubic elements*/
		{
			nn = ElNumberOfNodes[Type - 1];
			element_nodes = ElGetElementNodes(index);	
		
 			/* Calculate directional cosins, note that this is currently set up*/
			/* Sigma1 is in the y direction*/
			/* Sigma2 is in the z direction*/
			/* Sigma3 is in the x direction*/
			/* This correspondes approximately to the KTB site conditions*/

			for (i=0;i<nn;i++)
			{
				zelnodes[i]=GetNodeZ(element_nodes[i]);
				yelnodes[i]=GetNodeY(element_nodes[i]);
				xelnodes[i]=GetNodeX(element_nodes[i]);
			}
			

			/*Coordinate transformation to match sigma max direction*/
			/* y direction matches the north direction*/
			Pie=3.141592654;
			angle=(porosity_model_values[3]*Pie)/180.;
			for (i=0;i<nn;i++)
			{
				znodes[i]=zelnodes[i];
				xnodes[i]=xelnodes[i]*cos(angle)-yelnodes[i]*sin(angle);
				ynodes[i]=xelnodes[i]*sin(angle)+yelnodes[i]*cos(angle);
				
			}


			if (Type == 2) /*Square*/
			{
			a1=xnodes[2]-xnodes[0];
			a2=ynodes[2]-ynodes[0];
			a3=znodes[2]-znodes[0];
			b1=xnodes[3]-xnodes[1];
			b2=ynodes[3]-ynodes[1];
			b3=znodes[3]-znodes[1];
			normx=a2*b3-a3*b2;
			normy=a3*b1-a1*b3;
			normz=a1*b2-a2*b1;
			normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
			dircosl= normy/normlen;
			dircosm= normz/normlen;
			dircosn= normx/normlen;
			}	
		
			if (Type == 3) /*Cube*/
			{
			a1=xnodes[2]-xnodes[0];
			a2=ynodes[2]-ynodes[0];
			a3=znodes[2]-znodes[0];
			b1=xnodes[3]-xnodes[1];
			b2=ynodes[3]-ynodes[1];
			b3=znodes[3]-znodes[1];
			normx=a2*b3-a3*b2;
			normy=a3*b1-a1*b3;
			normz=a1*b2-a2*b1;
			normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
			dircosl= normy/normlen;
			dircosm= normz/normlen;
			dircosn= normx/normlen;
			}	
		
			if (Type == 4) /*Triangle*/
			{
			a1=xnodes[1]-xnodes[0];
			a2=ynodes[1]-ynodes[0];
			a3=znodes[1]-znodes[0];
			b1=xnodes[2]-xnodes[1];
			b2=ynodes[2]-ynodes[1];
			b3=znodes[2]-znodes[1];
			normx=a2*b3-a3*b2;
			normy=a3*b1-a1*b3;
			normz=a1*b2-a2*b1;
			normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
			dircosl= normy/normlen;
			dircosm= normz/normlen;
			dircosn= normx/normlen;
			}	
		
		
		/* Calculation of average location of element*/
			CalculateSimpleMiddelPointElement(index,coords);
			x_mid = coords[0];
			y_mid = coords[1];
			z_mid = coords[2];

			/*Calculate fluid pressure in element*/ 
			p = element_pressure;

			/*Calcualtion of stress according to Ito & Zoback 2000 for KTB hole*/
					
			sigma1=z_mid*0.045*-1e6;
			sigma2=z_mid*0.028*-1e6;
			sigma3=z_mid*0.02*-1e6;

			//Calculate total normal stress on element
			/*Note in this case sigma2 corresponds to the vertical stress*/
			tot_norm_stress=sigma1*dircosl*dircosl+sigma2*dircosm*dircosm+sigma3*dircosn*dircosn;
					
			/*Calculate normal effective stress*/
			eff_norm_stress=tot_norm_stress-p;
				
			/*Take value of storage from a curve*/
			porosity=GetCurveValue((int) porosity_model_values[0], 0, eff_norm_stress, &idummy);
		
		}
		/* default value if element type is not included in the method for calculating the normal */
		else porosity=porosity_model_values[0];
  return porosity;
}

/**************************************************************************/
/* ROCKFLOW - Funktion: CalculateSoilPorosityMethod1
 */
/* Aufgabe:
   Berechnet die Porositaet in Abhaengigkeit von der Konzentration
   (Salzloesungsmodell)
 */
/* Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E SOIL_PROPERTIES *sp: Zeiger auf eine Instanz vom Typ SOIL_PROPERTIES.
 */
/* Ergebnis:
   - s.o. -
 */
/* Programmaenderungen:
   02/2000     RK     Erste Version
   11/2001     AH     Warnung entfernt


 */
/**************************************************************************/
/*
double CalculateSoilPorosityMethod1(SOIL_PROPERTIES * sp, long index)
{
    // double grainbound_limit = 1.73e-8;
    double porosity_limit = 0.01;
    int porosity_dependence_model;
    double val0, val1;
    static double porosity = 0.0;
    static double rho;
    static double theta;
    static double porosity_n, density_rock;
    static double dissolution_rate, solubility_coefficient;
    int timelevel = 1;

    porosity_dependence_model = get_sp_porosity_dependence_model(sp);
    switch (porosity_dependence_model) {
    case 1: //Salzloesungsmodell
        val0 = get_sp_porosity_model_field_value(sp, 0);
        val1 = get_sp_porosity_model_field_value(sp, 1);

        theta = GetNumericalTimeCollocation("TRANSPORT");
        density_rock = GetSolidDensity(index);
        dissolution_rate = GetTracerDissolutionRate(index, 0, 0);
        porosity_n = GetElementSoilPorosity(index);
        rho = GetFluidDensity(0, index, 0., 0., 0., theta);
        if (GetTracerSolubilityModel(index, 0, 0) == 1) {
          //if (GetRFProcessHeatReactModel())
                solubility_coefficient = CalcTracerSolubilityCoefficient(index,0,0,1,PCSGetNODValueIndex("CONCENTRATION1",timelevel),1,PCSGetNODValueIndex("CONCENTRATION1",timelevel));
                //else solubility_coefficient = CalcTracerSolubilityCoefficient(index,0,0,1,PCSGetNODValueIndex("CONCENTRATION1",timelevel),0,PCSGetNODValueIndex("CONCENTRATION1",timelevel));
        } else {
            solubility_coefficient = GetTracerSolubilityCoefficient(index, 0, 0);
        }

        porosity = porosity_n + 2 * porosity_n * dissolution_rate * val1 * rho
            * (solubility_coefficient - val0) * dt / density_rock;

        if (porosity > porosity_limit)
            porosity = porosity_limit;


        break;

    default:
        DisplayMsgLn("Unknown porosity dependence model!");
        break;

    }

    return porosity;
}
*/

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
05/2005 MX Implementation
last modification:
**************************************************************************/
double CMediumProperties::PorosityVolumetricFreeSwellingConstantIonicstrength(long index,double saturation,double temperature)
{
  double mat_mp_m, beta;
  double porosity = 0.0;
  static double theta;
  static double porosity_n, porosity_IL, d_porosity, \
                density_rock, fmon;
  static double S_0;
  static double epsilon;
  static double satu_0=0.20;
  //  static double porosity_min=0.05;
  static double ion_strength;
  static double F_const=96484.6, epsilon_0=8.854e-12;
  static double R=8.314510, psi=1.0;
  theta = 1.0;
  //--------------------------------------------------------------------
  // MMP medium properties
  S_0 =      porosity_model_values[1];      // Specific surface area [m^2/g]
  fmon =     porosity_model_values[2];     // Anteil quelfaehige mineral [-]
  mat_mp_m = porosity_model_values[3]; // Schichtenanzahl eines quellfähigen Partikels [-]
  beta =     porosity_model_values[6];     // modifications coefficient (z.B. free swelling Weimar beta=3.0)
  //--------------------------------------------------------------------
  // MSP solid properties 
  CSolidProperties *m_msp = NULL;
  long group = ElGetElementGroupNumber(index);
  m_msp = msp_vector[group];
  density_rock  = m_msp->Density(1);
  //--------------------------------------------------------------------
  // State properties 
  ion_strength = porosity_model_values[4]; // Ionic strength [M]
  satu_0 =       porosity_model_values[5];       // Initial saturation, if the sample is homogenous [-]
  //--------------------------------------------------------------------
  // Interlayer porosity calculation
  if (abs(temperature)>1.0e10) temperature=298.0;  //TODO MX
  epsilon = 87.0 + exp(-0.00456*(temperature-273.0));
  porosity_n = porosity_model_values[0];
  // Maximal inter layer porosity
  porosity_IL = fmon * psi * mat_mp_m * S_0 * (density_rock * 1.0e3) \
              * sqrt(epsilon * epsilon_0 * R * temperature / (2000.0 * F_const * F_const * ion_strength ));
  d_porosity=porosity_IL*(pow(saturation,beta)-pow(satu_0,beta));    
  porosity_IL *=pow(saturation,beta);
  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY_IL"),porosity_IL);
  // Total porosity calculation
  porosity = porosity_n+d_porosity; 
  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY"),porosity); 
  return porosity;
}


/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
05/2005 MX Implementation
last modification:
**************************************************************************/
double CMediumProperties::PorosityEffectiveConstrainedSwelling(long index,double saturation,double temperature, double *porosity_sw)
{

 /*  Soil Properties */

  double mat_mp_m, beta;
  double porosity = 0.0;
  static double theta;
  double porosity_n, porosity_IL, d_porosity, \
                n_total, density_rock, fmon;
//  double n_max, n_min;
  static double S_0;
  /* Component Properties */
  static double  satu, epsilon;
  static double satu_0=0.20;
  static double porosity_min=0.05;
  static double ion_strength;
  static double F_const=96484.6, epsilon_0=8.854e-12;
  static double R=8.314510, psi=1.0;
  /* Fluid properies */
  int phase=1;

  //--------------------------------------------------------------------
  // MMP medium properties
  S_0 =      porosity_model_values[1];      // Specific surface area [m^2/g]
  fmon =     porosity_model_values[2];     // Anteil quelfaehige mineral [-]
  mat_mp_m = porosity_model_values[3]; // Schichtenanzahl eines quellfähigen Partikels [-]
  beta =     porosity_model_values[6];     // modifications coefficient (z.B. free swelling Weimar beta=3.0)
  //--------------------------------------------------------------------
  // MSP solid properties 
  CSolidProperties *m_msp = NULL;
  long group = ElGetElementGroupNumber(index);
  m_msp = msp_vector[group];
  density_rock  = m_msp->Density(1);
  //--------------------------------------------------------------------

  /* Component Properties */
   ion_strength = MATCalcIonicStrengthNew(index);
   if (ion_strength == 0.0){
	  ion_strength = porosity_model_values[4]; /*Ionic strength [M]*/
   }
  satu_0 = porosity_model_values[5];       /*Initial saturation, if the sample is homogenous [-]*/
  porosity_min = porosity_model_values[7]; /*minimal porosity after swelling compaction*/

 //  ion_strength = MATGetSoilIonicStrength(index);

  /* Field v0ariables */
  /*theta = GetNumericalTimeCollocation("TRANSPORT");*/
   theta = 1.0;
  /*  T = MATGetTemperatureGP (index,0.0,0.0,0.0,theta);*/
//  T=298.0;
  phase=1;
  satu = saturation; //MATGetSaturationGP(phase, index, 0.0, 0.0, 0.0, theta); /*only for fluid, phase=1*/ 

  /*-----------------------------------------------------------------------*/
  /* Interlayer Porositaet berechnen */
  if (abs(temperature)>1.0e10) temperature=298.0;  //TODO MX
  epsilon =87.0+exp(-0.00456*(temperature-273.0));
  porosity_n = porosity_model_values[0]; 

    /* Maximal inter layer porosity */    
  porosity_IL=fmon * psi * mat_mp_m * S_0 * (density_rock * 1.0e3) \
           * sqrt(epsilon * epsilon_0 * R * temperature / (2000.0 * F_const * F_const * ion_strength ));
  d_porosity=porosity_IL*(pow(satu, beta)-pow(satu_0, beta));    

/*-----------Interlayer porosity calculation------------------*/    

/*  porosity_IL = porosity_IL*satu; */
  porosity_IL  *=pow(satu,beta);
  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY_IL"),porosity_IL); 
    /* constrained swelling */

/*-----------Effective porosity calculation------------------*/    
/*  n_max=porosity_n;
  n_min=porosity_min;

  porosity = n_max-(n_max-n_min)*fmon*(pow(satu,1.5)); 

  ElSetElementVal(index,poro_start,porosity); 

*/
  porosity = porosity_n-d_porosity; 

  if (porosity<porosity_min) 
	  porosity =porosity_min;

  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY"),porosity);
/*-----------Void ratio for swelling pressure calculation------------------*/ 
//  e = porosity/(1.-porosity);
//  ElSetElementVal(index,PCSGetELEValueIndex("VoidRatio"),e); 
/*-----------Swelling potential calculation------------------*/      
/* constrained swelling */
//  n_total=porosity_n - d_porosity;
  n_total=porosity_n + d_porosity-porosity_min;
 
//  if(n_total>porosity_min)
  if(n_total>=0)
  {  
    *porosity_sw = n_total; 
  }
  else
//    *porosity_sw=-porosity_IL*(satu-satu_0)+(porosity_n-porosity_min); 
    *porosity_sw=d_porosity+(porosity_n-porosity_min); 
  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY_SW"),*porosity_sw);  


/*-----------Swelling pressure calculation------------------*/    
 // MATCalSoilSwellPressMethod0(index);

  return porosity;
}

/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
05/2005 MX Implementation
last modification:
**************************************************************************/
double CMediumProperties::PorosityVolumetricFreeSwelling(long index,double saturation,double temperature)
{
  double mat_mp_m, beta;
  double porosity = 0.0;
  static double theta;
  static double porosity_n, porosity_IL, d_porosity, \
                density_rock, fmon;
  static double S_0;
  static double epsilon;
  static double satu_0=0.20;
  //  static double porosity_min=0.05;
  static double ion_strength;
  static double F_const=96484.6, epsilon_0=8.854e-12;
  static double R=8.314510, psi=1.0;
  theta = 1.0;
  //--------------------------------------------------------------------
  // MMP medium properties
  S_0 =      porosity_model_values[1];      // Specific surface area [m^2/g]
  fmon =     porosity_model_values[2];     // Anteil quelfaehige mineral [-]
  mat_mp_m = porosity_model_values[3]; // Schichtenanzahl eines quellfähigen Partikels [-]
  beta =     porosity_model_values[6];     // modifications coefficient (z.B. free swelling Weimar beta=3.0)
  //--------------------------------------------------------------------
  // MSP solid properties 
  CSolidProperties *m_msp = NULL;
  long group = ElGetElementGroupNumber(index);
  m_msp = msp_vector[group];
  density_rock  = m_msp->Density(1);
  //--------------------------------------------------------------------
  // State properties 
   ion_strength = MATCalcIonicStrengthNew(index);
   if (ion_strength == 0.0){
	 ion_strength = porosity_model_values[4]; // Ionic strength [M]
   }
  satu_0 =       porosity_model_values[5];       // Initial saturation, if the sample is homogenous [-]
  //--------------------------------------------------------------------
  // Interlayer porosity calculation
  epsilon = 87.0 + exp(-0.00456*(temperature-273.0));
  porosity_n = porosity_model_values[0];
  // Maximal inter layer porosity
  porosity_IL = fmon * psi * mat_mp_m * S_0 * (density_rock * 1.0e3) \
              * sqrt(epsilon * epsilon_0 * R * temperature / (2000.0 * F_const * F_const * ion_strength ));
  d_porosity=porosity_IL*(pow(saturation,beta)-pow(satu_0,beta));    
  porosity_IL *=pow(saturation,beta);
  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY_IL"),porosity_IL);
  // Total porosity calculation
  porosity = porosity_n+d_porosity; 
  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY"),porosity); 
  return porosity;
}


/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
05/2005 MX Implementation
last modification:
**************************************************************************/
double CMediumProperties::PorosityEffectiveConstrainedSwellingConstantIonicStrength(long index,double saturation,double temperature, double *porosity_sw)
{

 /*  Soil Properties */

  double mat_mp_m, beta;
  double porosity = 0.0;
  double e;
  static double theta;
  double porosity_n, porosity_IL, d_porosity, \
                n_total, density_rock, fmon;
//  double n_max, n_min;
  static double S_0;
  /* Component Properties */
  static double  satu, epsilon;
  static double satu_0=0.20;
  static double porosity_min=0.05;
  static double ion_strength;
  static double F_const=96484.6, epsilon_0=8.854e-12;
  static double R=8.314510, psi=1.0;
  /* Fluid properies */
  int phase=1;

  //--------------------------------------------------------------------
  // MMP medium properties
  S_0 =      porosity_model_values[1];      // Specific surface area [m^2/g]
  fmon =     porosity_model_values[2];     // Anteil quelfaehige mineral [-]
  mat_mp_m = porosity_model_values[3]; // Schichtenanzahl eines quellfähigen Partikels [-]
  beta =     porosity_model_values[6];     // modifications coefficient (z.B. free swelling Weimar beta=3.0)
  //--------------------------------------------------------------------
  // MSP solid properties 
  CSolidProperties *m_msp = NULL;
  long group = ElGetElementGroupNumber(index);
  m_msp = msp_vector[group];
  density_rock  = m_msp->Density(1);
  //--------------------------------------------------------------------

  /* Component Properties */
  ion_strength = porosity_model_values[4]; /*Ionic strength [M]*/
  satu_0 = porosity_model_values[5];       /*Initial saturation, if the sample is homogenous [-]*/
  porosity_min = porosity_model_values[7]; /*minimal porosity after swelling compaction*/

 //  ion_strength = MATGetSoilIonicStrength(index);

  /* Field v0ariables */
  /*theta = GetNumericalTimeCollocation("TRANSPORT");*/
   theta = 1.0;
  /*  T = MATGetTemperatureGP (index,0.0,0.0,0.0,theta);*/
//  T=298.0;
  phase=1;
  satu = saturation; //MATGetSaturationGP(phase, index, 0.0, 0.0, 0.0, theta); /*only for fluid, phase=1*/ 

  /*-----------------------------------------------------------------------*/
  /* Interlayer Porositaet berechnen */
  epsilon =87.0+exp(-0.00456*(temperature-273.0));
  porosity_n = porosity_model_values[0]; 

    /* Maximal inter layer porosity */    
  porosity_IL=fmon * psi * mat_mp_m * S_0 * (density_rock * 1.0e3) \
           * sqrt(epsilon * epsilon_0 * R * temperature / (2000.0 * F_const * F_const * ion_strength ));
  d_porosity=porosity_IL*(pow(satu, beta)-pow(satu_0, beta));    

/*-----------Interlayer porosity calculation------------------*/    

/*  porosity_IL = porosity_IL*satu; */
  porosity_IL  *=pow(satu,beta);
  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY_IL"),porosity_IL); 
    /* constrained swelling */

/*-----------Effective porosity calculation------------------*/    
/*  n_max=porosity_n;
  n_min=porosity_min;

  porosity = n_max-(n_max-n_min)*fmon*(pow(satu,1.5)); 

  ElSetElementVal(index,poro_start,porosity); 

*/
  porosity = porosity_n-d_porosity; 

  if (porosity<porosity_min) 
	  porosity =porosity_min;

  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY"),porosity);
/*-----------Void ratio for swelling pressure calculation------------------*/ 
  e = porosity/(1.-porosity);
  ElSetElementVal(index,PCSGetELEValueIndex("VoidRatio"),e); 
/*-----------Swelling potential calculation------------------*/      
/* constrained swelling */
//  n_total=porosity_n - d_porosity;
  n_total=porosity_n + d_porosity-porosity_min;
 
//  if(n_total>porosity_min)
  if(n_total>=0)
  {  
    *porosity_sw = n_total; 
  }
  else
//    *porosity_sw=-porosity_IL*(satu-satu_0)+(porosity_n-porosity_min); 
    *porosity_sw=d_porosity+(porosity_n-porosity_min); 
  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY_SW"),*porosity_sw);  


/*-----------Swelling pressure calculation------------------*/    
 // MATCalSoilSwellPressMethod0(index);

  return porosity;
}


/**************************************************************************
FEMLib-Method: PorosityVolumetricChemicalReaction
Task: Porosity variation owing to chemical reactions
Programing:
05/2005 MX Implementation
last modification:
**************************************************************************/
double CMediumProperties::PorosityVolumetricChemicalReaction(long index)
{
  int n=0, i, timelevel, nn=0, m=0;
  static long *element_nodes;
//  long component;
  double porosity=0.0, tot_mineral_volume=0.0, tot_mineral_volume0=0.0;
  double conc[100];
  double mineral_volume[100],molar_volume[100];
//  double *ergebnis=NULL;
 // REACTION_MODEL *rcml=NULL;

//  rcml=GETReactionModel(index);
  REACT *rc = NULL; //SB
  rc->GetREACT();

 /*  MMP Medium Properties  */
  porosity = porosity_model_values[0];
  
//  m=rcml->number_of_equi_phases;
  m = rc->rcml_number_of_equi_phases;
  if (m ==0 || ElGetElement(index)==NULL) /* wenn solid phases and Element existiert */
  return porosity;

/* mineral molar volume abholen */
  for (i=0; i<m; i++){
    molar_volume[i]=porosity_model_values[i+1];  
  }

  tot_mineral_volume0=porosity_model_values[m+1]; /*initial total volume of the minerals*/

//  if (!rcml) {return porosity;}

/* calculate the concentrations of each solid phases (in mol/l soil) as element value from adjecent node values */
//  n=rcml->number_of_master_species;
  n = rc->rcml_number_of_master_species;
//  n = get_rcml_number_of_master_species(rcml);
  for (int component=0; component<m+2+n+rc->rcml_number_of_ion_exchanges; component++) {
	conc[component] =0.0;
	// Not used: CompProperties *m_cp = cp_vec[component];
 //    int z_i = m_cp->valence;
 //	 m_cp->compname; //What's this for
    if (component>=n+2 && component<n+2+m){

      if (ElGetElementActiveState(index)){  
         nn = ElGetElementNodesNumber(index);
         element_nodes = ElGetElementNodes(index);
         for (int j=0;j<nn;j++) {
            timelevel=1;
			conc[component] += PCSGetNODConcentration(element_nodes[j],component,timelevel);
		}
        conc[component] /= (double)nn;
        element_nodes = NULL;
      } 

/* calculate the solid phase: volume =v_mi*Ci */
      timelevel=0;
//      conc[i]=CalcElementMeanConcentration (index, i, timelevel, ergebnis);
      mineral_volume[component-n-2] = conc[component]*molar_volume[component-n-2];
      tot_mineral_volume += mineral_volume[component-n-2];
    }
  } /*for */


  porosity += tot_mineral_volume0 - tot_mineral_volume;
//  ElSetElementVal(index,"POROSITY",porosity);  
  ElSetElementVal(index,PCSGetELEValueIndex("POROSITY"),porosity);
  return porosity;
}

//------------------------------------------------------------------------
//4..TORTUOSITY
//------------------------------------------------------------------------
double CMediumProperties::TortuosityFunction(long number, double *gp, double theta,CFiniteElementStd* assem)
{
  static int nidx0,nidx1;
  double primary_variable[10];		//OK To Do
  int count_nodes;
  long* element_nodes = NULL;
//  int fct_number = 0;
//  int gueltig;
  //----------------------------------------------------------------------
  int i;
  int no_pcs_names =(int)pcs_name_vector.size();
  for(i=0;i<no_pcs_names;i++){
    nidx0 = PCSGetNODValueIndex(pcs_name_vector[i],0);
    nidx1 = PCSGetNODValueIndex(pcs_name_vector[i],1);
    if(mode==0){ // Gauss point values
      primary_variable[i] = (1.-theta)*InterpolValue(number,nidx0,gp[0],gp[1],gp[2]) \
                          + theta*InterpolValue(number,nidx1,gp[0],gp[1],gp[2]);
    }
    else if(mode==1){ // Node values
      primary_variable[i] = (1.-theta)*GetNodeVal(number,nidx0) \
                          + theta*GetNodeVal(number,nidx1);
    }
    else if(mode==2){ // Element average value
      count_nodes = ElNumberOfNodes[ElGetElementType(number) - 1];
      element_nodes = ElGetElementNodes(number);
      for (i = 0; i < count_nodes; i++)
	    primary_variable[i] += GetNodeVal(element_nodes[i],nidx1);
      primary_variable[i]/= count_nodes;
    }
  } //For Loop

 
    switch (tortuosity_model) {

		case 0:                     /* Tortuosity is read from a curve */
        //To do
        break;

		case 1:                      /* Constant Tortuosity*/
		tortuosity = tortuosity_model_values[0];
        break;
		
        default:
		DisplayMsgLn("Unknown tortuosisty model!");
        break;
		}	                         /* switch */

return (tortuosity);
}





/* 4B.4.3 non-linear flow */
/**************************************************************************/
/* ROCKFLOW - Funktion: NonlinearFlowFunction
 */
/* Aufgabe:
   Berechnung der relativen Permeabilitaet
   fuer nichtlineare Fliessgesetze
 */
/* Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
 */
/* Ergebnis:
   0 bei Fehler, sonst 1 (dient nur zum Abbrechen der Funktion)
 */
/* Programmaenderungen:
   06/1999   OK   Implementierung
   09/2004   CMCD In GeoSys 4
 */
/**************************************************************************/
double CMediumProperties::NonlinearFlowFunction(long index, double *gp, double theta)
{    
	//Pressure variable name (PRESSURE 1) not used as yet in this function	
	int i;
	//Pointer to fluid properties
//	int no_phases =(int)mfp_vector.size();
	CFluidProperties *m_mfp = NULL;
	int phase = (int)mfp_vector.size()-1;
	m_mfp = mfp_vector[phase];
    /* Knotendaten */
    int nn,Type;
    long *element_nodes;
    double p_element_node[4], h_element_node[4], z_element_node[4];
    /* Materialdaten */
    double alpha;
    double k_rel = 1.0;
    double g, rho, mu;
    /* Elementdaten */
    double grad_h[2],grad_x,grad_y;
    double mult[2];
    double detjac, *invjac, invjac2d[4];
    double grad_omega[8];
    double grad_h_min = MKleinsteZahl;  /*1.0; */
	double xgt[3],ygt[3],zgt[3];				//CMCD Global x,y,z coordinates of traingular element
	double xt[3],yt[3];							//CMCD Local x,y coordinates of traingular element
	double pt_element_node[4], ht_element_node[4], zt_element_node[4]; //CMCD Pressure, depth head traingular element
	double dN_dx[3],dN_dy[3], area;				//CMCD Shape function derivates for triangles
	double isotropicgradient,porosity, Re, Lambda, apperture, hyd_radius, perm;
	double linear_q,turbulent_q,linear_grad, turbulent_grad, flow_rate, temp;
	double dircos[6];							//CMCD 04 2004
    gp[0]= gp[1] = gp[2] = 0.0;					//Gaus points, triangular interpretation value not relevant
  
	element_nodes = ElGetElementNodes(index);
    g = gravity_constant;
    //OK4104 theta = GetNumericalTimeCollocation("PRESSURE1");
	Type = ElGetElementType(index);
  //--------------------------------------------------------------------
  // MMP medium properties
  CMediumProperties *m_mmp = NULL;
  long group = ElGetElementGroupNumber(index);
  m_mmp = mmp_vector[group];
  //--------------------------------------------------------------------
	switch (flowlinearity_model){
	case 1://Element Type Dependent
	alpha = flowlinearity_model_values[0];
	rho = m_mfp->Density();
	
		switch (Type){
		case 1:
			nn = ElNumberOfNodes[0];
			for (i = 0; i < nn; i++) {
				p_element_node[i] = GetNodeVal(element_nodes[i],1);
				z_element_node[i] = GetNode(element_nodes[i])->z;
				h_element_node[i] = (p_element_node[i]) / (g * rho) + z_element_node[i];
			}
			invjac = GEOGetELEJacobianMatrix(index, &detjac);
	/*          if(fabs(h_element_node[1]-h_element_node[0])>MKleinsteZahl) */
			if (fabs(h_element_node[1] - h_element_node[0]) > grad_h_min) {
				k_rel = \
					pow(fabs(0.5 * sqrt(MSkalarprodukt(invjac, invjac, 3))), (alpha - 1.0)) * \
					pow(fabs(h_element_node[1] - h_element_node[0]), (alpha - 1.0));
				if (k_rel > 1)
					k_rel = 1.;
			} else
				k_rel = 1.0;

			break;

		case 2:
			nn = ElNumberOfNodes[1];        /* Knotenanzahl nn muss 4 sein ! */
			for (i = 0; i < nn; i++) {
				p_element_node[i] = GetNodeVal(element_nodes[i],1);
				z_element_node[i] = GetNode(element_nodes[i])->z;
				h_element_node[i] = p_element_node[i] / (g * rho) + z_element_node[i];
			}
			Calc2DElementJacobiMatrix(index, 0.0, 0.0, invjac2d, &detjac);
			MGradOmega2D(grad_omega, 0, 0);         /* Gradientenmatrix */
			MMultMatVec(grad_omega, 2, 4, h_element_node, 4, mult, 2);
			MMultVecMat(mult, 2, invjac2d, 2, 2, grad_h, 2);
	/*          if( (fabs(grad_h[0])>MKleinsteZahl)||(fabs(grad_h[1])>MKleinsteZahl) ) ) */
			if ((fabs(grad_h[0]) > grad_h_min) || (fabs(grad_h[1]) > grad_h_min)) {
				k_rel = \
					pow(fabs(sqrt((grad_h[0]) * (grad_h[0]) + (grad_h[1]) * (grad_h[1]))), \
						(alpha - 1.0));
				/* DisplayDouble(k_rel_iteration,0,0);  DisplayMsgLn(""); */
			} else {
				k_rel = 1.0;
			}
			break;
		case 3:
			k_rel = 1.;
			break;
		}
		
	case 2://Equivalent Fractured Media represented by triangles   CMCD April 2004

			//Geometry
			nn = ElNumberOfNodes[Type - 1];
			element_nodes = ElGetElementNodes(index);	
			
			for (i = 0; i < nn; i++)
				{
				xgt[i] = GetNodeX(element_nodes[i]);
				ygt[i] = GetNodeY(element_nodes[i]);
				zgt[i] = GetNodeZ(element_nodes[i]);
				}
			//Input parameters
			porosity = CMediumProperties::Porosity(index,gp,theta);
			alpha = flowlinearity_model_values[0];
			apperture = porosity / flowlinearity_model_values[1]; /*Change equivalent porosity to individual fracture porosity */
			Re = flowlinearity_model_values[2];
			
			//Fluid properties
			rho = m_mfp->Density();
			mu  = m_mfp->Viscosity();
			Re = 0.0; //Reynolds number for turbulent flow CMCD 04. 2004 
			Lambda = 0.0; //Frictional Resistence

			//Flow status
			hyd_radius = 2*apperture;
			perm = (apperture * apperture)/12.;
			linear_q = (Re*mu)/(hyd_radius*rho); //max linear q
			
			/*Fluid pressure at each node*/ 
			for (i = 0; i < nn; i++) {
				pt_element_node[i] = GetNodeVal(element_nodes[i],1);
				zt_element_node[i] = GetNode(element_nodes[i])->z;
				ht_element_node[i] = pt_element_node[i] + ((g * rho) * zt_element_node[i]); //In Pascal
			}
			Calc2DElementCoordinatesTriangle(index,xt,yt,dircos); /*CMCD included 03/2004*/
			area = ElGetElementVolume(index)/m_mmp->geo_area; //CMCD March 2004 removed wrong area in  code */ 
			//Shape function derivatives
			dN_dx[0] = (yt[1] - yt[2]) / (2. * area);
			dN_dx[1] = (yt[2] - yt[0]) / (2. * area);
			dN_dx[2] = (yt[0] - yt[1]) / (2. * area);
			dN_dy[0] = (xt[2] - xt[1]) / (2. * area);
			dN_dy[1] = (xt[0] - xt[2]) / (2. * area);
			dN_dy[2] = (xt[1] - xt[0]) / (2. * area);
			grad_x = MSkalarprodukt(dN_dx, ht_element_node, nn);
			grad_y = MSkalarprodukt(dN_dy, ht_element_node, nn);
			//v2[0] = MSkalarprodukt(dN_dx, zg, nn);
			//v2[1] = MSkalarprodukt(dN_dy, zg, nn);
			//Assume isotropic nonlinear flow (p268 Kolditz 2001)
			linear_q = linear_q/3.0; // Here the whole element is considered hence 4* to remove avereaging effects
			isotropicgradient = pow((grad_x*grad_x+grad_y*grad_y),0.5);
			flow_rate = (perm * isotropicgradient)/mu; 
			if (flow_rate > linear_q){ 
				turbulent_q = flow_rate-linear_q;
				linear_grad = (linear_q *apperture * mu)/perm;
				turbulent_grad = isotropicgradient - linear_grad;
				temp = pow((turbulent_grad/(rho*g)),1-alpha)/(turbulent_grad/(rho*g));
				k_rel = ((linear_grad*1.0) +(turbulent_grad*temp))/isotropicgradient;
			}
			else {
				k_rel = 1.0;
			}

			//velovec[0] = (-k_x * k_rel_grad_p * k_rel_S*k_rel / mu)* (v1[0] + (rho * g * v2[0]));
			//velovec[1] = (-k_y * k_rel_grad_p * k_rel_S*k_rel / mu) * (v1[1] + (rho * g * v2[1]));

	/* special stop CMCD*/		

	//		if (index == 7021){
	//			printf("\n");
	//			printf("Element 4516 k_rel = %g\n",k_rel);
	//			}	
	//
	//		break;
		}
	return k_rel;
	}



/**************************************************************************
 9 Storage 
**************************************************************************
 ROCKFLOW - Funktion: Storage Function

 Aufgabe:
   Berechnet Speicherkoeffizienten

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E long index: Elementnummer

 Ergebnis:
   rel. Permeabilitaet

 Programmaenderungen:
   1/2001   C.Thorenz   Erste Version
   7/2007   C.Thorenz   Div. Druckabhaengigkeiten
  06/2003 OK/CM case 4: Storage as function of effective stress read from curve
  11/2003   CMCD		Geothermal methods added (20)

  Case overview
  0		Curve
  1		Constant
  2		Funktion der effektiven Spannung und des Drucks in Elementmitte
  3		Funktion der effektiven Spannung und des Drucks in Elementmitte ueber Kurve 
  4		Storage as function of effective stress read from curve
  5     Storage as normal stress in element in stress field defined by KTB stress field.
  6		Storage as normal stress in element in stress field defined by KTB stress
                field, function to increase storage with distance from borehole.
**************************************************************************/
double CMediumProperties::StorageFunction(long index,double *gp,double theta)
{
    int nn, i, idummy,Type;
    double p, sigma, z[8];
	int phase;
    double density_solid, stress_eff,S;
    double coords[3];
	double znodes[8],ynodes[8],xnodes[8];
	double zelnodes[8],yelnodes[8],xelnodes[8];
	double Pie,angle;
	double sigma1,sigma2,sigma3;
	double a1,a2,a3,b1,b2,b3;
	double normx,normy,normz,normlen;
	double dircosl, dircosm, dircosn;
    double tot_norm_stress, eff_norm_stress;
	int material_group;
	double x_mid, y_mid, z_mid, x_bore, y_bore, z_bore, distance;
	dircosl = dircosm = dircosn = 0.0;//Initialise variable

	static int nidx0,nidx1;
	double primary_variable[10];		//OK To Do
	int count_nodes;
	long* element_nodes = NULL;
//	int fct_number = 0;
//	int gueltig;

  
	int no_pcs_names =(int)pcs_name_vector.size();
	for(i=0;i<no_pcs_names;i++){
		nidx0 = PCSGetNODValueIndex(pcs_name_vector[i],0);
		nidx1 = PCSGetNODValueIndex(pcs_name_vector[i],1);
		if(mode==0){ // Gauss point values
		primary_variable[i] = (1.-theta)*InterpolValue(index,nidx0,gp[0],gp[1],gp[2]) \
							+ theta*InterpolValue(index,nidx1,gp[0],gp[1],gp[2]);		}
		else if(mode==1){ // Node values
		primary_variable[i] = (1.-theta)*GetNodeVal(index,nidx0) \
							+ theta*GetNodeVal(index,nidx1);		}
		else if(mode==2){ // Element average value
//MX		count_nodes = ElNumberOfNodes[ElGetElementType(number) - 1];
		count_nodes = ElNumberOfNodes[ElGetElementType(index) - 1];
//MX		element_nodes = ElGetElementNodes(number);
		element_nodes = ElGetElementNodes(index);
		for (i = 0; i < count_nodes; i++)
			primary_variable[i] += GetNodeVal(element_nodes[i],nidx1);
		primary_variable[i]/= count_nodes;
		}
	}





    switch (storage_model) {

    case 0:
 
        storage = GetCurveValue((int) storage_model_values[0], 0, primary_variable[0], &idummy);

        break;


    case 1:
        /* Konstanter Wert */
        storage = storage_model_values[0];
        break;

    case 2:
        /* Funktion der effektiven Spannung und des Drucks in Elementmitte */

        /* Den Druck holen */
 		p = primary_variable[0];

        /* Mittlere Tiefe */
        nn = ElNumberOfNodes[ElGetElementType(index) - 1];
        element_nodes = ElGetElementNodes(index);

        for (i = 0; i < nn; i++)
            z[i] = GetNodeZ(element_nodes[i]);

        /* Spannung = sigma(z0) + d_sigma/d_z*z */
        //OKsigma = storage_model_values[2] + storage_model_values[3] * InterpolValueVector(index, z, 0., 0., 0.);
        sigma = storage_model_values[2] + storage_model_values[3] * InterpolValueVector(ElGetElementType(index), z, 0., 0., 0.);

        /* Auf effektive Spannung umrechnen */
        sigma -= p;

        storage = exp(storage_model_values[0] - storage_model_values[1] * log(sigma));
        break;

    case 3:
        /* Funktion der effektiven Spannung und des Drucks in Elementmitte
                   ueber Kurve */

        /* Den Druck holen */
		p = primary_variable[0];

        /* Mittlere Tiefe */
        nn = ElNumberOfNodes[ElGetElementType(index) - 1];
        element_nodes = ElGetElementNodes(index);

        for (i = 0; i < nn; i++)
            z[i] = GetNodeZ(element_nodes[i]);

        /* Spannung = sigma(z0) + d_sigma/d_z*z */
        sigma = storage_model_values[1] + storage_model_values[2] * InterpolValueVector(ElGetElementType(index), z, 0., 0., 0.);

        /* Auf effektive Spannung umrechnen */
        sigma -= p;

        storage = GetCurveValue((int)storage_model_values[0],0,sigma,&i);
        break;

    case 4: /* McD Storage as function of effective stress read from curve */
        
		CalculateSimpleMiddelPointElement(index, coords);
        p = primary_variable[0];
	    density_solid = storage_model_values[2];
		stress_eff = (fabs(coords[2])*gravity_constant*density_solid) - p;
        storage =GetCurveValue((int) storage_model_values[0], 0, stress_eff, &idummy);
		break;
		
	case 5: /* Stroage : Normal stress calculated according to the orientation of the fracture element*/
        
		Type=ElGetElementType(index);
		material_group = ElGetElementGroupNumber(index);
		
			if (Type == 2||Type == 3||Type == 4)  /*Function defined for square, triangular and cubic elements*/
			{
				nn = ElNumberOfNodes[Type - 1];
				element_nodes = ElGetElementNodes(index);	
			
				

				/* Calculate directional cosins, note that this is currently set up*/
				/* Sigma1 is in the y direction*/
				/* Sigma2 is in the z direction*/
				/* Sigma3 is in the x direction*/
				/* This correspondes approximately to the KTB site conditions*/

			for (i=0;i<nn;i++)
			{
				zelnodes[i]=GetNodeZ(element_nodes[i]);
				yelnodes[i]=GetNodeY(element_nodes[i]);
				xelnodes[i]=GetNodeX(element_nodes[i]);
			}
			

			/*Coordinate transformation to match sigma max direction*/
			/* y direction matches the north direction*/

			Pie=3.141592654;
			angle=(storage_model_values[3]*Pie)/180.;
			for (i=0;i<nn;i++)
			{
				znodes[i]=zelnodes[i];
				xnodes[i]=xelnodes[i]*cos(angle)-yelnodes[i]*sin(angle);
				ynodes[i]=xelnodes[i]*sin(angle)+yelnodes[i]*cos(angle);
				
			}

				if (Type == 2) /*Square*/
				{
				a1=xnodes[2]-xnodes[0];
				a2=ynodes[2]-ynodes[0];
				a3=znodes[2]-znodes[0];
				b1=xnodes[3]-xnodes[1];
				b2=ynodes[3]-ynodes[1];
				b3=znodes[3]-znodes[1];
				normx=a2*b3-a3*b2;
				normy=a3*b1-a1*b3;
				normz=a1*b2-a2*b1;
				normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
				dircosl= normy/normlen;
				dircosm= normz/normlen;
				dircosn= normx/normlen;
				}	
			
				if (Type == 3) /*Cube*/
				{
				a1=xnodes[2]-xnodes[0];
				a2=ynodes[2]-ynodes[0];
				a3=znodes[2]-znodes[0];
				b1=xnodes[3]-xnodes[1];
				b2=ynodes[3]-ynodes[1];
				b3=znodes[3]-znodes[1];
				normx=a2*b3-a3*b2;
				normy=a3*b1-a1*b3;
				normz=a1*b2-a2*b1;
				normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
				dircosl= normy/normlen;
				dircosm= normz/normlen;
				dircosn= normx/normlen;
				}	
			
				if (Type == 4) /*Triangle*/
				{
				a1=xnodes[1]-xnodes[0];
				a2=ynodes[1]-ynodes[0];
				a3=znodes[1]-znodes[0];
				b1=xnodes[2]-xnodes[1];
				b2=ynodes[2]-ynodes[1];
				b3=znodes[2]-znodes[1];
				normx=a2*b3-a3*b2;
				normy=a3*b1-a1*b3;
				normz=a1*b2-a2*b1;
				normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
				dircosl= normy/normlen;
				dircosm= normz/normlen;
				dircosn= normx/normlen;
				}	
				/* Calculate average location of the element */	
				CalculateSimpleMiddelPointElement(index, coords);
				x_mid = coords[0];
				y_mid = coords[1];
				z_mid = coords[2];

				p = primary_variable[0];
			

			/*Calcualtion of stress according to Ito & Zoback 2000 for KTB hole*/
					
				sigma1=z_mid*0.045*-1e6;
				sigma2=z_mid*0.028*-1e6;
				sigma3=z_mid*0.02*-1e6;

                        ///Calculate total normal stress on element
			/*Note in this case sigma2 corresponds to the vertical stress*/
				tot_norm_stress=sigma1*dircosl*dircosl+sigma2*dircosm*dircosm+sigma3*dircosn*dircosn;
			
			
				
			/*Calculate normal effective stress*/
				eff_norm_stress=tot_norm_stress-p;
		
				/* special stop CMCD*/		
				if (eff_norm_stress>220000000.){
				phase=0;
				/* Stop here*/
				}			


			/*Take value of storage from a curve*/
				S=GetCurveValue((int) storage_model_values[0], 0, eff_norm_stress, &idummy);
			}
		
		/* default value if element type is not included in the method for calculating the normal */
		else S=storage_model_values[2];
		storage = S;
		break;



	case 6: /* Normal stress calculated according to the orientation of the fracture element*/
        
		Type=ElGetElementType(index);
		material_group = ElGetElementGroupNumber(index);
		
		if (material_group == 0)
		{
			if (Type == 2||Type == 3||Type == 4)  /*Function defined for square, triangular and cubic elements*/
			{
				nn = ElNumberOfNodes[Type - 1];
				element_nodes = ElGetElementNodes(index);	

				/* Calculate directional cosins, note that this is currently set up*/
				/* Sigma1 is in the y direction*/
				/* Sigma2 is in the z direction*/
				/* Sigma3 is in the x direction*/
				/* This correspondes approximately to the KTB site conditions*/
				for (i=0;i<nn;i++)
				{
					zelnodes[i]=GetNodeZ(element_nodes[i]);
					yelnodes[i]=GetNodeY(element_nodes[i]);
					xelnodes[i]=GetNodeX(element_nodes[i]);
				}
				

				/*Coordinate transformation to match sigma max direction*/
				/* y direction matches the north direction*/

				Pie=3.141592654;
				angle=(storage_model_values[3]*Pie)/180.;
				for (i=0;i<nn;i++)
				{
					znodes[i]=zelnodes[i];
					xnodes[i]=xelnodes[i]*cos(angle)-yelnodes[i]*sin(angle);
					ynodes[i]=xelnodes[i]*sin(angle)+yelnodes[i]*cos(angle);
					
				}

				if (Type == 2) /*Square*/
				{
				a1=xnodes[2]-xnodes[0];
				a2=ynodes[2]-ynodes[0];
				a3=znodes[2]-znodes[0];
				b1=xnodes[3]-xnodes[1];
				b2=ynodes[3]-ynodes[1];
				b3=znodes[3]-znodes[1];
				normx=a2*b3-a3*b2;
				normy=a3*b1-a1*b3;
				normz=a1*b2-a2*b1;
				normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
				dircosl= normy/normlen;
				dircosm= normz/normlen;
				dircosn= normx/normlen;
				}	
			
				if (Type == 3) /*Cube*/
				{
				a1=xnodes[2]-xnodes[0];
				a2=ynodes[2]-ynodes[0];
				a3=znodes[2]-znodes[0];
				b1=xnodes[3]-xnodes[1];
				b2=ynodes[3]-ynodes[1];
				b3=znodes[3]-znodes[1];
				normx=a2*b3-a3*b2;
				normy=a3*b1-a1*b3;
				normz=a1*b2-a2*b1;
				normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
				dircosl= normy/normlen;
				dircosm= normz/normlen;
				dircosn= normx/normlen;
				}	
			
				if (Type == 4) /*Triangle*/
				{
				a1=xnodes[1]-xnodes[0];
				a2=ynodes[1]-ynodes[0];
				a3=znodes[1]-znodes[0];
				b1=xnodes[2]-xnodes[1];
				b2=ynodes[2]-ynodes[1];
				b3=znodes[2]-znodes[1];
				normx=a2*b3-a3*b2;
				normy=a3*b1-a1*b3;
				normz=a1*b2-a2*b1;
				normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
				dircosl= normy/normlen;
				dircosm= normz/normlen;
				dircosn= normx/normlen;
				}	
			
			/* Calculation of average location of element*/
			
			CalculateSimpleMiddelPointElement(index,coords);
			x_mid = coords[0];
			y_mid = coords[1];
			z_mid = coords[2];

			p = primary_variable[0];

			x_bore = storage_model_values[4];
			y_bore = storage_model_values[5];
			z_bore = storage_model_values[6];

			distance = pow(pow((x_mid-x_bore),2.0) + pow((y_mid-y_bore),2.0) + pow((z_mid-z_bore),2.0),0.5);

			

			/*Calcualtion of stress according to Ito & Zoback 2000 for KTB hole*/
					
			sigma1=z_mid*0.045*-1e6;
			sigma2=z_mid*0.028*-1e6;
			sigma3=z_mid*0.02*-1e6;

			///Calculate total normal stress on element
			/*Note in this case sigma2 corresponds to the vertical stress*/
			tot_norm_stress=sigma1*dircosl*dircosl+sigma2*dircosm*dircosm+sigma3*dircosn*dircosn;
			
					
			/*Calculate normal effective stress*/
			eff_norm_stress=tot_norm_stress-p;

		
			/*Take value of storage from a curve*/
			S=GetCurveValue((int) storage_model_values[0], 0, eff_norm_stress, &idummy);
			if (distance > storage_model_values[7]){
				distance = storage_model_values[7];
				}
			if (distance > 2) S = S + (S * (distance-2) * storage_model_values[8]);
			}	

		else S=storage_model_values[2];
		}
		
		else S=storage_model_values[2];
		storage = S;
		break;    

	default:
        storage = 0.0;//OK DisplayMsgLn("The requested storativity model is unknown!!!");
 	break;
    }
	return storage;
}

/**************************************************************************
 11 Permeability
**************************************************************************
FEMLib-Method:
Task: Master calc function
Programing:
08/2004 OK MMP implementation 
           based on GetPermeabilityTensor by OK
10/2004 SB adapted to het_file
last modification:

**************************************************************************/
double* CMediumProperties::PermeabilityTensor(long index)
{
  static double tensor[9];
  switch(geo_dimension){
    case 1: // 1-D
        tensor[0] = permeability_tensor[0];
		if(permeability_file.size() > 0) tensor[0] = GetHetValue(index,"permeability");
      break;
    case 2: // 2-D
      if(permeability_tensor_type==0){
        tensor[0] = permeability_tensor[0];
        tensor[1] = 0.0;
        tensor[2] = 0.0;
        tensor[3] = permeability_tensor[0];
		if(permeability_file.size() > 0) {
			tensor[0] = GetHetValue(index,"permeability");
			tensor[3] = tensor[0];
		}
      }
      else if(permeability_tensor_type==1){
        tensor[0] = permeability_tensor[0];
        tensor[1] = 0.0;
        tensor[2] = 0.0;
        tensor[3] = permeability_tensor[1];
      }
      else if(permeability_tensor_type==2){
        tensor[0] = permeability_tensor[0];
        tensor[1] = permeability_tensor[1];
        tensor[2] = permeability_tensor[2];
        tensor[3] = permeability_tensor[3];
      }
      break;
    case 3: // 3-D
      if(permeability_tensor_type==0){
        tensor[0] = permeability_tensor[0];
        tensor[1] = 0.0;
        tensor[2] = 0.0;
        tensor[3] = 0.0;
        tensor[4] = permeability_tensor[0];
        tensor[5] = 0.0;
        tensor[6] = 0.0;
        tensor[7] = 0.0;
        tensor[8] = permeability_tensor[0];
		if(permeability_file.size() > 0) {
			tensor[0] = GetHetValue(index,"permeability");
			tensor[4] = tensor[0];
			tensor[8] = tensor[0];
		}
      }
      else if(permeability_tensor_type==1){
        tensor[0] = permeability_tensor[0];
        tensor[1] = 0.0;
        tensor[2] = 0.0;
        tensor[3] = 0.0;
        tensor[4] = permeability_tensor[1];
        tensor[5] = 0.0;
        tensor[6] = 0.0;
        tensor[7] = 0.0;
        tensor[8] = permeability_tensor[2];
      }
      else if(permeability_tensor_type==2){
        tensor[0] = permeability_tensor[0];
        tensor[1] = permeability_tensor[1];
        tensor[2] = permeability_tensor[2];
        tensor[3] = permeability_tensor[3];
        tensor[4] = permeability_tensor[4];
        tensor[5] = permeability_tensor[5];
        tensor[6] = permeability_tensor[6];
        tensor[7] = permeability_tensor[7];
        tensor[8] = permeability_tensor[8];
      }
      break;
  }
  return tensor;
}
//------------------------------------------------------------------------
//12.(i) PERMEABILITY_FUNCTION_DEFORMATION
//------------------------------------------------------------------------
//
//------------------------------------------------------------------------
//12.(ii) PERMEABILITY_FUNCTION_PRESSURE
//------------------------------------------------------------------------
double CMediumProperties::PermeabilityPressureFunction(long index,double *gp,double theta)
{
    int nn, i, idummy,p_idx1;
    long *element_nodes;
    static double gh,  p, eins_durch_rho_g, sigma, z[8], h[8], grad_h[3];
    static double invjac[8], detjac, grad_omega[8];
    static double mult[3];
    double x_mid,y_mid,z_mid,coords[3];
    double density_solid, stress_eff;
//	double porosity, factora, factorb,valuelogk;
	double k_rel=0.0;
    CFluidProperties* m_mfp=NULL;

	//Collect primary variables
	static int nidx0,nidx1;
	double primary_variable[10];		//OK To Do
	int count_nodes;
	int no_pcs_names =(int)pcs_name_vector.size();
	for(i=0;i<no_pcs_names;i++){
		nidx0 = PCSGetNODValueIndex(pcs_name_vector[i],0);
		nidx1 = PCSGetNODValueIndex(pcs_name_vector[i],1);
		if(mode==0){ // Gauss point values
		primary_variable[i] = (1.-theta)*InterpolValue(number,nidx0,gp[0],gp[1],gp[2]) \
							+ theta*InterpolValue(number,nidx1,gp[0],gp[1],gp[2]);
		}
		else if(mode==1){ // Node values
		primary_variable[i] = (1.-theta)*GetNodeVal(number,nidx0) \
							+ theta*GetNodeVal(number,nidx1);
		}
		else if(mode==2){ // Element average value
		count_nodes = ElNumberOfNodes[ElGetElementType(number) - 1];
		element_nodes = ElGetElementNodes(number);
		for (i = 0; i < count_nodes; i++)
			primary_variable[i] += GetNodeVal(element_nodes[i],nidx1);
		primary_variable[i]/= count_nodes;
		}
	}
    switch (permeability_pressure_model) {
    case 0://Curve function
        k_rel = 1.0;
        break;
    case 1://No functional dependence
		k_rel = 1.0;
        break;
        /* Funktion der effektiven Spannung */
	case 2:
        /* Den Druck holen */
        p = primary_variable[0];
        /* Mittlere Tiefe */
        nn = ElNumberOfNodes[ElGetElementType(index) - 1];
        element_nodes = ElGetElementNodes(index);
        for (i = 0; i < nn; i++)
            z[i] = GetNodeZ(element_nodes[i]);
        /* Spannung = sigma(z0) + d_sigma/d_z*z */
        sigma = permeability_pressure_model_values[2] + permeability_pressure_model_values[3] * InterpolValueVector(ElGetElementType(index), z, 0., 0., 0.);
        /* Auf effektive Spannung umrechnen */
        sigma -= p;
        k_rel = exp(permeability_pressure_model_values[0] - permeability_pressure_model_values[1] * log(sigma));
        break;
     case 3: /* Turbulentes Fliessen */
        /* k_rel = max(min((grad(h)*alpha1)^(alpha2-1), alpha4),alpha3) */
        m_mfp = MFPGet("LIQUID");
        eins_durch_rho_g = 1./gravity_constant/m_mfp->Density(); // YDGetFluidDensity(0, index, 0., 0., 0., 1.); 
        nn = ElNumberOfNodes[ElGetElementType(index) - 1];
        element_nodes = ElGetElementNodes(index);
		p_idx1 = PCSGetNODValueIndex("PRESSURE1",1);
        for (i=0;i<nn;i++) 
           h[i] = GetNodeVal(element_nodes[i], p_idx1)*eins_durch_rho_g 
                + GetNodeZ(element_nodes[i]);
        switch (ElGetElementType(index)) {
          default: 
            DisplayMsgLn("Error in GetSoilRelPermPress!");
            DisplayMsgLn("  Nonlinear permeability not available!");
            abort();
          case 2:
            Calc2DElementJacobiMatrix(index,0.0,0.0,invjac,&detjac);
            MGradOmega2D(grad_omega,0,0); /* Gradientenmatrix */
            MMultMatVec(grad_omega,2,4,h,4,mult,2);
            MMultVecMat(mult,2,invjac,2,2,grad_h,2);
            gh = sqrt(grad_h[0]*grad_h[0]+grad_h[1]*grad_h[1])*permeability_pressure_model_values[0];
            if(gh<MKleinsteZahl)
               k_rel = 1.0;
            else
               k_rel = max(min(pow(gh,permeability_pressure_model_values[1]-1.0),permeability_pressure_model_values[3]),permeability_pressure_model_values[2]);
        }
     case 4:
        /* Funktion der effektiven Spannung ueber Kurve */
        /* Den Druck holen */
        p = primary_variable[0];
        /* Mittlere Tiefe */
        nn = ElNumberOfNodes[ElGetElementType(index) - 1];
        element_nodes = ElGetElementNodes(index);
        for (i = 0; i < nn; i++)
            z[i] = GetNodeZ(element_nodes[i]);
        /* Spannung = sigma(z0) + d_sigma/d_z*z */
        sigma = permeability_pressure_model_values[1] + permeability_pressure_model_values[2] * InterpolValueVector(ElGetElementType(index), z, 0., 0., 0.);
        /* Auf effektive Spannung umrechnen */
        sigma -= p;
        k_rel = GetCurveValue((int)permeability_pressure_model_values[0], 0, sigma, &i);
        break;
    case 5:
        /* Funktion der effektiven Spannung ueber Kurve CMCD 26.06.2003*/ 
        /* Average depth */
		CalculateSimpleMiddelPointElement(index,coords);
		x_mid = coords[0];
		y_mid = coords[1];
		z_mid = coords[2];
		p = primary_variable[0];
        density_solid = permeability_pressure_model_values[2];
        stress_eff = (fabs(z_mid)*gravity_constant*density_solid) - p;
        k_rel = GetCurveValue((int) permeability_pressure_model_values[0], 0, stress_eff, &idummy);
        break;
	case 6:
		k_rel = PermeabilityPressureFunctionMethod1(index,primary_variable[0]);
		break;
	case 7: 
		k_rel = PermeabilityPressureFunctionMethod2(index,primary_variable[0]);
		break;
	case 8: 
		k_rel = PermeabilityPressureFunctionMethod3(index,primary_variable[0]);
		break;
	case 9: 
		k_rel = PermeabilityPressureFunctionMethod4(index,primary_variable[0], primary_variable[1]);
		break;
	default:					// CMCD Einbau
		k_rel = 1.0;			// CMCD Einbau
		break;					// CMCD Einbau
}
		return k_rel;
}


/**************************************************************************
12.(ii)a Subfunction of Permeability_Function_Pressure
ROCKFLOW - Funktion: MATCalcPressurePermeabilityMethod1
Application to KTB
 Aufgabe:
   Calculates relative permeability from
   the normal stress according to orientation of the fractures in the KTB site system
   converts the normal stress to effective stress, reads from a curve what the permeability
   of a fracture under the given effective stress is.

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: long index: Elementnummer, double primary_variable pressure
   R: relative permeability
 
 Ergebnis:
   rel. Permeabilitaet

 Programmaenderungen:
   09/2004   CMCD Inclusion in GeoSys 4
**************************************************************************/
 
double CMediumProperties::PermeabilityPressureFunctionMethod1(long index,double pressure)
{
	double R,Pie,p;
    double znodes[8],ynodes[8],xnodes[8], angle;
	double x_mid, y_mid, z_mid, coords[3];
	double zelnodes[8],yelnodes[8],xelnodes[8];
	double sigma1,sigma2,sigma3;
	double a1,a2,a3,b1,b2,b3;
	double normx,normy,normz,normlen;
	double dircosl, dircosm, dircosn;
    double tot_norm_stress, eff_norm_stress;
	int material_group,Type;
	int phase,nn,i,idummy;
	long *element_nodes;
	dircosl = dircosm = dircosn =0.0;
		Type=ElGetElementType(index);
		material_group = ElGetElementGroupNumber(index);

		/* special stop CMCD*/		
		if (index == 6590){
			phase=0;
			/* Stop here*/
			}				
				

		if (Type == 2||Type == 3||Type == 4)  /*Function defined for square, triangular and cubic elements*/
		{
			nn = ElNumberOfNodes[Type - 1];
			element_nodes = ElGetElementNodes(index);	
		
 			/* Calculate directional cosins, note that this is currently set up*/
			/* Sigma1 is in the y direction*/
			/* Sigma2 is in the z direction*/
			/* Sigma3 is in the x direction*/
			/* This correspondes approximately to the KTB site conditions*/

			for (i=0;i<nn;i++)
			{
				zelnodes[i]=GetNodeZ(element_nodes[i]);
				yelnodes[i]=GetNodeY(element_nodes[i]);
				xelnodes[i]=GetNodeX(element_nodes[i]);
			}
			

			/*Coordinate transformation to match sigma max direction*/
			/* y direction matches the north direction*/

			Pie=3.141592654;
			for (i=0;i<nn;i++)
			{
				znodes[i]=zelnodes[i];
				angle=(permeability_pressure_model_values[3]*Pie)/180.;
				xnodes[i]=xelnodes[i]*cos(angle)-yelnodes[i]*sin(angle);
				ynodes[i]=xelnodes[i]*sin(angle)+yelnodes[i]*cos(angle);
				
			}


			if (Type == 2) /*Square*/
			{
			a1=xnodes[2]-xnodes[0];
			a2=ynodes[2]-ynodes[0];
			a3=znodes[2]-znodes[0];
			b1=xnodes[3]-xnodes[1];
			b2=ynodes[3]-ynodes[1];
			b3=znodes[3]-znodes[1];
			normx=a2*b3-a3*b2;
			normy=a3*b1-a1*b3;
			normz=a1*b2-a2*b1;
			normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
			dircosl= normy/normlen;
			dircosm= normz/normlen;
			dircosn= normx/normlen;
			}	
		
			if (Type == 3) /*Cube*/
			{
			a1=xnodes[2]-xnodes[0];
			a2=ynodes[2]-ynodes[0];
			a3=znodes[2]-znodes[0];
			b1=xnodes[3]-xnodes[1];
			b2=ynodes[3]-ynodes[1];
			b3=znodes[3]-znodes[1];
			normx=a2*b3-a3*b2;
			normy=a3*b1-a1*b3;
			normz=a1*b2-a2*b1;
			normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
			dircosl= normy/normlen;
			dircosm= normz/normlen;
			dircosn= normx/normlen;
			}	
		
			if (Type == 4) /*Triangle*/
			{
			a1=xnodes[1]-xnodes[0];
			a2=ynodes[1]-ynodes[0];
			a3=znodes[1]-znodes[0];
			b1=xnodes[2]-xnodes[1];
			b2=ynodes[2]-ynodes[1];
			b3=znodes[2]-znodes[1];
			normx=a2*b3-a3*b2;
			normy=a3*b1-a1*b3;
			normz=a1*b2-a2*b1;
			normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
			dircosl= normy/normlen;
			dircosm= normz/normlen;
			dircosn= normx/normlen;
			}	
		
		
		/* Calculation of average location of element*/
		CalculateSimpleMiddelPointElement(index,coords);
		x_mid = coords[0];
		y_mid = coords[1];
		z_mid = coords[2];

		/*Calculate fluid pressure in element*/ 
		p = pressure;
		/*Calcualtion of stress according to Ito & Zoback 2000 for KTB hole*/
				
		sigma1=z_mid*0.045*-1e6;
		sigma2=z_mid*0.028*-1e6;
		sigma3=z_mid*0.02*-1e6;

		/*Calculate total normal stress on element
		Note in this case sigma2 corresponds to the vertical stress*/
		tot_norm_stress=sigma1*dircosl*dircosl+sigma2*dircosm*dircosm+sigma3*dircosn*dircosn;
				
		/*Calculate normal effective stress*/
		eff_norm_stress=tot_norm_stress-p;
			
		/*Take value of storage from a curve*/
		R=GetCurveValue((int) permeability_pressure_model_values[0], 0, eff_norm_stress, &idummy);
		
		return R;

		}
		/* default value if element type is not included in the method for calculating the normal */
		else R=permeability_pressure_model_values[2];

		return R;

		

}

/**************************************************************************
 12.(ii)b Subfunction of Permeability_Function_Pressure
 Function: MATCalcPressurePermeabilityMethod2
 Application to KTB
 Aufgabe:
   Calculates relative permeability from
   the normal stress according to orientation of the fractures in the KTB site system
   converts the normal stress to effective stress, reads from a curve what the permeability
   of a fracture under the given effective stress is. Permeability is then related to
   the distance from the borehole.

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: long index: Elementnummer, double primary_variable pressure
   R: relative permeability
 
 Ergebnis:
   rel. Permeabilitaet

 Programmaenderungen:
   09/2004   CMCD Inclusion in GeoSys 4
**************************************************************************/
 
double CMediumProperties::PermeabilityPressureFunctionMethod2(long index,double pressure)
{
	double R,Pie,p;
    double znodes[8],ynodes[8],xnodes[8], angle;
	double x_mid, y_mid, z_mid, coords[3];
	double zelnodes[8],yelnodes[8],xelnodes[8];
	double sigma1,sigma2,sigma3;
	double a1,a2,a3,b1,b2,b3;
	double normx,normy,normz,normlen;
	double dircosl, dircosm, dircosn;
    double tot_norm_stress, eff_norm_stress;
	int material_group,Type;
	double x_bore, y_bore, z_bore, distance;
	int nn,i,idummy;
	long *element_nodes;
	dircosl = dircosm = dircosn =0.0;
		Type=ElGetElementType(index);
		material_group = ElGetElementGroupNumber(index);

		/* special stop CMCD*/		
		if (index == 4516){
			/* Stop here */
		}				

		if (Type == 2||Type == 3||Type == 4)  /*Function defined for square, triangular and cubic elements*/
		{
			nn = ElNumberOfNodes[Type - 1];
			element_nodes = ElGetElementNodes(index);	


			/* Calculate directional cosins, note that this is currently set up*/
			/* Sigma1 is in the y direction*/
			/* Sigma2 is in the z direction*/
			/* Sigma3 is in the x direction*/
			/* This correspondes approximately to the KTB site conditions*/

			for (i=0;i<nn;i++)
			{
				zelnodes[i]=GetNodeZ(element_nodes[i]);
				yelnodes[i]=GetNodeY(element_nodes[i]);
				xelnodes[i]=GetNodeX(element_nodes[i]);
			}

			
			Pie=3.141592654;
			angle=(permeability_pressure_model_values[3]*Pie)/180.;
			for (i=0;i<nn;i++)
			{
				znodes[i]=zelnodes[i];
				xnodes[i]=xelnodes[i]*cos(angle)-yelnodes[i]*sin(angle);
				ynodes[i]=xelnodes[i]*sin(angle)+yelnodes[i]*cos(angle);
				
			}

			if (Type == 2) /*Square*/
			{
			a1=xnodes[2]-xnodes[0];
			a2=ynodes[2]-ynodes[0];
			a3=znodes[2]-znodes[0];
			b1=xnodes[3]-xnodes[1];
			b2=ynodes[3]-ynodes[1];
			b3=znodes[3]-znodes[1];
			normx=a2*b3-a3*b2;
			normy=a3*b1-a1*b3;
			normz=a1*b2-a2*b1;
			normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
			dircosl= normy/normlen;
			dircosm= normz/normlen;
			dircosn= normx/normlen;
			}	
		
			if (Type == 3) /*Cube*/
			{
			a1=xnodes[2]-xnodes[0];
			a2=ynodes[2]-ynodes[0];
			a3=znodes[2]-znodes[0];
			b1=xnodes[3]-xnodes[1];
			b2=ynodes[3]-ynodes[1];
			b3=znodes[3]-znodes[1];
			normx=a2*b3-a3*b2;
			normy=a3*b1-a1*b3;
			normz=a1*b2-a2*b1;
			normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
			dircosl= normy/normlen;
			dircosm= normz/normlen;
			dircosn= normx/normlen;
			}	
		
			if (Type == 4) /*Triangle*/
			{
			a1=xnodes[1]-xnodes[0];
			a2=ynodes[1]-ynodes[0];
			a3=znodes[1]-znodes[0];
			b1=xnodes[2]-xnodes[1];
			b2=ynodes[2]-ynodes[1];
			b3=znodes[2]-znodes[1];
			normx=a2*b3-a3*b2;
			normy=a3*b1-a1*b3;
			normz=a1*b2-a2*b1;
			normlen=sqrt(pow(normx,2.0)+pow(normy,2.0)+pow(normz,2.0));
			dircosl= normy/normlen;
			dircosm= normz/normlen;
			dircosn= normx/normlen;
			}	
		
		CalculateSimpleMiddelPointElement(index,coords);
		x_mid = coords[0];
		y_mid = coords[1];
		z_mid = coords[2];

		/*Calculate fluid pressure in element*/ 
		p = pressure;

		/*Calculate distance from borehole*/
		x_bore = permeability_pressure_model_values[4];
		y_bore = permeability_pressure_model_values[5];
		z_bore = permeability_pressure_model_values[6];

		distance = pow(pow((x_mid-x_bore),2.0) + pow((y_mid-y_bore),2.0) + pow((z_mid-z_bore),2.0),0.5);

		/*Calcualtion of stress according to Ito & Zoback 2000 for KTB hole*/
				
		sigma1=z_mid*0.045*-1e6;
		sigma2=z_mid*0.028*-1e6;
		sigma3=z_mid*0.02*-1e6;

		///Calculate total normal stress on element
		/*Note in this case sigma2 corresponds to the vertical stress*/
		tot_norm_stress=sigma1*dircosl*dircosl+sigma2*dircosm*dircosm+sigma3*dircosn*dircosn;
				
		/*Calculate normal effective stress*/
		eff_norm_stress=tot_norm_stress-p;
			
		/*Take value of storage from a curve*/
		R=GetCurveValue((int) permeability_pressure_model_values[0], 0, eff_norm_stress, &idummy);
		if (distance > permeability_pressure_model_values[7]){
			distance = permeability_pressure_model_values[7];
	
		}
			if (distance > 2) R = R + (R * (distance-2) * permeability_pressure_model_values[8]);
		
			return R;
		}
		/* default value if element type is not included in the method for calculating the normal */
		else R=permeability_pressure_model_values[2];

		return R;


}


/**************************************************************************
 12.(ii)c Subfunction of Permeability_Function_Pressure
 Function: MATCalcPressurePermeabilityMethod3
 Application to Urach

 Aufgabe:
   The normal stress across the fractures is calculated according to an approximate formulation
   from the relationship of stress with depth. This normal stress is then converted into a permeabilty
   by reference to effective stress and a curve.

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: long index: Elementnummer, double primary_variable pressure
   R: relative permeability
 
 Ergebnis:
   rel. Permeabilitaet

 Programmaenderungen:
   09/2004   CMCD Inclusion in GeoSys 4
**************************************************************************/
 
double CMediumProperties::PermeabilityPressureFunctionMethod3(long index,double pressure)
{

      /* Funktion der effektiven Spannung ueber Kurve CMCD 26.06.2003*/ 
		double x_mid, y_mid, z_mid, coords[3]; 
		double stress_eff,p,perm;
		int idummy;
        /* Average depth */
		CalculateSimpleMiddelPointElement(index,coords);
		x_mid = coords[0];
		y_mid = coords[1];
		z_mid = coords[2];

		/*Calculate fluid pressure in element*/ 
		p = pressure;
	    stress_eff = (fabs(z_mid)*permeability_pressure_model_values[1]*1e6)-p;
		perm = GetCurveValue((int) permeability_pressure_model_values[0], 0, stress_eff, &idummy);

		return perm;

}       

/**************************************************************************
12.(ii)d Subfunction of Permeability_Function_Pressure
 Function: MATCalcPressurePermeabilityMethod4
 Application to Urach

 Aufgabe:
   The normal stress across the fractures is calculated according to an approximate formulation
   from the relationship of stress with depth. This normal stress is then adjusted to take account of 
   thermal cooling, and the resulting effective stress across the fracture isconverted into a permeabilty.
   by reference to a curve.

 Formalparameter: (E: Eingabe; R: Rueckgabe; X: Beides)
   E: long index: Elementnummer, double primary_variable pressure
   R: relative permeability
 
 Ergebnis:
   rel. Permeabilitaet

 Programmaenderungen:
   09/2004   CMCD Inclusion in GeoSys 4
**************************************************************************/
 
double CMediumProperties::PermeabilityPressureFunctionMethod4(long index,double pressure, double temperature)
{

      /* Funktion der effektiven Spannung ueber Kurve CMCD 26.06.2003*/ 
		double x_mid, y_mid, z_mid, coords[3]; 
		double stress_eff,p,perm, thermal_stress;
		int idummy;
        /* Average depth */
		CalculateSimpleMiddelPointElement(index,coords);
		x_mid = coords[0];
		y_mid = coords[1];
		z_mid = coords[2];

		/*Calculate effective stress in the element*/ 
		p = pressure;
	    stress_eff = (fabs(z_mid)*permeability_pressure_model_values[1]*1e6)-p;
		
		/*Impact of thermal stress*/
		thermal_stress = (permeability_pressure_model_values[2]-temperature)*permeability_pressure_model_values[3]*permeability_pressure_model_values[4];

		stress_eff = stress_eff - thermal_stress;
		if (stress_eff < 0.0){
			stress_eff = 0.0;
		}
		/*Read value effective stress against curve*/
		perm = GetCurveValue((int) permeability_pressure_model_values[0], 0, stress_eff, &idummy);
		return perm;

} 

//------------------------------------------------------------------------
//12.(i) PERMEABILITY_FUNCTION_SATURATION
//------------------------------------------------------------------------
//------------------------------------------------------------------------
//12.(vi) PERMEABILITY_FUNCTION_POROSITY
//------------------------------------------------------------------------
double CMediumProperties::PermeabilityPorosityFunction(long number,double *gp,double theta)
{
    int i;
    long *element_nodes;
	double factora, factorb,valuelogk;
	double k_rel=0.0;

	//Collect primary variables
	static int nidx0,nidx1;
	double primary_variable[10];		//OK To Do
	int count_nodes;
	int no_pcs_names =(int)pcs_name_vector.size();
	for(i=0;i<no_pcs_names;i++){
		nidx0 = PCSGetNODValueIndex(pcs_name_vector[i],0);
		nidx1 = PCSGetNODValueIndex(pcs_name_vector[i],1);
		if(mode==0){ // Gauss point values
		primary_variable[i] = (1.-theta)*InterpolValue(number,nidx0,gp[0],gp[1],gp[2]) \
							+ theta*InterpolValue(number,nidx1,gp[0],gp[1],gp[2]);
		}
		else if(mode==1){ // Node values
		primary_variable[i] = (1.-theta)*GetNodeVal(number,nidx0) \
							+ theta*GetNodeVal(number,nidx1);
		}
		else if(mode==2){ // Element average value
		count_nodes = ElNumberOfNodes[ElGetElementType(number) - 1];
		element_nodes = ElGetElementNodes(number);
		for (i = 0; i < count_nodes; i++)
			primary_variable[i] += GetNodeVal(element_nodes[i],nidx1);
		primary_variable[i]/= count_nodes;
		}
	}
    switch (permeability_porosity_model) {
		case 0://Reserved for a curve function
			k_rel=1.0;
			break;
		case 1://Constant value function
			k_rel=1.0;
			break;
		case 2://Ming Lian function
			factora=permeability_porosity_model_values[0];
			factorb=permeability_porosity_model_values[1];
			valuelogk=factora+(factorb*porosity);
			k_rel=0.987e-12*pow(10.,valuelogk);
            CMediumProperties *m_mmp = NULL;
            long group = ElGetElementGroupNumber(number);
			m_mmp = mmp_vector[group];
			double* k_ij;
			k_ij = m_mmp->PermeabilityTensor(number); //permeability;
			k_rel /= k_ij[0];
			break;
	}
	return k_rel;
}

//------------------------------------------------------------------------
//13. CAPILLARY_PRESSURE_FUNCTION
//------------------------------------------------------------------------
/**************************************************************************
FEMLib-Method:
Task: 
Programing:
03/1997 CT Erste Version
09/1998 CT Kurven ueber Schluesselwort #CURVES
08/1999 CT Erweiterung auf n-Phasen begonnen
08/2004 OK Template for MMP implementation
01/2004 OK Mode 3, given saturation
last modification:
ToDo: GetSoilRelPermSatu
**************************************************************************/
double CMediumProperties::CapillaryPressureFunction(long number,double*gp,double theta,\
                                            int phase,double saturation)
{
  double density_fluid = 0.;
  double van_saturation,van_beta;
  CFluidProperties *m_mfp = NULL;
  // only for liquid phase
  //if (phase == 0)  //YD
  //  return 0.;
  static int nidx0,nidx1;
  int gueltig;
  double capillary_pressure=0.0;
  //---------------------------------------------------------------------
  if(mode==2){ // Given value
    saturation = saturation;
  }
  else{
    string pcs_name_this = "SATURATION";
    char phase_char[1];
    sprintf(phase_char,"%i",phase+1);
    pcs_name_this.append(phase_char);
    nidx0 = PCSGetNODValueIndex(pcs_name_this,0);
    nidx1 = PCSGetNODValueIndex(pcs_name_this,1);
    if(mode==0){ // Gauss point values
      saturation = (1.-theta)*InterpolValue(number,nidx0,gp[0],gp[1],gp[2]) \
                 + theta*InterpolValue(number,nidx1,gp[0],gp[1],gp[2]);
    }
    else if(mode==1){ // Node values
     saturation = (1.-theta)*GetNodeVal(number,nidx0) \
                + theta*GetNodeVal(number,nidx1);
    }
  }
  //----------------------------------------------------------------------
  switch(capillary_pressure_model){   
    case 0:  // k = f(x) user-defined function
      capillary_pressure = GetCurveValue((int)capillary_pressure_model_values[0],0,saturation,&gueltig);
      break;
    case 1:  // constant
      capillary_pressure = capillary_pressure;
      break;
    case 2:  // Lineare Kapillardruck-Saettigungs-Beziehung
      // kap12 steigt linear von 0 auf kap[2] im Bereich satu_water_saturated bis satu_water_residual
      break;
    case 3:  // Parabolische Kapillardruck-Saettigungs-Beziehung
      // kap12 steigt parabolisch von 0 auf kap[2] im Bereich satu_water_saturated bis satu_water_residual mit Exponent mat.kap[3] 
      break;
    case 4:  // Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 Page 894 Equation 21
            // YD Advances in Water Resource 19 (1995) 25-38 
      if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;  /* Mehr als Vollsaettigung mit Wasser */
      if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */
      m_mfp = MFPGet("LIQUID");  //YD
      density_fluid = m_mfp->Density();
	  van_beta = 1./(1-saturation_exp[phase]);
	  van_saturation =(saturation - saturation_res[phase])/(saturation_max[phase]-saturation_res[phase]);
	  capillary_pressure =density_fluid*gravity_constant/capillary_pressure_model_values[0]  \
		                  *pow(pow(van_saturation,-1./saturation_exp[phase])-1.,1./van_beta);
      /* 
	  //WW
      if (capillary_pressure > (9.0e4 - MKleinsteZahl))
           capillary_pressure = 9.0e4;  // Mehr als Vollsaettigung mit Wasser 
	  */
      if (capillary_pressure < (0. + MKleinsteZahl))
          capillary_pressure = 0. + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */
        //if (capillary_pressure < 200)
        //cout<<"c"<<capillary_pressure;

      break;
    case 5:  // Haverkamp Problem: aus SOIL SCI. SOC. AM. J., VOL. 41, 1977, Pages 285ff 
      break;
    case 6:  // Brooks-Corey Kurve nach Helmig et al. in Advances in Water Resources 1998 Vol21 No8 pp 697-711
      break;
    case 7:  // Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 Page 894 Equation 21 
      break;
    case 8: // 3Phasen ueber Kurven 
      break;
    default:
      cout << "Error in CFluidProperties::CapillaryPressure: no valid material model" << endl;
      break;
  }
  return capillary_pressure;
}


/**************************************************************************
FEMLib-Method:
Task: 
Programing:
03/2002 CT CECalcSatuFromCap
        SB Extensions case 4 and case 5
02/2005 OK CMediumProperties function
08/2005 WW Remove interploation
Last modified:
**************************************************************************/
double CMediumProperties::SaturationCapillaryPressureFunction
                          (const double capillary_pressure, const int phase)
{
  static double saturation;
  int gueltig;
  double van_beta = 0.0;
  double density_fluid = 0.;
  CFluidProperties *m_mfp = NULL;
  //----------------------------------------------------------------------
  switch(capillary_pressure_model){   
    case 0:  // k = f(x) user-defined function
      saturation = GetCurveValueInverse((int)capillary_pressure_model_values[0],0,capillary_pressure,&gueltig);
      break;
    case 1:  // constant
      break;
    case 2:  // Lineare Kapillardruck-Saettigungs-Beziehung
      // kap12 steigt linear von 0 auf kap[2] im Bereich satu_water_saturated bis satu_water_residual
      break;
    case 3:  // Parabolische Kapillardruck-Saettigungs-Beziehung
      // kap12 steigt parabolisch von 0 auf kap[2] im Bereich satu_water_saturated bis satu_water_residual mit Exponent mat.kap[3] 
      break;
    case 4:  // Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 Page 894 Equation 21
      m_mfp = mfp_vector[phase];
      density_fluid = m_mfp->Density();
	  van_beta = 1/(1-saturation_exp[phase]) ;
	  if(capillary_pressure < MKleinsteZahl)
      saturation = saturation_max[phase];
	  else
   	  saturation = pow((pow(capillary_pressure*capillary_pressure_model_values[0]\
	                     /density_fluid/gravity_constant,van_beta)+1),-1.*saturation_exp[phase]) \
		           *(saturation_max[phase]-saturation_res[phase]) + saturation_res[phase];  
	    if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;  /* Mehr als Vollsaettigung mit Wasser */
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */

/*OK
      a = kap[2];             // Test 0.005
      m = kap[3];             // Test 0.5
      n = kap[4];             // Test 2.
	  saturation = pow(1.0 + pow(a*cap[1],n),-m) * (kap[1] - kap[0]) + kap[0];
*/
      break;
    case 5:  // Haverkamp Problem: aus SOIL SCI. SOC. AM. J., VOL. 41, 1977, Pages 285ff 
/*
      a = kap[2];             // 1.611e6 fuer Haverkamp
      b = kap[3];             // 3.96 fuer Haverkamp
      // SB: Umrechnen von cap in p_cap [cm]
      p_cap = cap[1] / GetFluidDensity(1, element, 0., 0., 0., 0.) / gravity_constant* 100.0;
      satu_return[1] = (a * (kap[1] - kap[0]))/(a + pow(p_cap,b)) + kap[0];
*/
      break;
    case 6:  // Brooks-Corey Kurve nach Helmig et al. in Advances in Water Resources 1998 Vol21 No8 pp 697-711
      break;
    case 7:  // Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 Page 894 Equation 21 
      break;
    case 8: // 3Phasen ueber Kurven 
      break;
    default:
      cout << "Error in CMediumProperties::SaturationCapillaryPressureFunction: no valid material model" << endl;
      break;
  }
  return saturation;
}


/**************************************************************************
FEMLib-Method:
Task: 
Programing:
03/2002 CT CECalcSatuFromCap
        SB Extensions case 4 and case 5
02/2005 OK CMediumProperties function
Last modified:
**************************************************************************/
double CMediumProperties::SaturationCapillaryPressureFunction(long number,double*gp,double theta,int phase)
{
  static int nidx0,nidx1;
  static double saturation;
  int gueltig;
  double capillary_pressure=0.0;
  double van_beta = 0.0;
  double density_fluid = 0.;
  CFluidProperties *m_mfp = NULL;
  //---------------------------------------------------------------------
if(mode==2){
    capillary_pressure = argument;
}
else{
/* WW delete these
  if(m_pcs->m_msh){
    if(gp==NULL){ // NOD value
      nidx0 = GetNodeValueIndex("PRESSURE_CAP");
      nidx1 = GetNodeValueIndex("PRESSURE_CAP")+1;
      capillary_pressure = (1.-theta)*GetNodeValue(number,nidx0) \
                         + theta*GetNodeValue(number,nidx1);
    }
    else // ELE mean value
      capillary_pressure = m_pcs->GetELEValue(number,gp,theta,"PRESSURE_CAP");
  }
  else{
*/
  nidx0 = PCSGetNODValueIndex("PRESSURE_CAP",0);
  nidx1 = PCSGetNODValueIndex("PRESSURE_CAP",1);
  if(mode==0){ // Gauss point values
    capillary_pressure = (1.-theta)*InterpolValue(number,nidx0,gp[0],gp[1],gp[2]) \
                       + theta*InterpolValue(number,nidx1,gp[0],gp[1],gp[2]);
  }
  else{ // Node values
    capillary_pressure = (1.-theta)*GetNodeVal(number,nidx0) \
                       + theta*GetNodeVal(number,nidx1);
  }
 // }
}
  //----------------------------------------------------------------------
  switch(capillary_pressure_model){   
    case 0:  // k = f(x) user-defined function
      saturation = GetCurveValueInverse((int)capillary_pressure_model_values[0],0,capillary_pressure,&gueltig);
      break;
    case 1:  // constant
      break;
    case 2:  // Lineare Kapillardruck-Saettigungs-Beziehung
      // kap12 steigt linear von 0 auf kap[2] im Bereich satu_water_saturated bis satu_water_residual
      break;
    case 3:  // Parabolische Kapillardruck-Saettigungs-Beziehung
      // kap12 steigt parabolisch von 0 auf kap[2] im Bereich satu_water_saturated bis satu_water_residual mit Exponent mat.kap[3] 
      break;
    case 4:  // Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 Page 894 Equation 21
      m_mfp = mfp_vector[phase];
      density_fluid = m_mfp->Density();
	  van_beta = 1/(1-saturation_exp[phase]) ;
	  if(capillary_pressure < MKleinsteZahl)
      saturation = saturation_max[phase];
	  else
   	  saturation = pow((pow(capillary_pressure*capillary_pressure_model_values[0]/density_fluid/gravity_constant,van_beta)+1),-1.*saturation_exp[phase]) \
		           *(saturation_max[phase]-saturation_res[phase]) + saturation_res[phase];  
	    if (saturation > (saturation_max[phase] - MKleinsteZahl))
            saturation = saturation_max[phase] - MKleinsteZahl;  /* Mehr als Vollsaettigung mit Wasser */
        if (saturation < (saturation_res[phase] + MKleinsteZahl))
            saturation = saturation_res[phase] + MKleinsteZahl;   /* Weniger als Residualsaettigung Wasser */

/*OK
      a = kap[2];             // Test 0.005
      m = kap[3];             // Test 0.5
      n = kap[4];             // Test 2.
	  saturation = pow(1.0 + pow(a*cap[1],n),-m) * (kap[1] - kap[0]) + kap[0];
*/
      break;
    case 5:  // Haverkamp Problem: aus SOIL SCI. SOC. AM. J., VOL. 41, 1977, Pages 285ff 
/*
      a = kap[2];             // 1.611e6 fuer Haverkamp
      b = kap[3];             // 3.96 fuer Haverkamp
      // SB: Umrechnen von cap in p_cap [cm]
      p_cap = cap[1] / GetFluidDensity(1, element, 0., 0., 0., 0.) / gravity_constant* 100.0;
      satu_return[1] = (a * (kap[1] - kap[0]))/(a + pow(p_cap,b)) + kap[0];
*/
      break;
    case 6:  // Brooks-Corey Kurve nach Helmig et al. in Advances in Water Resources 1998 Vol21 No8 pp 697-711
      break;
    case 7:  // Van Genuchten: Wasser/Gas aus SOIL SIC. SOC. AM. J. VOL. 44, 1980 Page 894 Equation 21 
      break;
    case 8: // 3Phasen ueber Kurven 
      break;
    default:
      cout << "Error in CMediumProperties::SaturationCapillaryPressureFunction: no valid material model" << endl;
      break;
  }
  return saturation;
}


/**************************************************************************
FEMLib-Method:
Task: 
Programing:
08/1999 CT Erste Version (MMTM0699GetSaturationPressureDependency)
05/2001 OK Verallgemeinerung
02/2005 OK CMediumProperties function
08/2005 WW Remove interpolation
Last modified:
**************************************************************************/
double CMediumProperties::SaturationPressureDependency(const double saturation,double theta)
//(int phase, long index, double r, double s, double t, double theta)
{
  static double capillary_pressure,capillary_pressure1,capillary_pressure2;
  static double saturation1,saturation2;
  static double dS,dS_dp,dpc;
  int phase = (int)mfp_vector.size()-1; 
  //----------------------------------------------------------------------
  // Vollsaettigung? 
  mode = 2;
  phase = (int)mfp_vector.size()-1; 
  capillary_pressure = CapillaryPressureFunction(number,NULL,theta,phase, saturation);
  if(capillary_pressure < MKleinsteZahl)
    return 0.;
  //----------------------------------------------------------------------
 // Wenn wir nah an der Vollsaettigung, ggf. Schrittweite verkleinern 
  dS = 1.e-2;
  do{
    dS /= 10.;
    saturation1 = saturation - dS;
    capillary_pressure1 = CapillaryPressureFunction(number,NULL,theta,phase,saturation1);
    saturation2 = saturation + dS;
    capillary_pressure2 = CapillaryPressureFunction(number,NULL,theta,phase,saturation2);
    dpc = capillary_pressure1 - capillary_pressure2; //OK4105
  }
  while((dS > MKleinsteZahl) && (capillary_pressure2 < MKleinsteZahl / 100.));
  if ( ((capillary_pressure1 > MKleinsteZahl)||(capillary_pressure2 > MKleinsteZahl)) \
      && (dpc > MKleinsteZahl) )
    dS_dp = 2. * dS / (capillary_pressure1 - capillary_pressure2);
  else{
    dS_dp = 0.;
    //cout << "Warning in CMediumProperties::SaturationPressureDependency: dS_dp = 0" << endl; //OK4105
  }
  //----------------------------------------------------------------------
  mode = 0;
  return dS_dp;
}



/**************************************************************************
FEMLib-Method:
Task: 
Programing:
08/1999 CT Erste Version (MMTM0699GetSaturationPressureDependency)
05/2001 OK Verallgemeinerung
02/2005 OK CMediumProperties function
Last modified:
**************************************************************************/
double CMediumProperties::SaturationPressureDependency(long number,double*gp,double theta)
//(int phase, long index, double r, double s, double t, double theta)
{
  static double capillary_pressure,capillary_pressure1,capillary_pressure2;
  static double saturation,saturation1,saturation2;
  static double dS,dS_dp,dpc;
  int nidx0,nidx1;
  int phase = (int)mfp_vector.size()-1;
  //----------------------------------------------------------------------
if(mode==2){
    saturation = argument;
}
else{
  /* //WW Delete these
  if(m_pcs->m_msh){
    nidx0 = GetNodeValueIndex("SATURATION1");
    nidx1 = GetNodeValueIndex("SATURATION1") + 1;
    if(mode==0){ // Gauss point values
      saturation = (1.-theta)*m_pcs->InterpolateNODValue(number,nidx0,gp) \
                 + theta*m_pcs->InterpolateNODValue(number,nidx1,gp);

    }
    else{ // Node values
      //saturation = SaturationCapillaryPressureFunction(number,NULL,theta);
      saturation = (1.-theta)*GetNodeValue(number,nidx0) \
                 + theta*GetNodeValue(number,nidx1);
    }
    saturation = m_pcs->GetELEValue(number,gp,theta,"SATURATION1");
  }
  */
 // else{
    nidx0 = PCSGetNODValueIndex("SATURATION1",0);
    nidx1 = PCSGetNODValueIndex("SATURATION1",1);
    if(mode==0){ // Gauss point values
      saturation = (1.-theta)*InterpolValue(number,nidx0,gp[0],gp[1],gp[2]) \
               + theta*InterpolValue(number,nidx1,gp[0],gp[1],gp[2]);
    }
    else{ // Node values
    saturation = SaturationCapillaryPressureFunction(number,NULL,theta,phase);
    saturation = (1.-theta)*GetNodeVal(number,nidx0) \
               + theta*GetNodeVal(number,nidx1);
    }
 // }
}
  //----------------------------------------------------------------------
  // Vollsaettigung? 
  mode = 2;
  capillary_pressure = CapillaryPressureFunction(number,NULL,theta,phase,saturation);
  if(capillary_pressure < MKleinsteZahl)
    return 0.;
  //----------------------------------------------------------------------
 // Wenn wir nah an der Vollsaettigung, ggf. Schrittweite verkleinern 
  dS = 1.e-2;
  do{
    dS /= 10.;
    saturation1 = saturation - dS;
    capillary_pressure1 = CapillaryPressureFunction(number,NULL,theta,phase,saturation1);
    saturation2 = saturation + dS;
    capillary_pressure2 = CapillaryPressureFunction(number,NULL,theta,phase,saturation2);
    dpc = capillary_pressure1 - capillary_pressure2; //OK4105
  }
  while((dS > MKleinsteZahl) && (capillary_pressure2 < MKleinsteZahl / 100.));
  if ( ((capillary_pressure1 > MKleinsteZahl)||(capillary_pressure2 > MKleinsteZahl)) \
      && (dpc > MKleinsteZahl) )
    dS_dp = 2. * dS / (capillary_pressure1 - capillary_pressure2);
  else{
    dS_dp = 0.;
    //cout << "Warning in CMediumProperties::SaturationPressureDependency: dS_dp = 0" << endl; //OK4105
  }
  //----------------------------------------------------------------------
  mode = 0;
  return dS_dp;
}

/*************************************************************************************************************
14.$MASSDISPERSION
************************************************************************************************************/

/*************************************************************************************************************
Density
************************************************************************************************************/
/**************************************************************************
FEMLib-Method: 
Task: 
Programing:
11/2004 OK Implementation
**************************************************************************/
double CMediumProperties::Density(long element,double*gp,double theta) 
{
  int no_phases = (int)mfp_vector.size();
  double density = 0.0;
  int i;
  CFluidProperties* m_mfp = NULL;
  CSolidProperties* m_msp = NULL;
  char saturation_name[15];
  if(no_phases==1){
    m_mfp = mfp_vector[0];
    density = Porosity(element,gp,theta) * m_mfp->Density();
  }
  else{
    for(i=0;i<no_phases;i++){
      m_mfp = mfp_vector[i];
      sprintf(saturation_name,"SATURATION%i",i+1);
      density += Porosity(element,gp,theta) * m_mfp->Density() * PCSGetELEValue(element,gp,theta,saturation_name);
    }
  }
  long group = ElGetElementGroupNumber(element);
  m_msp = msp_vector[group];
  density += (1.-Porosity(element,gp,theta))*fabs(m_msp->Density());
  return density;
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
01/2005 OK Implementation
last modified:
**************************************************************************/
void MMPDelete()
{
  long i;
  int no_mmp =(int)mmp_vector.size();
  for(i=0;i<no_mmp;i++){
    delete mmp_vector[i];
  }
  mmp_vector.clear();
}

/**************************************************************************
FEMLib-Method:
Task:
Programing:
06/2005 OK Implementation
07/2005 WW Change due to geometry element object applied
last modified:
**************************************************************************/
void MMP2PCSRelation(CRFProcess*m_pcs)
{
  CMediumProperties*m_mmp = NULL;
  Mesh_Group::CElem* m_ele = NULL;
  if(m_pcs->m_msh){
	  for(long i=0;i<(long)m_pcs->m_msh->ele_vector.size();i++){
      m_ele = m_pcs->m_msh->ele_vector[i];
	  if(m_ele->GetPatchIndex()<(int)mmp_vector.size()){
        m_mmp = mmp_vector[m_ele->GetPatchIndex()];
        m_mmp->m_pcs = m_pcs;
      }
    }
  }
  else{
    for(int j=0;j<(int)mmp_vector.size();j++){
      m_mmp = mmp_vector[j];
      m_mmp->m_pcs = m_pcs;
    }
  }
}
/**************************************************************************
PCSLib-Function
Liest zu jedem Knoten einen Wert der Permeabilität ein.
Identifikation über Koordinaten
Programing:
10/2003     SB  First Version
01/2004     SB  2. Version
01/2005 OK Check MAT groups //OK41
06/2005 MB msh, loop over mmp groups
09/2005 MB EleClass
//SB/MB ? member function of CFEMesh / CMediumProperties
11/2005 OK GEOMETRY_AREA
**************************************************************************/
//MMPGetHeterogeneousFields
void GetHeterogeneousFields()
{
  int ok=0;
  char* name_file=NULL;
  CMediumProperties *m_mmp = NULL;
  //----------------------------------------------------------------------
  // File handling
  string file_path;
  string file_path_base_ext;
  CGSProject* m_gsp = NULL;
  m_gsp = GSPGetMember("mmp");
  if(m_gsp)
    file_path = m_gsp->path;
  //----------------------------------------------------------------------
  // Tests
  if(mmp_vector.size()==0)
    return;
  //----------------------------------------------------------------------
  //Schleife über alle Gruppen
  for(int i=0;i<(int)mmp_vector.size();i++){
    m_mmp = mmp_vector[i];
    //....................................................................
    // For Permeability
    if(m_mmp->permeability_file.size()>0){
      //OK name_file = (char *) m_mmp->permeability_file.data();
      //OK if(name_file != NULL)
        //OK ok = FctReadHeterogeneousFields(name_file,m_mmp);
      file_path_base_ext = file_path + m_mmp->permeability_file;
      m_mmp->SetDistributedELEProperties(file_path_base_ext);
      m_mmp->WriteTecplotDistributedProperties();
    }
    //....................................................................
    // For Porosity
    if(m_mmp->porosity_file.size()>0){
      name_file = (char *) m_mmp->porosity_file.data();
      if(name_file != NULL)
        ok = FctReadHeterogeneousFields(name_file,m_mmp);
    } 
    //....................................................................
    // GEOMETRY_AREA
    if(m_mmp->geo_area_file.size()>0){
      file_path_base_ext = file_path + m_mmp->geo_area_file;
      m_mmp->SetDistributedELEProperties(file_path_base_ext);
      m_mmp->WriteTecplotDistributedProperties();
    } 
    //....................................................................
  } 
  //----------------------------------------------------------------------
}

/**************************************************************************
PCSLib-Method: 
Programing:
11/2005 OK Implementation
**************************************************************************/
void CMediumProperties::SetDistributedELEProperties(string file_name) 
{
  string line_string;
  string mmp_property_name;
  string mmp_property_dis_type;
  CElem* m_ele_geo = NULL;
  long i;
  double mmp_property_value;
  int mat_vector_size;
  double ddummy;
  //----------------------------------------------------------------------
  // File handling
  ifstream mmp_property_file(file_name.data(),ios::in);
  if(!mmp_property_file.good()){
    cout << "Warning in CMediumProperties::SetDistributedELEProperties: no MMP property data" << endl;
    return;
  }
  mmp_property_file.clear();
  mmp_property_file.seekg(0,ios::beg); 
  //----------------------------------------------------------------------
  line_string = GetLineFromFile1(&mmp_property_file);
  if(!(line_string.find("#MEDIUM_PROPERTIES_DISTRIBUTED")!=string::npos)){
    cout << "Keyword #MEDIUM_PROPERTIES_DISTRIBUTED not found" << endl;
    return;
  }
  //----------------------------------------------------------------------
  while(!mmp_property_file.eof()) {
    line_string = GetLineFromFile1(&mmp_property_file);
    if(line_string.find("STOP")!=string::npos)
      return;
    if(line_string.empty()){
      cout << "Error in CMediumProperties::SetDistributedELEProperties - no enough data sets" << endl;
      return;
    }
    //....................................................................
    if(line_string.find("$MSH_TYPE")!=string::npos){
      line_string = GetLineFromFile1(&mmp_property_file);
      m_msh = FEMGet(line_string);
      if(!m_msh){
        cout << "CMediumProperties::SetDistributedELEProperties: no MSH data" << endl;
        return;
      }
      continue;
    }
    //....................................................................
    if(line_string.find("$MMP_TYPE")!=string::npos){
      mmp_property_file >> mmp_property_name;
      if(!m_msh){
        cout << "CMediumProperties::SetDistributedELEProperties: no MSH data" << endl;
        return;
      }
      m_msh->mat_names_vector.push_back(mmp_property_name);
      continue;
    }
    //....................................................................
    if(line_string.find("$DIS_TYPE")!=string::npos){
      mmp_property_file >> mmp_property_dis_type;
      continue;
    }
    //....................................................................
    if(line_string.find("$DATA")!=string::npos){
      switch(mmp_property_dis_type[0]){
        case 'I': // Raster data for interpolation
          break;
        case 'E': // Element data
          for(i=0;i<(long)m_msh->ele_vector.size();i++){
            m_ele_geo = m_msh->ele_vector[i];
            mmp_property_file >> ddummy >> mmp_property_value;
            mat_vector_size = m_ele_geo->mat_vector.Size();
            m_ele_geo->mat_vector.resize(mat_vector_size+1);
            m_ele_geo->mat_vector(mat_vector_size) = mmp_property_value;
            if(line_string.empty()){
              cout << "Error in CMediumProperties::SetDistributedELEProperties - no enough data sets" << endl;
              return;
            }
          }
          break;
      }
      continue;
    }
    //....................................................................
  }
  //----------------------------------------------------------------------
}

/**************************************************************************
PCSLib-Method: 
Programing:
11/2005 OK Implementation
**************************************************************************/
void CMediumProperties::WriteTecplotDistributedProperties()
{
  int j,k;
  long i;
  string element_type;
  string m_string = "MAT";
  double m_mat_prop_nod;
  //----------------------------------------------------------------------
  // Path
  string path;
  CGSProject* m_gsp = NULL;
  m_gsp = GSPGetMember("msh");
  if(m_gsp){
    path = m_gsp->path;
  }
  //--------------------------------------------------------------------
  // MSH
  CNode* m_nod = NULL;
  CElem* m_ele = NULL;
  if(!m_msh)
    return;
  //--------------------------------------------------------------------
  // File handling
  string mat_file_name = path + name + "_" + m_msh->pcs_name + "_PROPERTIES" + TEC_FILE_EXTENSION;
  fstream mat_file (mat_file_name.data(),ios::trunc|ios::out);
  mat_file.setf(ios::scientific,ios::floatfield);
  mat_file.precision(12);
  if (!mat_file.good()) return;
  mat_file.seekg(0L,ios::beg);
  //--------------------------------------------------------------------
  if((long)m_msh->ele_vector.size()>0){
    m_ele = m_msh->ele_vector[0];
    switch(m_ele->GetElementType()){
      case 1:
        element_type = "QUADRILATERAL";
        break;
      case 2:
        element_type = "QUADRILATERAL";
        break;
      case 3:
        element_type = "BRICK";
        break;
      case 4:
        element_type = "TRIANGLE";
        break;
      case 5:
        element_type = "TETRAHEDRON";
        break;
      case 6:
        element_type = "BRICK";
        break;
    }
  }
  //--------------------------------------------------------------------
  // Header
  mat_file << "VARIABLES = X,Y,Z";
  for(j=0;j<(int)m_msh->mat_names_vector.size();j++){
    mat_file << "," << m_msh->mat_names_vector[j];
  }
  mat_file << endl;
  mat_file << "ZONE T = " << name << ", " \
           << "N = " << (long)m_msh->nod_vector.size() << ", " \
           << "E = " << (long)m_msh->ele_vector.size() << ", " \
           << "F = FEPOINT" << ", " << "ET = " << element_type << endl;
  //--------------------------------------------------------------------
  // Nodes
  for(i=0;i<(long)m_msh->nod_vector.size();i++) {
    m_nod = m_msh->nod_vector[i];
    mat_file << m_nod->X() << " " << m_nod->Y() << " " << m_nod->Z();
    for(j=0;j<(int)m_msh->mat_names_vector.size();j++){
      m_mat_prop_nod = 0.0;
      for(k=0;k<(int)m_nod->connected_elements.size();k++){
        m_ele = m_msh->ele_vector[m_nod->connected_elements[k]];
        m_mat_prop_nod += m_ele->mat_vector(j);
      }
      m_mat_prop_nod /= (int)m_nod->connected_elements.size();
      mat_file << " " << m_mat_prop_nod;
    }
    mat_file << endl;
  }
  //--------------------------------------------------------------------
  // Elements
  for(i=0;i<(long)m_msh->ele_vector.size();i++){
    m_ele = m_msh->ele_vector[i];
    //OK if(m_ele->GetPatchIndex()==number) {
      switch(m_ele->GetElementType()){
        case 1:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[0]+1 << endl;
            element_type = "QUADRILATERAL";
            break;
          case 2:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << " " << m_ele->nodes_index[3]+1 << endl;
            element_type = "QUADRILATERAL";
            break;
          case 3:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << " " << m_ele->nodes_index[3]+1 << " " \
              << m_ele->nodes_index[4]+1 << " " << m_ele->nodes_index[5]+1 << " " << m_ele->nodes_index[6]+1 << " " << m_ele->nodes_index[7]+1 << endl;
            element_type = "BRICK";
            break;
          case 4:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << endl;
            element_type = "TRIANGLE";
            break;
          case 5:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << " " << m_ele->nodes_index[3]+1 << endl;
            element_type = "TETRAHEDRON";
            break;
          case 6:
            mat_file \
              << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[0]+1 << " " << m_ele->nodes_index[1]+1 << " " << m_ele->nodes_index[2]+1 << " " \
              << m_ele->nodes_index[3]+1 << " " << m_ele->nodes_index[3]+1 << " " << m_ele->nodes_index[4]+1 << " " << m_ele->nodes_index[5]+1 << endl;
          element_type = "BRICK";
          break;
      }
    //OK}
  }
  //--------------------------------------------------------------------
}
