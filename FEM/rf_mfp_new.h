/**************************************************************************
   FEMLib - Object: MAT-FP
   Task: class implementation
   Programing:
   08/2004 OK Implementation
   last modified:
**************************************************************************/
#ifndef rf_mfp_new_INC
#define rf_mfp_new_INC

#include <iostream>
#include <string>
#include <vector>
//#include "rfmat_cp.h"//AKS

class CompProperties;
class CRFProcess;

/*!
   \class hash_table

   WW 07.2011
 */
#ifdef MFP_TEST
class Hash_Table
{
public:
	Hash_Table(std::string f_name);
	~Hash_Table();

	double CalcValue(double* var, const int var_id) const;

private:
	///Hash. Test. 08.2011. WW
	int num_par;
	int num_var;

	vector<std::string> names;
	vector<double> hash_row_par;
	vector<int> table_section_ends;
	vector<double*> hash_table_data;
};
#endif
/*---------------------------------------------------------------*/
//namespace FiniteElement {class CElement;}
//using FiniteElement::CElement;
namespace FiniteElement {
	class CFiniteElementStd;
}

using FiniteElement::CFiniteElementStd;

class CFluidProperties
{
	friend bool MFPRead(std::string);

public:
	double getCriticalDensity() const { return rhoc; }
	double getCriticalTemperature() const { return Tc; }
	double getCriticalPressure() const { return pc; }
	double getSpecificGasConstant() const { return Rs; }
	double getMolarMass() const { return molar_mass; }
	double getUniversalGasConstant() const { return Ru; }
	/**
	 * get the acentric factor for Peng-Robinson EOS (omega)
	 * @return omega
	 */
	double getAzentricFactor() const { return omega; }
	double getDiffusion() const { return diffusion; }
	double getSpecificHeatCapacity() const { return specific_heat_capacity; }

	/**
	 * get the reference temperature
	 * @return
	 */
	double getReferenceTemperature() const { return T_0; }

private:
	double rhoc;                          //critical_density; //NB
	double Tc;                            //critical_temperature;
	double pc;                            //critical_pressure;
	double Tt;                            //triple_point_temperature;
	double pt;                            //triple_point_pressure;
	double Rs;                            //specific_gas_constant;
	double Ru;                            //universal_gas_constant;
	double omega;                         // azentric factor for Peng-Robinson EOS
	double molar_mass;
	double Vd, Zc, n0, m0, a, b, k1, k2, k3;//constants are used in VTPR
	/**
	 * density
	 */
	double rho_0;
	/**
	 * density deviated with respect to the pressure
	 */
	double drho_dp;
	/**
	 * density deviated with respect to the temperature
	 */
	double drho_dT;
	/**
	 * density deviated with respect to the concentration
	 */
	double drho_dC;

	double diffusion; /*SB:2p */
	double diffusion_coef; //AKS
	// Viscosity
	double viscosity;
	double my_0;
	double dmy_dp;
	double dmy_dT;
	double dmy_dC;
	//Multi_componential flow
	   int decay_model, isotherm_model;
	double rho[4], mu[4], cp[4], kappa[4], lambda[4], Kd[4], D0[4], alpha_T[4], beta_p[4];
	std::string eos_name, mu_JT;
	// Thermal properties
	double specific_heat_capacity;
	double heat_conductivity;
	double temperature_buffer;            //YD, shifted to public JOD

	// State variables
	double p_0;
	/**
	 * state variable: reference temperature
	 */
	double T_0;
	double C_0;
	double Z;

	// Chemical properties
	double T_Latent1, T_Latent2, latent_heat;

public:
	int fluid_id;                         // specification of substance (NB JUN 09)
	std::string name;
	std::string cmpNm1, cmpNm2, cmpNm3, cmpNm4; // component name 
	int cmpN; //components number
private:
	std::string fluid_name;               //NB4801
	// compressibility
	int compressibility_model_pressure;   //NB
	int compressibility_model_temperature; //NB
		int solutal_expansivity_model; //NB
	double compressibility_pressure;         //NB
	double compressibility_temperature;      //NB
		double solutal_expansivity; //AKS

	int phase;

private:
	// Limits and coefficients for free Helmholtz Energy, NB JUN 09
	int limit[5];
	double k [2][8];
	double K [14][56], KP[4];

public:
	// FEM
	CFiniteElementStd* Fem_Ele_Std;
	long node;                            //OK4704
	// Density
	int density_model;

	// TF 11/2011 - used only in read- and write-method
//	std::string _rho_fct_name;

