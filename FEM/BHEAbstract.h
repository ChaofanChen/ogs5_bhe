/**
* \file BHEAbstract.h
* 2014/06/04 HS inital implementation
* borehole heat exchanger abstract class
*
* 1) Diersch_2011_CG
* Two very important references to understand this class implementations are: 
* H.-J.G. Diersch, D. Bauer, W. Heidemann, W. R�haak, P. Sch�tzl, 
* Finite element modeling of borehole heat exchanger systems: 
* Part 1. Fundamentals, Computers & Geosciences, 
* Volume 37, Issue 8, August 2011, Pages 1122-1135, ISSN 0098-3004, 
* http://dx.doi.org/10.1016/j.cageo.2010.08.003.
*
* 2) FEFLOW_2014_Springer
* FEFLOW: Finite Element Modeling of Flow, Mass and Heat Transport in Porous and Fractured Media
* Diersch, Hans-Joerg, 2014, XXXV, 996 p, Springer. 
* 
*/

#ifndef BHE_ABSTRACT_H
#define BHE_ABSTRACT_H

#include <iostream>
#include "Eigen/Eigen"
#include "../GEO/Polyline.h"

namespace BHE  // namespace of borehole heat exchanger
{
	/**
	  * list the types of borehole heat exchanger
	  */
	enum BHE_TYPE {
		BHE_TYPE_2U,   // two u-tube borehole heat exchanger
		BHE_TYPE_1U,   // one u-tube borehole heat exchanger
		BHE_TYPE_CXC,  // coaxial pipe with annualar inlet
		BHE_TYPE_CXA	  // coaxial pipe with centreed inlet
	};

	/**
	  * discharge type of the 2U BHE
	  */
	enum BHE_DISCHARGE_TYPE {
		BHE_DISCHARGE_TYPE_PARALLEL,   // parallel discharge
		BHE_DISCHARGE_TYPE_SERIAL	   // serial discharge
	};

	class BHEAbstract
	{
	public:
		/**
		  * constructor
		  */
		BHEAbstract(BHE_TYPE my_type, const std::string name) 
            : type(my_type), _name(name)
		{};

		/**
		  * destructor
		  */
		virtual ~BHEAbstract() {}; 

		/**
		  * return the number of unknowns needed for this BHE
		  * abstract function, need to be realized. 
		  */
		virtual std::size_t get_n_unknowns() = 0;

		/**
		  * return the type of the BHE
		  */
		BHE_TYPE get_type() { return type; };

        /**
          * return the name of the BHE
          */
        const std::string get_name() { return _name;  };

		/**
		  * return the thermal resistance for the inlet pipline
		  * idx is the index, when 2U case, 
		  * 0 - the first u-tube
		  * 1 - the second u-tube
		  * needs to be overwritten
		  */
		virtual double get_thermal_resistance_fig(std::size_t idx = 0) = 0;

		/**
		  * return the thermal resistance for the outlet pipline
		  * idx is the index, when 2U case,
		  * 0 - the first u-tube
		  * 1 - the second u-tube
		  * needs to be overwritten
		  */
		virtual double get_thermal_resistance_fog(std::size_t idx = 0) = 0;

		/**
		  * return the thermal resistance
		  */
		virtual double get_thermal_resistance(std::size_t idx = 0) = 0;

		/**
		  * initialization calcultion,
		  * need to be overwritten.
		  */
		virtual void initialize()
		{
			calc_u();
			calc_Re();
			calc_Pr();
			calc_Nu();
			calc_thermal_resistances();
			calc_heat_transfer_coefficients();
		};

		/**
		  * thermal resistance calculation, 
		  * need to be overwritten. 
		  */
		virtual void calc_thermal_resistances() = 0; 

		/**
		  * Nusselt number calculation,
		  * need to be overwritten.
		  */
		virtual void calc_Nu() = 0;

		/**
		  * Renolds number calculation,
		  * need to be overwritten.
		  */
		virtual void calc_Re() = 0;

		/**
		  * Prandtl number calculation,
		  * need to be overwritten.
		  */
		virtual void calc_Pr() = 0;

		/**
		  * flow velocity inside the pipeline
		  * need to be overwritten.
		  */
		virtual void calc_u() = 0;

		/**
		  * heat transfer coefficient,
		  * need to be overwritten.
		  */
		virtual void calc_heat_transfer_coefficients() = 0;

        /**
          * return the coeff of mass matrix, 
          * depending on the index of unknown. 
          */
        virtual double get_mass_coeff(std::size_t idx_unknown) = 0; 

        /**
          * return the coeff of laplace matrix,
          * depending on the index of unknown.
          */
        virtual double get_laplace_coeff(std::size_t idx_unknown) = 0;

        /**
          * return the coeff of advection matrix,
          * depending on the index of unknown.
          */
        virtual double get_advection_coeff(std::size_t idx_unknown) = 0;

        /**
          * get the polyline geometry 
          * that is representing this BHE. 
          */
        const GEOLIB::Polyline* get_geo_ply() { return _geo_ply; }

        /**
          * set the polyline geometry
          * that is representing this BHE.
          */
        void set_geo_ply(const GEOLIB::Polyline* ply) { _geo_ply = ply; }

		/**
		  * total refrigerant flow discharge of BHE
		  * unit is m^3/sec 
		  */
		double Q_r; 

		/**
		  * radius of the pipline inner side
		  * unit is m
		  */
		double r_inner;

		/**
		  * radius of the pipline outer side
		  * unit is m
		  */
		double r_outer;

		/**
		  * pipe-in wall thickness
		  * unit is m
		  */
		double b_in;

		/**
		  * pipe-out wall thickness
		  * unit is m
		  */
		double b_out;

		/**
		  * dynamics viscosity of the refrigerant
		  * unit is kg m-1 sec-1
		  */
		double mu_r;

		/**
		  * density of the refrigerant
		  * unit is kg m-3
		  */
		double rho_r;

		/**
		  * longitudinal dispersivity of the
		  * referigerant flow in the pipeline
		  */
		double alpha_L;

        /**
          * density of the grout
          * unit is kg m-3
          */
        double rho_g;

		/**
		  * porosity of the grout
		  * unit is [-]
		  */
		double porosity_g;

		/**
		  * specific heat capacity of the refrigerant
		  * unit is m^2 sec^-2 K^-1
		  */
		double heat_cap_r;

        /**
          * specific heat capacity of the grout
          * unit is m^2 sec^-2 K^-1
          */
        double heat_cap_g;

		/**
		  * thermal conductivity of the refrigerant
		  * unit is kg m sec^-3 K^-1
		  */
		double lambda_r;

		/**
		  * thermal conductivity of the pipe wall
		  * unit is kg m sec^-3 K^-1
		  */
		double lambda_p; 

		/**
		  * thermal conductivity of the grout
		  * unit is kg m sec^-3 K^-1
		  */
		double lambda_g;

		/**
		  * length/depth of the BHE
		  * unit is m
		  */
		double L; 

		/**
		  * diameter of the BHE
		  * unit is m
		  */
		double D;
	private:

		/**
		  * the type of the BHE
		  */
		const BHE_TYPE type;

        /**
          * the polyline geometry representing the BHE
          */
        const GEOLIB::Polyline* _geo_ply;

        /**
          * name of the borehole heat exchanger
          */
        const std::string _name;

	};

}  // end of namespace
#endif