	// Viscosity
	int viscosity_model;
	// TF 11/2011 - used only in read- and write-method
//	std::string _my_fct_name;
	// Thermal properties
	// TF 11/2011 - used only in read- and write-method
//	std::string heat_capacity_fct_name;
	int heat_conductivity_model;
	// TF 11/2011 - used only in read- and write-method
//	std::string heat_conductivity_fct_name;
	int heat_diffusion_model;             //AKS
	int heat_capacity_model;              //YD, shifted to public JOD
	// Electrical properties
	int electric_conductivity_model;
	int electric_conductivity_num_val;
	double* electric_conductivity_val;
	//
	// Chemical properties
	// TF 11/2011 - only in read-method used
//	std::string dif_fct_name;
	int diffusion_model;                  /* SB:p2 */

	int heat_phase_change_curve;
	// IO
	int mode;
	// PCS  YD
	std::vector<std::string>density_pcs_name_vector;
	std::vector<std::string>viscosity_pcs_name_vector;
	std::vector<std::string>specific_heat_capacity_pcs_name_vector;
	std::vector<std::string>heat_conductivity_pcs_name_vector;
	std::vector<std::string>enthalpy_pcs_name_vector;
	//AKS
	std::vector<CompProperties*>component_vector;

	// FCT
	// TF 11/2011 - not used
//	std::string fct_pcs_name;
	// TF 11/2011 - not used
//	std::string fct_name;

public:
	CFluidProperties(void);
	~CFluidProperties(void);
	std::ios::pos_type Read(std::ifstream*);
	void Write(std::ofstream*) const;
	void CalPrimaryVariable(std::vector<std::string>& pcs_name_vector);
	// Add an argument: double* variables = NULL. 28.05.2008 WW
	double Density(double* variables = NULL);
	double GetElementValueFromNodes(long ElementIndex,
	                                int GPIndex,
	                                int PhaseIndex,
	                                int Variable);  
    double drhodP(double* variables = NULL);
	double drhodT(double* variables = NULL);
	double drhodX(int CIndex, double* variables = NULL);
	double ComponentDensity(int CIndex, double* variables = NULL);//AKS
	double Viscosity(double* variables = NULL); //OK4709
	                                            //NB Jan09
	double SpecificHeatCapacity(double* variables = NULL);
	void therm_prop(std::string caption); //NB 4.9.05
	double PhaseChange();                 // JOD
	double HeatConductivity(double* variables = NULL);
	double CalcEnthalpy(double temperature);
	//WW double Enthalpy(int,double);
	//WW double EnthalpyPhase(long,int,double*,double);
	//WW double MassFraction(long number,int comp,double*gp,double theta, CFiniteElementStd* assem=NULL);
	//WW double InternalEnergy(long number,double*gp,double theta);
	//WW double DensityTemperatureDependence(long number,int comp,double*gp,double theta);
	double vaporDensity(const double T);  //WW
	                                      //WW
	double vaporDensity_derivative(const double T);
	bool CheckGravityCalculation() const {return cal_gravity; }
	int GetHeatCapacityModel() const      //YD
	{
		return heat_capacity_model;
	}
	// Derivations of free Helmholtz energy, NB JUN 09
	double phi_r_d (double rho, double T) const;
	double phi_r_tt (double rho, double T) const;
	double phi_0_t (double T) const;
	double phi_r_t (double rho, double T) const;
	double phi_r_dt (double rho, double T) const;
	double phi_r_dd (double rho, double T) const;
	double phi_0_tt (double T) const;
	double EffectiveDiffusionCoef(int CIndex, double* variables = NULL); //AKS

private:
	// State variables
	double primary_variable[10];          //WW
	double primary_variable_t0[10];       //CMCD
	double primary_variable_t1[10];       //CMCD
	bool cal_gravity;                     //YD/WW
	// FEM
	//WW
	friend class FiniteElement::CFiniteElementStd;


#ifdef MFP_TEST
	Hash_Table* scatter_data; //WW
#endif

	double GasViscosity_Reichenberg_1971(double,double);
	//AKS
	double MATCalcFluidDensityMethod8(double p, double T, double C);
	double LiquidViscosity_Yaws_1976(double);
	double LiquidViscosity_Marsily_1986(double);
	double LiquidViscosity_NN(double,double);
	double LiquidViscosity_CMCD(double p,double T,double C);
	double MATCalcHeatConductivityMethod2(double p, double T, double C);
	double MATCalcFluidHeatCapacityMethod2(double p, double T, double C);
};

extern std::vector<CFluidProperties*>mfp_vector;
extern bool MFPRead(std::string);
extern void MFPWrite(std::string);
#define MFP_FILE_EXTENSION ".mfp"
//WW extern double MFPCalcVapourPressure(double);
//WW
extern double MFPCalcFluidsHeatCapacity(CFiniteElementStd* assem = NULL);
extern double MFPCalcFluidsHeatConductivity(long index,
                                            double* gp,
                                            double theta,
                                            CFiniteElementStd* assem = NULL);
extern void MFPDelete();
//OK/YD
extern CFluidProperties* MFPGet(const std::string&);
extern CFluidProperties* MFPGet(int);             //NB JUN 09
                                                  //NB AUG 09
double MFPGetNodeValue(long,const std::string&,int);
#endif
